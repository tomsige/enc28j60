#include "enc28j60.h"
#include "ip_arp_udp_tcp.h"
#include "net.h"
#include "server.h"
#include <stdio.h>
#include <string.h>

//const uint LED_PIN = 25;

static uint8_t payloadlen=0;
static uint8_t cmd_pos=0;
static uint8_t cmdval;
static int8_t cmd;
static uint16_t plen, dat_p;
static uint8_t str[30];
static uint8_t buf[BUFFER_SIZE+1];


void enc28j60_interrupt_handler(void)
{
	/* LED3 On */
	//GPIO_SetBits(GPIOC, GPIO_Pin_9); 	// zmenime stav  LED3 - zelena
    gpio_put(LED_PIN, 1);    
	// provadej dokud EPKTCNT neni roven nule (tj. vsechny pakety jsou zpracovany)
	while (enc28j60_rcr(EPKTCNT)>0)
	{
        plen = enc28j60_recv_packet(buf, BUFFER_SIZE);
        /* plen < 40 is a not valid packet */
        if(plen >= 40)
        {
            // check arp broadcast for us
            if(eth_type_is_arp_and_my_ip(buf,plen))
           	{
    			make_arp_answer_from_request(buf);
                printf("ARP packet.\n");
    	   	}
            // check if ip packets are for us:
            else if(eth_type_is_ip_and_my_ip(buf,plen)!=0)
            {
                // check ICMP ping packets are for us:
                if(buf[IP_PROTO_P]==IP_PROTO_ICMP_V && buf[ICMP_TYPE_P]==ICMP_TYPE_ECHOREQUEST_V)
                {
        			make_echo_reply_from_request(buf, plen);	// send pong
                    printf("Ping-pong.\n");
               	}
                // tcp port www start, compare only the lower byte
               	else if (buf[IP_PROTO_P]==IP_PROTO_TCP_V&&buf[TCP_DST_PORT_H_P]==0&&buf[TCP_DST_PORT_L_P]==mywwwport)
        		{	// ano, jde o TCP paket a smeruje na nas www port
                    printf("TCP pro nas.\n");
                    if (buf[TCP_FLAGS_P] & TCP_FLAGS_SYN_V)
       				{	// chce jen potvrdit syn, posleme ancknowledge
                        make_tcp_synack_from_syn(buf);
                   	}
                    else if (buf[TCP_FLAGS_P] & TCP_FLAGS_ACK_V)
        			{
        	            init_len_info(buf); // init some data structures
        	            // mozna nic neprijimame, jen jde o acknowlege na nas paket
        	            dat_p=get_tcp_data_pointer();
        	            if (dat_p==0)
        				{   // ano, paket neobsahuje data
        	                if (buf[TCP_FLAGS_P] & TCP_FLAGS_FIN_V) // finack, answer with ack
        					{
        	                    make_tcp_ack_from_any(buf);
        	                }
        	            }
        	            else if (strncmp("GET ",(char *)&(buf[dat_p]),4)!=0)
        				{
        			        // head, post and other methods - for possible status codes see:
        			        // http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
        			        plen=fill_tcp_data_p(buf,0,"HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n<h1>200 OK</h1>");
            				make_tcp_ack_from_any(buf); // send ack for http get
            				make_tcp_ack_with_data(buf, (uint16_t)plen); // send data
       					}
        	            else if (strncmp("/ ",(char *)&(buf[dat_p+4]),2)==0)
        				{	// v datech nemame lomitko, takze tam neni heslo!!
        			        plen=fill_tcp_data_p(buf,0,"HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n");
        			        plen=fill_tcp_data_p(buf,plen,"<p>Usage: ");
        			        plen=fill_tcp_data(buf,plen,baseurl);
        			        plen=fill_tcp_data_p(buf,plen,"password</p>");
            				make_tcp_ack_from_any(buf); // send ack for http get
            				make_tcp_ack_with_data(buf, (uint16_t)plen); // send data
        				}
        	            else
        	            {
            				cmd=analyse_get_url((uint8_t *)&(buf[dat_p+5]));
            				if (cmd==-1)
           					{
            			        plen=fill_tcp_data_p(buf,0,"HTTP/1.0 401 Unauthorized\r\nContent-Type: text/html\r\n\r\n<h1>401 Unauthorized</h1>");
	            				make_tcp_ack_from_any(buf); // send ack for http get
                				make_tcp_ack_with_data(buf, (uint16_t)plen); // send data
           					}
            				else if (cmd==1)
            				{
            					//GPIO_SetBits(GPIOC, GPIO_Pin_8);	// rozsvitime modrou LED na kitu dle pozadavku
                                gpio_put(RED_LED_PIN, 1);
            					svitLED=1;
	               				plen=print_webpage(buf,(svitLED));
	            				make_tcp_ack_from_any(buf); // send ack for http get
	            				make_tcp_ack_with_data(buf, (uint16_t)plen); // send data
            				}
            				else if (cmd==0)
            				{
            					//GPIO_ResetBits(GPIOC, GPIO_Pin_8);	// zhasneme modrou LED na kitu dle pozadavku
                                gpio_put(RED_LED_PIN, 0);
            					svitLED=0;
	               				plen=print_webpage(buf,(svitLED));
	            				make_tcp_ack_from_any(buf); // send ack for http get
	            				make_tcp_ack_with_data(buf, (uint16_t)plen); // send data
           					}
            				else
            				{
	               				// if (cmd==-2) or any other value
	               				// just display the status:
                                   
	               				plen=print_webpage(buf,(svitLED));
	            				make_tcp_ack_from_any(buf); // send ack for http get
	            				make_tcp_ack_with_data(buf, (uint16_t)plen); // send data
            				}
        	            }
        			} // if (buf[TCP_FLAGS_P] & TCP_FLAGS_ACK_V)
        		} // tcp port www END
                // udp start, we listen on udp port 1200=0x4B0
        		if (buf[IP_PROTO_P]==IP_PROTO_UDP_V&&buf[UDP_DST_PORT_H_P]==4&&buf[UDP_DST_PORT_L_P]==0xb0)
        		{
        			payloadlen=buf[UDP_LEN_L_P]-UDP_HEADER_LEN;
        			// you must sent a string starting with v
        			// e.g udpcom version 10.0.0.24
        			if (strncmp(password, (uint8_t *)&(buf[UDP_DATA_P]),5)==0)
       				{	// find the first comma which indicates the start of a command:
        				cmd_pos=0;
        				while(cmd_pos<payloadlen)
        				{
        					cmd_pos++;
        					if (buf[UDP_DATA_P+cmd_pos]==',')
        					{
        					    cmd_pos++; // put on start of cmd
        					    break;
        					}
        				}
        				// a command is one char and a value. At
        				// least 3 characters long. It has an '=' on
        				// position 2:
        				if (cmd_pos<2 || cmd_pos>payloadlen-3 || buf[UDP_DATA_P+cmd_pos+1]!='=')
        				{
        					strcpy(str,"e=no_cmd");
        				} else
        				// supported commands are t=1 t=0 t=?
        				if (buf[UDP_DATA_P+cmd_pos]=='t')
        				{
        					cmdval=buf[UDP_DATA_P+cmd_pos+2];
        					if(cmdval=='1')
        					{
            					//GPIO_SetBits(GPIOC, GPIO_Pin_8);	// rozsvitime modrou LED na kitu dle pozadavku
                                gpio_put(RED_LED_PIN, 1);
            					svitLED=1;
       						    strcpy(str,"L=1");
   							} else
   							if(cmdval=='0')
        					{
   								//GPIO_ResetBits(GPIOC, GPIO_Pin_8);	// zhasneme modrou LED na kitu dle pozadavku
                                gpio_put(RED_LED_PIN, 0);       
            					svitLED=0;
        						strcpy(str,"L=0");
   							} else
   							if(cmdval=='?')
        					{
   							    if(0 == 0 /*STM32vldiscovery_PBGetState(BUTTON_USER)*/)
   								{
   									strcpy(str,"t=0");
   						    	}
   								else
   								{
        						    strcpy(str,"t=1");
   								}
   							}
       					}
        				else
        				{
            				strcpy(str,"e=no_such_cmd");
        				}
        			}
        			else
        			{
            			strcpy(str,"e=invalid_pw");
        			}
        			make_udp_reply_from_request(buf, str, strlen(str), myudpport);
        		} // udp port END
        	} // if(eth_type_is_ip_and_my_ip(buf,plen)!=0)
        }
	} // while
	//GPIO_ResetBits(GPIOC, GPIO_Pin_9); 	// zmenime stav  LED3 - zelena
    gpio_put(LED_PIN, 0);
}

// takes a string of the form password/commandNumber and analyse it
// return values: -1 invalid password, otherwise command number
//                -2 no command given but password valid
int8_t analyse_get_url(uint8_t *str)
	{
	uint8_t i=0;

	i = strncmp(password,str,5);
	if (i != 0)
	{
		return(-1);
	}
	// find first "/"
	// passw not longer than 9 char:
	while(*str && i<10 && *str >',' && *str<'{')
	{
        if (*str=='/')
		{
            str++;
            break;
       	}
        i++;
        str++;
	}
	if (*str < 0x3a && *str > 0x2f)
	{
        // is a ASCII number, return it
        return(*str-0x30);
	}
	return(-2);
}


// prepare the webpage by writing the data to the tcp send buffer
uint16_t print_webpage(uint8_t *buf,uint8_t on_off)
	{
    uint16_t plen;
    plen=fill_tcp_data_p(buf,0,"HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nPragma: no-cache\r\n\r\n");
    plen=fill_tcp_data_p(buf,plen,"<html><body bgcolor=\"#AAFFAA\"><center><h1>STM32 VL Discovery kit</h1>\n<h3>http://mcu.cz server</h3><br><br><br><br>\n");
    plen=fill_tcp_data_p(buf,plen,"<p>LED stav: ");
    if (on_off)
			{
            plen=fill_tcp_data_p(buf,plen,"<font color=\"#0000FF\"> SVITI </font>");
    		}
		else
			{
            plen=fill_tcp_data_p(buf,plen," nesviti ");
    		}
    plen=fill_tcp_data_p(buf,plen," <BR><BR><BR><small><a href=\"");
    plen=fill_tcp_data(buf,plen,baseurl);
    plen=fill_tcp_data(buf,plen,password);
    plen=fill_tcp_data_p(buf,plen,"\">Aktualizace</a></small></p>\n<p>");
    plen=fill_tcp_data_p(buf,plen,"<br><br><br><br>");
//stisk tlacitka    
    if(0 == 0 /*STM32vldiscovery_PBGetState(BUTTON_USER)*/)
	{
        plen=fill_tcp_data_p(buf,plen,"<br><p>Tlacitko nestisknuto.<br><br><br></p>\n");
	}
	else
	{
        plen=fill_tcp_data_p(buf,plen,"<br><p>Tlacitko <b>STISKNUTO</b>.<br><br><br></p>\n");
	}
    
    plen=fill_tcp_data_p(buf,plen,"<p><a href=\"");
    // the url looks like this http://baseurl/password/command
    plen=fill_tcp_data(buf,plen,baseurl);
    plen=fill_tcp_data(buf,plen,password);
    if (on_off)
			{
            plen=fill_tcp_data_p(buf,plen,"/0\">zhasnout LED</a><p>");
    		}
		else
			{
            plen=fill_tcp_data_p(buf,plen,"/1\">rozsvitit LED</a><p>");
    		}
    plen=fill_tcp_data_p(buf,plen,"</center><br><br><br><hr><br>STM32F100 Value Line UCOS-II V2.85 WEB\n</body></html>");
    
    return(plen);
}
