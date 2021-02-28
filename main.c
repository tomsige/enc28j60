#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "enc28j60.h"
#include "ip_arp_udp_tcp.h"
#include "net.h"

#define PSTR(s) s

//uint8_t mac[6] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46};
uint8_t mac[6] = {'R', 'A', 'D', 'E', 'K', 'T'};
uint8_t ip[4] = {192, 168, 100, 111};

static char baseurl[]="http://192.168.100.111/";
static uint16_t mywwwport =80; // listen port for tcp/www (max range 1-254)
static uint16_t myudpport =1200;

#define BUFFER_SIZE 1500//400
static uint8_t buf[BUFFER_SIZE+1];

static char password[]="123456"; // must not be longer than 9 char
const unsigned char WebSide[] = {
"<p><b><span lang=EN-US>A/D </span>STM32 Vl Discovery<span lang=EN-US>:(</span>1234<span\r\n"
"lang=EN-US>0~<st1:chmetcnv UnitName=\"AC\" SourceValue=\"50\" HasSpace=\"False\"\r\n"
"Negative=\"False\" NumberType=\"1\" TCSC=\"0\" w:st=\"on\">50 AC</st1:chmetcnv>)</span></b></p>\r\n"
"\r\n"
"<table class=MsoNormalTable border=1 cellspacing=0 cellpadding=0 width=540\r\n"
" style='width:405.0pt;mso-cellspacing:0cm;background:red;border:outset 4.5pt;\r\n"
" mso-padding-alt:0cm 0cm 0cm 0cm'>\r\n"
" <tr style='mso-yfti-irow:0;mso-yfti-firstrow:yes;mso-yfti-lastrow:yes'>\r\n"
"  <td style='padding:0cm 0cm 0cm 0cm'>\r\n"
"  <table class=MsoNormalTable border=0 cellspacing=0 cellpadding=0\r\n"
"   style='mso-cellspacing:0cm;mso-padding-alt:0cm 0cm 0cm 0cm'>\r\n"
"   <tr style='mso-yfti-irow:0;mso-yfti-firstrow:yes;mso-yfti-lastrow:yes'>\r\n"
"    <td style='background:lime;padding:0cm 0cm 0cm 0cm'>\r\n"
"    <p class=MsoNormal><span lang=EN-US>&nbsp;</span></p>\r\n"
"    </td>\r\n"
};
// 
uint8_t verify_password(char *str)
	{
	// the first characters of the received string are
	// a simple password/cookie:
	if (strncmp(password,str,5)==0)
		{
	    return(1);
		}
	return(0);
	}

// takes a string of the form password/commandNumber and analyse it
// return values: -1 invalid password, otherwise command number
//                -2 no command given but password valid
int8_t analyse_get_url(char *str)
	{
	uint8_t i=0;
	if (verify_password(str)==0)
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
    plen=fill_tcp_data_p(buf,0,PSTR("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nPragma: no-cache\r\n\r\n"));
    plen=fill_tcp_data_p(buf,plen,PSTR("<html><body><center><h1>Raspberry Pi Pico</h1>\n<h1>http://mcu.cz server</h1><br><br><br><br>\n"));
    plen=fill_tcp_data_p(buf,plen,PSTR("<p>LED: "));
    if (on_off)
			{
            plen=fill_tcp_data_p(buf,plen,PSTR("<font color=\"#0000FF\"> sviti </font>"));
    		}
		else
			{
            plen=fill_tcp_data_p(buf,plen,PSTR(" nesviti "));
    		}
    //plen=fill_tcp_data_p(buf,plen,PSTR(" <small><a href=\""));
    //plen=fill_tcp_data(buf,plen,baseurl);
    //plen=fill_tcp_data(buf,plen,password);
    //plen=fill_tcp_data_p(buf,plen,PSTR("\">[DA]</a></small></p>\n<p><a href=\""));
    plen=fill_tcp_data_p(buf,plen,PSTR("<br><br><br><br>"));
    if(0 == 0 /*STM32vldiscovery_PBGetState(BUTTON_USER)*/)
	{
        plen=fill_tcp_data_p(buf,plen,PSTR("<br><p>Tlacitko nestisknuto.<br><br><br></p>\n"));
	}
	else
	{
        plen=fill_tcp_data_p(buf,plen,PSTR("<br><p>Tlacitko <b>stisknuto</b>.<br><br><br></p>\n"));
	}
    plen=fill_tcp_data_p(buf,plen,PSTR("<p><a href=\""));
    // the url looks like this http://baseurl/password/command
    plen=fill_tcp_data(buf,plen,baseurl);
    plen=fill_tcp_data(buf,plen,password);
    if (on_off)
			{
            plen=fill_tcp_data_p(buf,plen,PSTR("/0\">zhasnout LED</a><p>"));
    		}
		else
			{
            plen=fill_tcp_data_p(buf,plen,PSTR("/1\">rozsvitit LED</a><p>"));
    		}
    plen=fill_tcp_data_p(buf,plen,PSTR("</center><br><br><br><hr><br>Raspberry Pi Pico web\n</body></html>"));
    //plen=fill_tcp_data_p(buf,plen,PSTR(WebSide));
    
    return(plen);
	}

int main() {
     
    const uint LED_PIN = 25;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    stdio_init_all();
    printf("\033[2J");
    
    enc28j60_init(mac);
    init_ip_arp_udp_tcp((uint8_t *)mac, (uint8_t *)ip, 80);

    printf("\n");

// simple server

    uint16_t plen;
    uint16_t dat_p;
    uint8_t i=0;
    uint8_t cmd_pos=0;
    int8_t cmd;
    uint8_t payloadlen=0;
    char str[30];
    char cmdval;


   while(1)
    	{
		//OSTimeDlyHMSM(0, 0, 0, 50);
        // get the next new packet:
        //plen = enc28j60PacketReceive(BUFFER_SIZE, buf);
        plen = enc28j60_recv_packet(buf, BUFFER_SIZE);
        //USART_DMASendData(USART1,buf,plen);

        /*plen will ne unequal to zero if there is a valid packet (without crc error) */
        if(plen==0)
        	{
            continue;
        	}

        printf("Packet with size %ld received\n", plen);
        // arp is broadcast if unknown but a host may also
        // verify the mac address by sending it to 
        // a unicast address.
        if(eth_type_is_arp_and_my_ip(buf,plen))
        	{
            printf("  ARP request for my IP received\n");
			make_arp_answer_from_request(buf);
			//USART_DMASendText(USART1,"make_arp_answer_from_request\n");
            continue;
        	}

        // check if ip packets are for us:
        if(eth_type_is_ip_and_my_ip(buf,plen)==0)
        	{
            continue;
        	}

        
        if(buf[IP_PROTO_P]==IP_PROTO_ICMP_V && buf[ICMP_TYPE_P]==ICMP_TYPE_ECHOREQUEST_V)
        	{
            // a ping packet, let's send pong	
            printf("  Ping received\n");
			make_echo_reply_from_request(buf, plen);
			//USART_DMASendText(USART1,"make_echo_reply_from_request\n");
			continue;
        	}
               // tcp port www start, compare only the lower byte
		if (buf[IP_PROTO_P]==IP_PROTO_TCP_V&&buf[TCP_DST_PORT_H_P]==0&&buf[TCP_DST_PORT_L_P]==mywwwport)
			{
            if (buf[TCP_FLAGS_P] & TCP_FLAGS_SYN_V)
				{
                make_tcp_synack_from_syn(buf);
                // make_tcp_synack_from_syn does already send the syn,ack
                continue;
            	}
	        if (buf[TCP_FLAGS_P] & TCP_FLAGS_ACK_V)
				{
	            init_len_info(buf); // init some data structures
	            // we can possibly have no data, just ack:
	            dat_p=get_tcp_data_pointer();
	            if (dat_p==0)
					{
	                if (buf[TCP_FLAGS_P] & TCP_FLAGS_FIN_V)
						{
	                    // finack, answer with ack
	                    make_tcp_ack_from_any(buf);
	                	}
	                // just an ack with no data, wait for next packet
	                continue;
	                }
				if (strncmp("GET ",(char *)&(buf[dat_p]),4)!=0)
					{
			        // head, post and other methods:
			        //
			        // for possible status codes see:
			        // http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
			        plen=fill_tcp_data_p(buf,0,PSTR("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n<h1>200 OK</h1>"));
			        goto SENDTCP;
					}
				if (strncmp("/ ",(char *)&(buf[dat_p+4]),2)==0)
					{
			        plen=fill_tcp_data_p(buf,0,PSTR("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n"));
			        plen=fill_tcp_data_p(buf,plen,PSTR("<p>Usage: "));
			        plen=fill_tcp_data(buf,plen,baseurl);
			        plen=fill_tcp_data_p(buf,plen,PSTR("password</p>"));
			        goto SENDTCP;
					}
				cmd=analyse_get_url((char *)&(buf[dat_p+5]));
				// for possible status codes see:
				// http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
				if (cmd==-1)
					{
			        plen=fill_tcp_data_p(buf,0,PSTR("HTTP/1.0 401 Unauthorized\r\nContent-Type: text/html\r\n\r\n<h1>401 Unauthorized</h1>"));
			        goto SENDTCP;
					}
				if (cmd==1)
					{
			        //PORTD|= (1<<PD7);// transistor on
					//IOCLR |= (1<<26);
                    //LED1ON();
					//  - GPIO_SetBits(GPIOC, GPIO_Pin_8);
                    gpio_put(LED_PIN, 1);
					i=1;
					}
				if (cmd==0)
					{
			        //PORTD &= ~(1<<PD7);// transistor off
					//IOSET |= (1<<26);
                    //LED1OFF();
					// - GPIO_ResetBits(GPIOC, GPIO_Pin_8);
                    gpio_put(LED_PIN, 0);
					i=0;
					}
				// if (cmd==-2) or any other value
				// just display the status:
				plen=print_webpage(buf,(i));
				
				SENDTCP:
				make_tcp_ack_from_any(buf); // send ack for http get
				make_tcp_ack_with_data(buf, (uint16_t)plen); // send data
				continue;
				}
			}
	// tcp port www end
	//
	// udp start, we listen on udp port 1200=0x4B0
		if (buf[IP_PROTO_P]==IP_PROTO_UDP_V&&buf[UDP_DST_PORT_H_P]==4&&buf[UDP_DST_PORT_L_P]==0xb0)
			{
			payloadlen=buf[UDP_LEN_L_P]-UDP_HEADER_LEN;
			// you must sent a string starting with v
			// e.g udpcom version 10.0.0.24
			if (verify_password((char *)&(buf[UDP_DATA_P])))
				{
				// find the first comma which indicates 
				// the start of a command:
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
					goto ANSWER;
					}
				// supported commands are
				// t=1 t=0 t=?
				if (buf[UDP_DATA_P+cmd_pos]=='t')
					{
					cmdval=buf[UDP_DATA_P+cmd_pos+2];
					if(cmdval=='1')
							{
						    //PORTD|= (1<<PD7);// transistor on
							//IOCLR |= (1<<26);
                            //LED1ON();
						    strcpy(str,"t=1");
						    goto ANSWER;
							}
						else if(cmdval=='0')
							{
						    //PORTD &= ~(1<<PD7);// transistor off
							//IOSET |= (1<<26);
                            //LED1OFF();
						    strcpy(str,"t=0");
						    goto ANSWER;
							}
						else if(cmdval=='?')
							{
	/*
						    if (IOPIN & (1<<26))
								{
					            strcpy(str,"t=1");
					            goto ANSWER;
						    	}
	*/
						    strcpy(str,"t=0");
						    goto ANSWER;
							}
					}
				strcpy(str,"e=no_such_cmd");
				goto ANSWER;
				}
			strcpy(str,"e=invalid_pw");
			ANSWER:
			make_udp_reply_from_request(buf,str,strlen(str),myudpport);
			
			}
		}


// simple server




/*    
    uint8_t reread[10] = {0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20};
    
    printf("PHCON1  "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(enc28j60_read_phy(PHCON1)>>8), BYTE_TO_BINARY(enc28j60_read_phy(PHCON1)));
    printf("PHSTAT1 "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(enc28j60_read_phy(PHSTAT1)>>8), BYTE_TO_BINARY(enc28j60_read_phy(PHSTAT1)));
    printf("PHID1   "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(enc28j60_read_phy(PHID1)>>8), BYTE_TO_BINARY(enc28j60_read_phy(PHID1)));
    printf("PHID2   "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(enc28j60_read_phy(PHID2)>>8), BYTE_TO_BINARY(enc28j60_read_phy(PHID2)));
    printf("PHCON2  "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(enc28j60_read_phy(PHCON2)>>8), BYTE_TO_BINARY(enc28j60_read_phy(PHCON2)));
    printf("PHSTAT2 "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(enc28j60_read_phy(PHSTAT2)>>8), BYTE_TO_BINARY(enc28j60_read_phy(PHSTAT2)));
    printf("PHIE    "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(enc28j60_read_phy(PHIE)>>8), BYTE_TO_BINARY(enc28j60_read_phy(PHIE)));
    printf("PHIR    "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(enc28j60_read_phy(PHIR)>>8), BYTE_TO_BINARY(enc28j60_read_phy(PHIR)));
    printf("PHLCON  "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(enc28j60_read_phy(PHLCON)>>8), BYTE_TO_BINARY(enc28j60_read_phy(PHLCON))); 

    enc28j60_write_buffer(mac, 6);
    enc28j60_read_buffer(reread, 6);
*/


    while (true) {

        gpio_put(LED_PIN, 1);
        sleep_ms(50);
        gpio_put(LED_PIN, 0);
        sleep_ms(600);
    }
}