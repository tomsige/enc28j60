#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "enc28j60.h"
#include <stdio.h>

#define PIN_SCK  10
#define PIN_MOSI 11
#define PIN_MISO 12
#define PIN_CS   13
        
#define SPI_PORT spi1

uint8_t enc28j60_current_bank = 0;
uint16_t enc28j60_rxrdpt = 0;

void enc28j60_select(void)
{
    gpio_put(PIN_CS, 0);  // Active low
}

void enc28j60_release(void)
{
    gpio_put(PIN_CS, 1);
}

// Generic SPI read command
uint8_t enc28j60_read_op(uint8_t cmd, uint8_t adr)
{
	uint8_t input;
    uint8_t output;
      
    input = cmd | (adr & ENC28J60_ADDR_MASK);

    enc28j60_select();
    spi_write_blocking(SPI_PORT, &input, 1);     
    spi_read_blocking(SPI_PORT, 0, &output, 1);
    if (adr & 0x80) {
        spi_read_blocking(SPI_PORT, 0, &output, 1);
    }         
    enc28j60_release();
    return output;
}

// Generic SPI write command
void enc28j60_write_op(uint8_t cmd, uint8_t adr, uint8_t data)
{
    uint8_t buf[2];

    buf[0] = cmd | (adr & ENC28J60_ADDR_MASK);
    buf[1] = data;

	enc28j60_select();
    spi_write_blocking(SPI_PORT, buf, 2);
	enc28j60_release();    
}

// Initiate software reset
void enc28j60_soft_reset()
{
	enc28j60_write_op(ENC28J60_SPI_SR, 0, ENC28J60_SPI_SR);
	sleep_ms(2000); // Wait until device initializes

	enc28j60_current_bank = 0;
}

/*
 * Memory access
 */

// Set register bank
void enc28j60_set_bank(uint8_t adr)
{
	uint8_t bank;

	if( (adr & ENC28J60_BANK_MASK) != enc28j60_current_bank )
	{
		bank = (adr & ENC28J60_BANK_MASK) >> 5; //BSEL1|BSEL0=0x03
		enc28j60_write_op(ENC28J60_SPI_BFC, ECON1, 0x03);
		enc28j60_write_op(ENC28J60_SPI_BFS, ECON1, bank);
		enc28j60_current_bank = (adr & ENC28J60_BANK_MASK);
	}
}

// Read register
uint8_t enc28j60_rcr(uint8_t adr)
{
	enc28j60_set_bank(adr);
	return enc28j60_read_op(ENC28J60_SPI_RCR, adr);
}

// Write register
void enc28j60_wcr(uint8_t adr, uint8_t data)
{
	enc28j60_set_bank(adr);
	enc28j60_write_op(ENC28J60_SPI_WCR, adr, data);
}




void enc28j60_init(uint8_t *macadrs)  {

    printf("\nStarting ENC28J60 init procedure\n");
    printf("MAC %s\n", macadrs);

    //spi_set_format(SPI_PORT, 8, 0, 0, SPI_MSB_FIRST);
    spi_deinit(SPI_PORT);
    spi_init(SPI_PORT, 20000 * 1000); //20 MHz

    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    
    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);

    enc28j60_soft_reset();

	//BANK 3
	enc28j60_wcr(MAADR5, macadrs[0]); // Set MAC address
    enc28j60_wcr(MAADR4, macadrs[1]);
	enc28j60_wcr(MAADR3, macadrs[2]);
	enc28j60_wcr(MAADR2, macadrs[3]);
	enc28j60_wcr(MAADR1, macadrs[4]);       
	enc28j60_wcr(MAADR0, macadrs[5]);


    printf("ENC28J60 init procedure done.\n");
}
