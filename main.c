#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "enc28j60.h"
#include "ip_arp_udp_tcp.h"
#include "net.h"
#include "pico/unique_id.h"
#include "hardware/gpio.h"
#include "server.h"


#define PSTR(s) s
uint LED_PIN = 25;
uint RED_LED_PIN = 20;
uint8_t svitLED = 0;

//uint8_t mymac[6] = {0xdc, 0xa6, 0x32, 0xf4, 0x31, 0x66};
uint8_t mymac[7] = {'R', 'A', 'D', 'E', 'K', 'T',0};
uint8_t myip[4] = {192, 168, 100, 111};

uint8_t pimac[6] = {0xdc, 0xa6, 0x32, 0xf4, 0x31, 0x67};
uint8_t piip[4] = {192, 168, 100, 200};

uint8_t baseurl[23]="http://192.168.100.111/";
uint16_t mywwwport =80; // listen port for tcp/www (max range 1-254)
uint16_t myudpport =1200;

#define BUFFER_SIZE 1500//400
static uint8_t buf[BUFFER_SIZE+1];

uint8_t password[]="123456"; // must not be longer than 9 char

void gpio_callback(uint gpio, uint32_t events)
{
	if (gpio == 15)	{
		enc28j60_interrupt_handler();
	}
}



int main() {
     
    //const uint LED_PIN = 25;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

	gpio_init(RED_LED_PIN);
    gpio_set_dir(RED_LED_PIN, GPIO_OUT);

    stdio_init_all();
    printf("\033[2J");
    
    enc28j60_init(mymac);
    init_ip_arp_udp_tcp((uint8_t *)mymac, (uint8_t *)myip, 80);

    printf("\n");
	pico_unique_board_id_t pico_id;
	pico_get_unique_board_id(&pico_id);
	printf("Pico ID: %llx\n", pico_id);

	gpio_set_irq_enabled_with_callback(15, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);


	uint8_t pcip[4] = {192, 168, 100, 138};	
	uint8_t pcmac[6] = {0x04, 0x92, 0x26, 0xd3, 0x6c, 0xfc};
	

	send_arp_probe(pcip);
	send_arp_request(pcip);

	send_echo_request(pcip, pcmac);
	send_echo_request(piip, pimac);

	uint8_t databuf[18] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18};
	send_udp_packet(pcip, pcmac, databuf, 0);

	while(1);

    while (true) {
		send_echo_request(pcip, pcmac);
		sleep_ms(1000);
//        gpio_put(LED_PIN, 1);
//        sleep_ms(50);
//        gpio_put(LED_PIN, 0);
//        sleep_ms(600);

    }
}