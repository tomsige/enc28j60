#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "enc28j60.h"

//uint8_t mac[6] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46};
uint8_t mac[6] = {'R', 'A', 'D', 'E', 'K', 'T'};

int main() {
     
    const uint LED_PIN = 25;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    stdio_init_all();
    printf("\033[2J");
    
    enc28j60_init(mac);

    printf("\n");
    
    uint8_t reread[10] = {0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20};

    enc28j60_write_buffer(mac, 6);
    enc28j60_read_buffer(reread, 6);

    while (true) {

        gpio_put(LED_PIN, 1);
        sleep_ms(50);
        gpio_put(LED_PIN, 0);
        sleep_ms(600);
    }
}