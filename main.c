#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "enc28j60.h"

uint8_t mac[6] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46};


int main() {
     
    const uint LED_PIN = 25;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    stdio_init_all();
    printf("\033[2J");
    
    enc28j60_init(mac);

    printf("\n");
    printf("EREVID 0x%x\n", enc28j60_rcr(EREVID));
    printf("ESTAT 0x%x\n", enc28j60_rcr(ESTAT));
    printf("ERDPTL 0x%x\n", enc28j60_rcr(ERDPTL));
    printf("ERDPTH 0x%x\n", enc28j60_rcr(ERDPTH));
    
 
    printf("MAADR5 0x%x (%c)\n", enc28j60_rcr(MAADR5),enc28j60_rcr(MAADR5));
    printf("MAADR4 0x%x (%c)\n", enc28j60_rcr(MAADR4),enc28j60_rcr(MAADR4));
    printf("MAADR3 0x%x (%c)\n", enc28j60_rcr(MAADR3),enc28j60_rcr(MAADR3));
    printf("MAADR2 0x%x (%c)\n", enc28j60_rcr(MAADR2),enc28j60_rcr(MAADR2));
    printf("MAADR1 0x%x (%c)\n", enc28j60_rcr(MAADR1),enc28j60_rcr(MAADR1));
    printf("MAADR0 0x%x (%c)\n", enc28j60_rcr(MAADR0),enc28j60_rcr(MAADR0));



    while (true) {

        gpio_put(LED_PIN, 1);
        sleep_ms(50);
        gpio_put(LED_PIN, 0);
        sleep_ms(600);
    }
}