
#include "pico/stdlib.h"
#include "enc28j60.h"

#ifndef IP_ARP_UDP_TCP_H
#define IP_ARP_UDP_TCP_H

//extern void enc28j60_send_packet(uint8_t *data, uint16_t len);

// you must call this function once before you use any of the other functions:
void init_ip_arp_udp_tcp( uint8_t * mymac, uint8_t * myip, uint16_t wwwp);

uint8_t eth_type_is_arp_and_my_ip(uint8_t *buf,uint16_t len);
uint8_t eth_type_is_ip_and_my_ip(uint8_t *buf,uint16_t len);
void make_arp_answer_from_request(uint8_t *buf);
void make_echo_reply_from_request(uint8_t *buf,uint16_t len);
void make_udp_reply_from_request(uint8_t *buf, uint8_t *data,uint16_t datalen,uint16_t port);


void make_tcp_synack_from_syn(uint8_t *buf);
void init_len_info(uint8_t *buf);
uint8_t get_tcp_data_pointer(void);
uint8_t fill_tcp_data_p(uint8_t *buf,uint16_t pos, const uint8_t *progmem_s);
uint8_t fill_tcp_data(uint8_t *buf,uint16_t pos, const uint8_t *s);
void make_tcp_ack_from_any(uint8_t *buf);
void make_tcp_ack_with_data(uint8_t *buf,uint16_t dlen);

void send_arp_probe(uint8_t *desired_ip);
void send_arp_request(uint8_t *desired_ip);

void send_echo_request(uint8_t *desired_ip, uint8_t *desired_mac);
void send_udp_packet(uint8_t *dst_ip, uint8_t *dst_mac, uint8_t *data_buff, uint16_t datalen);

#endif 