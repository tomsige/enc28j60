#include "pico/stdlib.h"

#ifndef _TCPIP_H
#define _TCPIP_H

extern uint LED_PIN;
extern uint RED_LED_PIN;

extern uint8_t mymac[];
extern uint8_t myip[];

// base url (you can put a DNS name instead of an IP addr. if you have
// a DNS server (baseurl must end in "/"):
extern uint8_t baseurl[];
extern uint16_t mywwwport; // listen port for tcp/www (max range 1-254)
extern uint16_t myudpport; // listen port for udp

//extern uint8_t buf[];

// the password string (only the first 5 char checked), (only a-z,0-9,_ characters):
extern uint8_t password[]; // must not be longer than 9 char

extern uint8_t svitLED;
//extern uint16_t pomocny;


int8_t analyse_get_url(uint8_t *str);
uint16_t print_webpage(uint8_t *buf,uint8_t on_off);
void enc28j60_interrupt_handler(void);

#endif