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

//uint8_t mac[6] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46};
uint8_t mymac[7] = {'R', 'A', 'D', 'E', 'K', 'T',0};
uint8_t myip[4] = {192, 168, 100, 111};

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

	while(1);


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