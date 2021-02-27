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

// Read register pair   
/*
uint16_t enc28j60_rcr16(uint8_t adr)
{
	enc28j60_set_bank(adr);
	return enc28j60_read_op(ENC28J60_SPI_RCR, adr) |
		(enc28j60_read_op(ENC28J60_SPI_RCR, adr+1) << 8);
}
*/

// Write register
void enc28j60_wcr(uint8_t adr, uint8_t data)
{
	enc28j60_set_bank(adr);
	enc28j60_write_op(ENC28J60_SPI_WCR, adr, data);
}

// Write register pair
/*
void enc28j60_wcr16(uint8_t adr, uint16_t data)
{
	enc28j60_set_bank(adr);
	enc28j60_write_op(ENC28J60_SPI_WCR, adr, data);
	enc28j60_write_op(ENC28J60_SPI_WCR, adr+1, data>>8);
}
*/

// Clear bits in register (reg &= ~mask)
void enc28j60_bfc(uint8_t adr, uint8_t mask)
{
	enc28j60_set_bank(adr);
	enc28j60_write_op(ENC28J60_SPI_BFC, adr, mask);
}

// Set bits in register (reg |= mask)
void enc28j60_bfs(uint8_t adr, uint8_t mask)
{
	enc28j60_set_bank(adr);
	enc28j60_write_op(ENC28J60_SPI_BFS, adr, mask);
}


// read the revision of the chip:
uint8_t enc28j60getrev(void)
{
	return(enc28j60_rcr(EREVID));
}

// Read Rx/Tx buffer (at ERDPT)
void enc28j60_read_buffer(uint8_t *buf, uint16_t len)
{
    enc28j60_wcr(ERDPTL, ENC28J60_RXSTART & 0xFF);
    enc28j60_wcr(ERDPTH, ENC28J60_RXSTART >> 8);

	uint8_t cmd = ENC28J60_SPI_RBM; 

	enc28j60_select();
	spi_write_blocking(SPI_PORT, &cmd, 1);
	spi_read_blocking(SPI_PORT, 0, buf, len);		
	enc28j60_release();
}

// Write Rx/Tx buffer (at EWRPT)
void enc28j60_write_buffer(uint8_t *buf, uint16_t len)
{
	uint8_t cmd = ENC28J60_SPI_WBM;

    enc28j60_wcr(EWRPTL, ENC28J60_TXSTART & 0xFF);
    enc28j60_wcr(EWRPTH, ENC28J60_TXSTART >> 8);

	enc28j60_select();
	spi_write_blocking(SPI_PORT, &cmd, 1);
	spi_write_blocking(SPI_PORT, buf, len);
	enc28j60_release();
}





void enc28j60_init(uint8_t *macadrs)  {

    printf("\nStarting ENC28J60 init procedure\n");
    printf("MAC %s\n", macadrs);

    //spi_set_format(SPI_PORT, 8, 0, 0, SPI_MSB_FIRST);
    spi_deinit(SPI_PORT);
    spi_init(SPI_PORT, 1000 * 1000); //20 MHz

    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    
    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);

    enc28j60_soft_reset();

	// set receive buffer start address
	enc28j60_rxrdpt = ENC28J60_RXSTART;

	// Setup Rx/Tx buffer
	enc28j60_wcr(ERXSTL, ENC28J60_RXSTART&0xFF);
	enc28j60_wcr(ERXSTH, ENC28J60_RXSTART>>8);	

	// set receive pointer address
	enc28j60_wcr(ERXRDPTL, ENC28J60_RXEND&0xFF);
	enc28j60_wcr(ERXRDPTH, ENC28J60_RXEND>>8);

	// RX end
	enc28j60_wcr(ERXNDL, ENC28J60_RXEND&0xFF);
	enc28j60_wcr(ERXNDH, ENC28J60_RXEND>>8);

	// TX start
	enc28j60_wcr(ETXSTL, ENC28J60_TXSTART&0xFF);
	enc28j60_wcr(ETXSTH, ENC28J60_TXSTART>>8);

	// TX end
	enc28j60_wcr(ETXNDL, ENC28J60_TXSTART&0xFF);
	enc28j60_wcr(ETXNDH, ENC28J60_TXSTART>>8);

	uint8_t buf[2] = {ENC28J60_SPI_WBM, 0x00};
	spi_write_blocking(SPI_PORT, buf, 2);
	


	//BANK 3
	enc28j60_wcr(MAADR5, macadrs[0]); // Set MAC address
    enc28j60_wcr(MAADR4, macadrs[1]);
	enc28j60_wcr(MAADR3, macadrs[2]);
	enc28j60_wcr(MAADR2, macadrs[3]);
	enc28j60_wcr(MAADR1, macadrs[4]);       
	enc28j60_wcr(MAADR0, macadrs[5]);


    printf("ENC28J60 init procedure done.\n");

	// switch to bank 0
	enc28j60_set_bank(ECON1);
	enc28j60_release();
}
