#include <stdio.h>
#include <string.h>
#include "net.h"
#include "pico/stdlib.h"
#include "enc28j60.h"

#define  pgm_read_byte(ptr)  ((uint8_t)*(ptr))

static uint8_t wwwport;
static uint8_t macaddr[6];
static uint8_t ipaddr[4];
static uint16_t info_hdr_len;
static uint16_t info_data_len;
static uint8_t seqnum; // my initial tcp sequence number = 0xA

// The Ip checksum is calculated over the ip header only starting
// with the header length field and a total length of 20 bytes
// unitl ip.dst
// You must set the IP checksum field to zero before you start
// the calculation.
// len for ip is 20.
//
// For UDP/TCP we do not make up the required pseudo header. Instead we 
// use the ip.src and ip.dst fields of the real packet:
// The udp checksum calculation starts with the ip.src field
// Ip.src=4bytes,Ip.dst=4 bytes,Udp header=8bytes + data length=16+len
// In other words the len here is 8 + length over which you actually
// want to calculate the checksum.
// You must set the checksum field to zero before you start
// the calculation.
// len for udp is: 8 + 8 + data length
// len for tcp is: 4+4 + 20 + option len + data length
//
// For more information on how this algorithm works see:
// http://www.netfor2.com/checksum.html
// http://www.msc.uky.edu/ken/cs471/notes/chap3.htm
// The RFC has also a C code example: http://www.faqs.org/rfcs/rfc1071.html

uint16_t checksum(uint8_t *buf, uint16_t len,uint8_t type)
{
	// type 0=ip 
	//      1=udp
	//      2=tcp
	uint32_t sum = 0;
	
	//if(type==0){
	//        // do not add anything
	//}
	if(type==1)
	{
		sum+=IP_PROTO_UDP_V; // protocol udp
		// the length here is the length of udp (data+header len)
		// =length given to this function - (IP.scr+IP.dst length)
		sum+=len-8; // = real tcp len
	}
	if(type==2)
	{
		sum+=IP_PROTO_TCP_V; 
		// the length here is the length of tcp (data+header len)
		// =length given to this function - (IP.scr+IP.dst length)
		sum+=len-8; // = real tcp len
	}
	// build the sum of 16bit words
	while(len >1)
	{
		sum += 0xFFFF & (*buf<<8|*(buf+1));
		buf+=2;
		len-=2;
	}
	// if there is a byte left then add it (padded with zero)
	if (len)
		sum += (0xFF & *buf)<<8;
	// now calculate the sum over the bytes in the sum
	// until the result is only 16bit long
	while (sum>>16)
		sum = (sum & 0xFFFF)+(sum >> 16);
	// build 1's complement:
	return( (uint16_t) sum ^ 0xFFFF);
}

// you must call this function once before you use any of the other functions:
void init_ip_arp_udp_tcp(uint8_t *mymac,uint8_t *myip,uint8_t wwwp)
{
	uint8_t i=0;
	wwwport=wwwp;
	info_hdr_len=0;
	info_data_len=0;
	seqnum=0xA;
	while(i<4)
	{
        ipaddr[i]=myip[i];
        i++;
	}
	i=0;
	while(i<6)
	{
        macaddr[i]=mymac[i];
        i++;
	}
}

uint8_t eth_type_is_arp_and_my_ip(uint8_t *buf, uint8_t len)
{
	uint8_t i=0;
	//  
	if (len<41)
	    return(0);
	if(buf[ETH_TYPE_H_P] != ETHTYPE_ARP_H_V || buf[ETH_TYPE_L_P] != ETHTYPE_ARP_L_V)
	    return(0);

	//moje vsuvka
	if (!(buf[ETH_ARP_OPCODE_H_P] != ETH_ARP_OPCODE_REPLY_H_V || buf[ETH_ARP_OPCODE_L_P] != ETH_ARP_OPCODE_REPLY_L_V))
		printf("ARP response.\n");

    // pokracovani puvodniho 
	while(i<4)
	{
	    if(buf[ETH_ARP_DST_IP_P+i] != ipaddr[i])
	        return(0);
	    i++;
	}
	return(1);
}

uint8_t eth_type_is_ip_and_my_ip(uint8_t *buf,uint8_t len)
{
	uint8_t i=0;
	//eth+ip+udp header is 42
	if (len<42)
	    return(0);
	if(buf[ETH_TYPE_H_P]!=ETHTYPE_IP_H_V || buf[ETH_TYPE_L_P]!=ETHTYPE_IP_L_V)
	    return(0);
	if (buf[IP_HEADER_LEN_VER_P]!=0x45)
	{
	    // must be IP V4 and 20 byte header
	    return(0);
	}
	while(i<4)
	{
	    if(buf[IP_DST_P+i]!=ipaddr[i])
	        return(0);
	    i++;
	}
	return(1);
}

// make a return eth header from a received eth packet
void make_eth(uint8_t *buf)
{
	uint8_t i=0;
	//
	//copy the destination mac from the source and fill my mac into src
	while(i<6)
	{
        buf[ETH_DST_MAC +i]=buf[ETH_SRC_MAC +i];
        buf[ETH_SRC_MAC +i]=macaddr[i];
        i++;
	}
}

void fill_ip_hdr_checksum(uint8_t *buf)
{
	uint16_t ck;
	// clear the 2 byte checksum
	buf[IP_CHECKSUM_P]=0;
	buf[IP_CHECKSUM_P+1]=0;
	buf[IP_FLAGS_P]=0x40; // don't fragment
	buf[IP_FLAGS_P+1]=0;  // fragement offset
	buf[IP_TTL_P]=64; // ttl
	// calculate the checksum:
	ck=checksum(&buf[IP_P], IP_HEADER_LEN,0);
	buf[IP_CHECKSUM_P]=ck>>8;
	buf[IP_CHECKSUM_P+1]=ck& 0xff;
}

// make a return ip header from a received ip packet
void make_ip(uint8_t *buf)
{
	uint8_t i=0;
	while(i<4)
	{
        buf[IP_DST_P+i]=buf[IP_SRC_P+i];
        buf[IP_SRC_P+i]=ipaddr[i];
        i++;
	}
	fill_ip_hdr_checksum(buf);
}

// make a return ip header from a received ip packet for echo reply
void make_ip_echo(uint8_t *buf)
{
	uint8_t i=0;
	uint16_t ck;
	while(i<4)
	{
        buf[IP_DST_P+i]=buf[IP_SRC_P+i];
        buf[IP_SRC_P+i]=ipaddr[i];
        i++;
	}
	// clear the 2 byte checksum
	i = buf[IP_CHECKSUM_P];
	buf[IP_CHECKSUM_P]=0;
	buf[IP_CHECKSUM_P+1]=0;
	buf[IP_FLAGS_P]=0x40; // don't fragment
	buf[IP_FLAGS_P+1]=0;  // fragement offset
	buf[IP_TTL_P]=64; // ttl
	// calculate the checksum:
	ck=checksum(&buf[IP_P], IP_HEADER_LEN,0);
	buf[IP_CHECKSUM_P]=i;
	buf[IP_CHECKSUM_P+1]=ck& 0xff;
}


// make a return tcp header from a received tcp packet
// rel_ack_num is how much we must step the seq number received from the
// other side. We do not send more than 255 bytes of text (=data) in the tcp packet.
// If mss=1 then mss is included in the options list
//
// After calling this function you can fill in the first data byte at TCP_OPTIONS_P+4
// If cp_seq=0 then an initial sequence number is used (should be use in synack)
// otherwise it is copied from the packet we received
void make_tcphead(uint8_t *buf,uint16_t rel_ack_num,uint8_t mss,uint8_t cp_seq)
{
	uint8_t i=0;
	uint8_t tseq;
	while(i<2)
	{
	    buf[TCP_DST_PORT_H_P+i]=buf[TCP_SRC_PORT_H_P+i];
	    buf[TCP_SRC_PORT_H_P+i]=0; // clear source port
	    i++;
	}
	// set source port  (http):
	buf[TCP_SRC_PORT_L_P]=wwwport;
	i=4;
	// sequence numbers:
	// add the rel ack num to SEQACK
	while(i>0)
	{
	    rel_ack_num=buf[TCP_SEQ_H_P+i-1]+rel_ack_num;
	    tseq=buf[TCP_SEQACK_H_P+i-1];
	    buf[TCP_SEQACK_H_P+i-1]=0xff&rel_ack_num;
	    if (cp_seq)
		{
	        // copy the acknum sent to us into the sequence number
	        buf[TCP_SEQ_H_P+i-1]=tseq;
    	}
		else
		{
            buf[TCP_SEQ_H_P+i-1]= 0; // some preset vallue
   		}
	    rel_ack_num=rel_ack_num>>8;
	    i--;
	}
	if (cp_seq==0)
	{
	    // put inital seq number
	    buf[TCP_SEQ_H_P+0]= 0;
	    buf[TCP_SEQ_H_P+1]= 0;
	    // we step only the second byte, this allows us to send packts 
	    // with 255 bytes or 512 (if we step the initial seqnum by 2)
	    buf[TCP_SEQ_H_P+2]= seqnum; 
	    buf[TCP_SEQ_H_P+3]= 0;
	    // step the inititial seq num by something we will not use
	    // during this tcp session:
	    seqnum+=2;
	}
	// zero the checksum
	buf[TCP_CHECKSUM_H_P]=0;
	buf[TCP_CHECKSUM_L_P]=0;
	
	// The tcp header length is only a 4 bit field (the upper 4 bits).
	// It is calculated in units of 4 bytes. 
	// E.g 24 bytes: 24/4=6 => 0x60=header len field
	//buf[TCP_HEADER_LEN_P]=(((TCP_HEADER_LEN_PLAIN+4)/4)) <<4; // 0x60
	if (mss)
	{
	    // the only option we set is MSS to 1408:
	    // 1408 in hex is 0x580
	    buf[TCP_OPTIONS_P]=2;
	    buf[TCP_OPTIONS_P+1]=4;
	    buf[TCP_OPTIONS_P+2]=0x05;
	    buf[TCP_OPTIONS_P+3]=0x80;
	    // 24 bytes:
	    buf[TCP_HEADER_LEN_P]=0x60;
	}
	else
	{
	    // no options:
	    // 20 bytes:
	    buf[TCP_HEADER_LEN_P]=0x50;
	}
}

void make_arp_answer_from_request(uint8_t *buf)
{
	uint8_t i=0;
	//
	make_eth(buf);
	buf[ETH_ARP_OPCODE_H_P]=ETH_ARP_OPCODE_REPLY_H_V;
	buf[ETH_ARP_OPCODE_L_P]=ETH_ARP_OPCODE_REPLY_L_V;
	// fill the mac addresses:
	while(i<6)
	{
        buf[ETH_ARP_DST_MAC_P+i]=buf[ETH_ARP_SRC_MAC_P+i];
        buf[ETH_ARP_SRC_MAC_P+i]=macaddr[i];
        i++;
	}
	i=0;
	while(i<4)
	{
        buf[ETH_ARP_DST_IP_P+i]=buf[ETH_ARP_SRC_IP_P+i];
        buf[ETH_ARP_SRC_IP_P+i]=ipaddr[i];
        i++;
	}
	// eth+arp is 42 bytes:
	//enc28j60PacketSend(42,buf);
	enc28j60_send_packet(buf, 42);
}

void make_echo_reply_from_request(uint8_t *buf,uint16_t len)
{
	make_eth(buf);
	make_ip(buf);
	buf[ICMP_TYPE_P]=ICMP_TYPE_ECHOREPLY_V;
	//////////////////////////////////////////////////////////////////////////////////
	// we changed only the icmp.type field from request(=8) to reply(=0).
	// we can therefore easily correct the checksum:
	if (buf[ICMP_CHECKSUM_P] > (0xff-0x08))
	    buf[ICMP_CHECKSUM_P+1]++;
	buf[ICMP_CHECKSUM_P]+=0x08;
	//
	//enc28j60PacketSend(len,buf);
	enc28j60_send_packet(buf, len);
}

// you can send a max of 220 bytes of data
void make_udp_reply_from_request(uint8_t *buf, uint8_t *data, uint8_t datalen, uint16_t port)
{
	uint8_t i=0;
	uint16_t ck;
	make_eth(buf);
	if (datalen>220)
	    datalen=220;
	// total length field in the IP header must be set:
	buf[IP_TOTLEN_H_P]=0;
	buf[IP_TOTLEN_L_P]=IP_HEADER_LEN+UDP_HEADER_LEN+datalen;
	make_ip(buf);
	buf[UDP_DST_PORT_H_P]=port>>8;
	buf[UDP_DST_PORT_L_P]=port & 0xff;
	// source port does not matter and is what the sender used.
	// calculte the udp length:
	buf[UDP_LEN_H_P]=0;
	buf[UDP_LEN_L_P]=UDP_HEADER_LEN+datalen;
	// zero the checksum
	buf[UDP_CHECKSUM_H_P]=0;
	buf[UDP_CHECKSUM_L_P]=0;
	// copy the data:
	while(i<datalen)
	{
        buf[UDP_DATA_P+i]=data[i];
        i++;
	}
	ck=checksum(&buf[IP_SRC_P], 16 + datalen,1);
	buf[UDP_CHECKSUM_H_P]=ck>>8;
	buf[UDP_CHECKSUM_L_P]=ck& 0xff;
	//enc28j60PacketSend(UDP_HEADER_LEN+IP_HEADER_LEN+ETH_HEADER_LEN+datalen,buf);
	enc28j60_send_packet(buf, UDP_HEADER_LEN+IP_HEADER_LEN+ETH_HEADER_LEN+datalen);
}

void make_tcp_synack_from_syn(uint8_t *buf)
{
	uint16_t ck;
	make_eth(buf);
	// total length field in the IP header must be set:
	// 20 bytes IP + 24 bytes (20tcp+4tcp options)
	buf[IP_TOTLEN_H_P]=0;
	buf[IP_TOTLEN_L_P]=IP_HEADER_LEN+TCP_HEADER_LEN_PLAIN+4;
	make_ip(buf);
	buf[TCP_FLAGS_P]=TCP_FLAGS_SYNACK_V;
	make_tcphead(buf,1,1,0);
	// calculate the checksum, len=8 (start from ip.src) + TCP_HEADER_LEN_PLAIN + 4 (one option: mss)
	ck=checksum(&buf[IP_SRC_P], 8+TCP_HEADER_LEN_PLAIN+4,2);
	buf[TCP_CHECKSUM_H_P]=ck>>8;
	buf[TCP_CHECKSUM_L_P]=ck& 0xff;
	// add 4 for option mss:
	//enc28j60PacketSend(IP_HEADER_LEN+TCP_HEADER_LEN_PLAIN+4+ETH_HEADER_LEN,buf);
	enc28j60_send_packet(buf, IP_HEADER_LEN+TCP_HEADER_LEN_PLAIN+4+ETH_HEADER_LEN);
}

// get a pointer to the start of tcp data in buf
// Returns 0 if there is no data
// You must call init_len_info once before calling this function
uint16_t get_tcp_data_pointer(void)
{
	if (info_data_len)
		return((uint16_t)TCP_SRC_PORT_H_P+info_hdr_len);
	else
        return(0);
}

// do some basic length calculations and store the result in static varibales
void init_len_info(uint8_t *buf)
{
    info_data_len=(buf[IP_TOTLEN_H_P]<<8)|(buf[IP_TOTLEN_L_P]&0xff);
    info_data_len-=IP_HEADER_LEN;
    info_hdr_len=(buf[TCP_HEADER_LEN_P]>>4)*4; // generate len in bytes;
    info_data_len-=info_hdr_len;
    if (info_data_len<=0)
        info_data_len=0;
}

// fill in tcp data at position pos. pos=0 means start of
// tcp data. Returns the position at which the string after
// this string could be filled.
uint16_t fill_tcp_data_p(uint8_t *buf, uint16_t pos, const uint8_t *progmem_s)
{
	uint8_t c;
	// fill in tcp data at position pos
	//
	// with no options the data starts after the checksum + 2 more bytes (urgent ptr)
	while ((c = pgm_read_byte(progmem_s++))) 
	{
	    buf[TCP_CHECKSUM_L_P+3+pos]=c;
	    pos++;
	}
	return(pos);
}

// fill in tcp data at position pos. pos=0 means start of
// tcp data. Returns the position at which the string after
// this string could be filled.
uint16_t fill_tcp_data(uint8_t *buf, uint16_t pos, const uint8_t *s)
{
	// fill in tcp data at position pos
	//
	// with no options the data starts after the checksum + 2 more bytes (urgent ptr)
	while (*s) 
	{
	    buf[TCP_CHECKSUM_L_P+3+pos]=*s;
	    pos++;
	    s++;
	}
	return(pos);
}

// Make just an ack packet with no tcp data inside
// This will modify the eth/ip/tcp header 
void make_tcp_ack_from_any(uint8_t *buf)
{
	uint16_t j;
	make_eth(buf);
	// fill the header:
	buf[TCP_FLAGS_P]=TCP_FLAGS_ACK_V;
	if (info_data_len==0)
	{
        // if there is no data then we must still acknoledge one packet
        make_tcphead(buf,1,0,1); // no options
	}
	else
	{
        make_tcphead(buf,info_data_len,0,1); // no options
	}
	
	// total length field in the IP header must be set:
	// 20 bytes IP + 20 bytes tcp (when no options) 
	j=IP_HEADER_LEN+TCP_HEADER_LEN_PLAIN;
	buf[IP_TOTLEN_H_P]=j>>8;
	buf[IP_TOTLEN_L_P]=j& 0xff;
	make_ip(buf);
	// calculate the checksum, len=8 (start from ip.src) + TCP_HEADER_LEN_PLAIN + data len
	j=checksum(&buf[IP_SRC_P], 8+TCP_HEADER_LEN_PLAIN,2);
	buf[TCP_CHECKSUM_H_P]=j>>8;
	buf[TCP_CHECKSUM_L_P]=j& 0xff;
	//enc28j60PacketSend(IP_HEADER_LEN+TCP_HEADER_LEN_PLAIN+ETH_HEADER_LEN,buf);
	enc28j60_send_packet(buf, IP_HEADER_LEN+TCP_HEADER_LEN_PLAIN+ETH_HEADER_LEN);
}

// you must have called init_len_info at some time before calling this function
// dlen is the amount of tcp data (http data) we send in this packet
// You can use this function only immediately after make_tcp_ack_from_any
// This is because this function will NOT modify the eth/ip/tcp header except for
// length and checksum
void make_tcp_ack_with_data(uint8_t *buf, uint16_t dlen)
{
	uint16_t j;
	// fill the header:
	// This code requires that we send only one data packet
	// because we keep no state information. We must therefore set
	// the fin here:
	buf[TCP_FLAGS_P]=TCP_FLAGS_ACK_V|TCP_FLAGS_PUSH_V|TCP_FLAGS_FIN_V;
	
	// total length field in the IP header must be set:
	// 20 bytes IP + 20 bytes tcp (when no options) + len of data
	j=IP_HEADER_LEN+TCP_HEADER_LEN_PLAIN+dlen;
	buf[IP_TOTLEN_H_P]=j>>8;
	buf[IP_TOTLEN_L_P]=j& 0xff;
	fill_ip_hdr_checksum(buf);
	// zero the checksum
	buf[TCP_CHECKSUM_H_P]=0;
	buf[TCP_CHECKSUM_L_P]=0;
	// calculate the checksum, len=8 (start from ip.src) + TCP_HEADER_LEN_PLAIN + data len
	j=checksum(&buf[IP_SRC_P], 8+TCP_HEADER_LEN_PLAIN+dlen,2);
	buf[TCP_CHECKSUM_H_P]=j>>8;
	buf[TCP_CHECKSUM_L_P]=j& 0xff;
	//enc28j60PacketSend(IP_HEADER_LEN+TCP_HEADER_LEN_PLAIN+dlen+ETH_HEADER_LEN,buf);
	enc28j60_send_packet(buf, IP_HEADER_LEN+TCP_HEADER_LEN_PLAIN+dlen+ETH_HEADER_LEN);
}

/* end of ip_arp_udp.c */


uint16_t compute_sum(uint16_t sum, void const *ptr, int len)
{
	int i;
	uint32_t s = sum;
	for(i=0; i<len; i++)	{
		uint16_t c = ((uint8_t const *)ptr)[i];
		if (i & 1)	{
			s += c;
		} else {
			s += c << 8;
		}
		s = (s + (s >> 16)) & 0xffff;
	}
	return (uint16_t)s;
}



void send_arp_probe(uint8_t *desired_ip)
{
	struct eth_arp_frame arp_packet;
	memset(&arp_packet, 0, sizeof(struct eth_arp_frame));

	memset(&arp_packet.eth.dst, 0xFF, 6);
	memcpy(&arp_packet.eth.src, &macaddr, 6);
	arp_packet.eth.type[0] = 0x08;
	arp_packet.eth.type[1] = 0x06;
	//arp_packet.eth.type = ((0x0806 << 8) & 0xFF00) || ((0x0806 >> 8) & 0x00FF);
	arp_packet.arp.hw_type[0] = 0x00;
	arp_packet.arp.hw_type[1] = 0x01;
	arp_packet.arp.protocol_type[0] = 0x08;
	arp_packet.arp.protocol_type[1] = 0x00;
	arp_packet.arp.hw_size = 0x06;
	arp_packet.arp.protocol_size = 0x04;
	arp_packet.arp.opcode[0] = 0x00;
	arp_packet.arp.opcode[1] = 0x01;
	memcpy(&arp_packet.arp.src_hw_addr, &macaddr, 6);
	memcpy(&arp_packet.arp.dst_ip_addr, desired_ip, 4);

	enc28j60_send_packet((uint8_t *)&arp_packet, sizeof(struct eth_arp_frame));
}


void send_arp_request(uint8_t *desired_ip)
{
	struct eth_arp_frame arp_packet;
	memset(&arp_packet, 0, sizeof(struct eth_arp_frame));

	memset(&arp_packet.eth.dst, 0xFF, 6);
	//memcpy(&arp_packet.eth.src, &macaddr, 6);
	arp_packet.eth.type[0] = 0x08;
	arp_packet.eth.type[1] = 0x06;
	//arp_packet.eth.type = ((0x0806 << 8) & 0xFF00) || ((0x0806 >> 8) & 0x00FF);
	arp_packet.arp.hw_type[0] = 0x00;
	arp_packet.arp.hw_type[1] = 0x01;
	arp_packet.arp.protocol_type[0] = 0x08;
	arp_packet.arp.protocol_type[1] = 0x00;
	arp_packet.arp.hw_size = 0x06;
	arp_packet.arp.protocol_size = 0x04;
	arp_packet.arp.opcode[0] = 0x00;
	arp_packet.arp.opcode[1] = 0x01;
	memcpy(&arp_packet.arp.src_hw_addr, &macaddr, 6);
	memcpy(&arp_packet.arp.src_ip_addr, &ipaddr, 4);
	memcpy(&arp_packet.arp.dst_ip_addr, desired_ip, 4);

	enc28j60_send_packet((uint8_t *)&arp_packet, sizeof(struct eth_arp_frame));
}

//MAC mit nekde ulozenou a nepredavat ji ve funkci
void send_echo_request(uint8_t *desired_ip, uint8_t *desired_mac)
{
	struct eth_ip_icmp_frame icmp_packet;
	memset(&icmp_packet, 0, sizeof(struct eth_ip_icmp_frame));

	//eth
	memcpy(&icmp_packet.eth.dst, desired_mac, 6);
	memcpy(&icmp_packet.eth.src, &macaddr, 6);
	memset(&icmp_packet.eth.type[0], 0x08, 1);
	memset(&icmp_packet.eth.type[1], 0x00, 1);

	//ip
	memset(&icmp_packet.ip.version_and_length, 0x45, 1);
	memset(&icmp_packet.ip.service, 0, 1);
	memset(&icmp_packet.ip.total_length[1], 0x44, 1);

	memset(&icmp_packet.ip.flags_and_fragment_offset[0], 0x40, 1);
	//memset(&icmp_packet.ip.identification[0], 0x40, 1);
	//memset(&icmp_packet.ip.identification[1], 0x40, 1);
	
	memset(&icmp_packet.ip.checksum[0], 0x0, 1);
	memset(&icmp_packet.ip.checksum[1], 0x0, 1);

	memset(&icmp_packet.ip.time_to_live, 0x40, 1);
	memset(&icmp_packet.ip.protocol, 0x01, 1);
	memcpy(&icmp_packet.ip.src, &ipaddr, 4);
	memcpy(&icmp_packet.ip.dst, desired_ip, 4);

	uint32_t sum;
	sum = compute_sum(0, &icmp_packet.ip, sizeof(struct ip_frame));
	sum = ~sum;

	memset(&icmp_packet.ip.checksum[0], (uint8_t)(sum >> 8), 1);
	memset(&icmp_packet.ip.checksum[1], (uint8_t)(sum), 1);

	//icmp
	memset(&icmp_packet.icmp.type, 0x08, 1);
	memset(&icmp_packet.icmp.code, 0x00, 1);
	memset(&icmp_packet.icmp.sequence[0], 0x00, 1);
	memset(&icmp_packet.icmp.sequence[1], 0x01, 1);
/*	
	memset(&icmp_packet.icmp.timestamp[0], 0xc2, 1);
	memset(&icmp_packet.icmp.timestamp[1], 0xcb, 1);
	memset(&icmp_packet.icmp.timestamp[2], 0x4a, 1);
	memset(&icmp_packet.icmp.timestamp[3], 0x60, 1);
	memset(&icmp_packet.icmp.timestamp[4], 0x6c, 1);
	memset(&icmp_packet.icmp.timestamp[5], 0x64, 1);
	memset(&icmp_packet.icmp.timestamp[6], 0x09, 1);
	memset(&icmp_packet.icmp.timestamp[7], 0x00, 1);
*/	
	memset(&icmp_packet.icmp.data, 0x41, 32);

	sum = compute_sum(0, &icmp_packet.icmp, sizeof(struct icmp_frame));
	sum = ~sum;

	memset(&icmp_packet.icmp.checksum[0], (uint8_t)(sum >> 8), 1);
	memset(&icmp_packet.icmp.checksum[1], (uint8_t)(sum), 1);


	enc28j60_send_packet((uint8_t *)&icmp_packet, sizeof(struct eth_ip_icmp_frame));
}


void send_udp_packet(uint8_t *dst_ip, uint8_t *dst_mac, uint8_t *data_buff, uint16_t datalen)	{

	uint8_t buffer[ENC28J60_MAXFRAME];
	
	memset(buffer, 0, sizeof(buffer));

	struct eth_ip_udp_frame *udp;
	uint8_t *data;
	
    udp = (struct eth_ip_udp_frame *) buffer;
	data = buffer + sizeof(struct eth_ip_udp_frame);

	//eth
	memcpy(&udp->eth.dst, dst_mac, 6);
	memcpy(&udp->eth.src, &macaddr, 6);
	memset(&udp->eth.type[0], 0x08, 1);
	memset(&udp->eth.type[1], 0x00, 1);

	//ip
	memset(&udp->ip.version_and_length, 0x45, 1);
	memset(&udp->ip.service, 0, 1);
	memset(&udp->ip.flags_and_fragment_offset[0], 0x40, 1);
	//memset(&udp_packet.ip.identification[0], 0x40, 1);
	//memset(&udp_packet.ip.identification[1], 0x40, 1);

	memset(&udp->ip.time_to_live, 0x40, 1);
	memset(&udp->ip.protocol, 17, 1);
	memcpy(&udp->ip.src, &ipaddr, 4);
	memcpy(&udp->ip.dst, dst_ip, 4);

	//udp
	udp->udp.dst_port[0] = 0x13;
	udp->udp.dst_port[1] = 0x8d;
	udp->udp.src_port[0] = 0x8d;
	udp->udp.src_port[1] = 0x13;
   
	memcpy(data, data_buff, datalen);
	if (datalen < 18){
		datalen = 18;  // count padding in
	}

	udp->ip.total_length[0] = (sizeof(struct ip_frame) + sizeof(struct udp_frame) + datalen) >> 8;
	udp->ip.total_length[1] = (sizeof(struct ip_frame) + sizeof(struct udp_frame) + datalen) & 0xFF;
	
	uint32_t sum;
	sum = compute_sum(0, &udp->ip, sizeof(struct ip_frame));
	sum = ~sum;
	memset(&udp->ip.checksum[0], (uint8_t)(sum >> 8), 1);
	memset(&udp->ip.checksum[1], (uint8_t)(sum), 1);

	udp->udp.length[0] = (uint8_t)((8 + datalen) >> 8);
	udp->udp.length[1] = (uint8_t)((8 + datalen) & 0xFF);

	struct	{
		uint8_t src[4];
		uint8_t dst[4];
		uint8_t	 zero;
		uint8_t  protocol;
		uint8_t length[2];
	} pseudo_header;

	memcpy(&pseudo_header.src, udp->ip.src, 4);
	memcpy(&pseudo_header.dst, udp->ip.dst, 4);
	pseudo_header.zero = 0;
	pseudo_header.protocol = udp->ip.protocol;
	pseudo_header.length[0] = udp->udp.length[0];
	pseudo_header.length[1] = udp->udp.length[1];

	sum = compute_sum(0, &pseudo_header, sizeof(pseudo_header));
	sum = compute_sum(sum, &udp->udp, sizeof(struct udp_frame) + datalen);
	sum = ~sum;

	memset(&udp->udp.checksum[0], (uint8_t)(sum >> 8), 1);
	memset(&udp->udp.checksum[1], (uint8_t)(sum), 1);			

	enc28j60_send_packet((uint8_t *)udp, sizeof(struct eth_ip_udp_frame) + datalen);
}

