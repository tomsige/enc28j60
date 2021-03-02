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
uint16_t enc28j60_rcr16(uint8_t adr)
{
	enc28j60_set_bank(adr);
	return enc28j60_read_op(ENC28J60_SPI_RCR, adr) |
		(enc28j60_read_op(ENC28J60_SPI_RCR, adr+1) << 8);
}

// Write register
void enc28j60_wcr(uint8_t adr, uint8_t data)
{
	enc28j60_set_bank(adr);
	enc28j60_write_op(ENC28J60_SPI_WCR, adr, data);
}
/*
// Write register pair
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
	// pro test zapisu a cteni ze stejneho mista v pameti 
    //enc28j60_wcr(ERDPTL, ENC28J60_RXSTART & 0xFF);
    //enc28j60_wcr(ERDPTH, ENC28J60_RXSTART >> 8);

	uint8_t cmd = ENC28J60_SPI_RBM; 

	enc28j60_select();
	spi_write_blocking(SPI_PORT, &cmd, 1);
	spi_read_blocking(SPI_PORT, 0, buf, len);		
	enc28j60_release();
}

// Write Rx/Tx buffer (at EWRPT)
void enc28j60_write_buffer(uint8_t *buf, uint16_t len)
{
	// pro test zapisu a cteni ze stejneho mista v pameti
    //enc28j60_wcr(EWRPTL, ENC28J60_TXSTART & 0xFF);
    //enc28j60_wcr(EWRPTH, ENC28J60_TXSTART >> 8);

	uint8_t cmd = ENC28J60_SPI_WBM;

	enc28j60_select();
	spi_write_blocking(SPI_PORT, &cmd, 1);
	spi_write_blocking(SPI_PORT, buf, len);
	enc28j60_release();
}

// Read PHY register
uint16_t enc28j60_read_phy(uint8_t adr)
{
	enc28j60_wcr(MIREGADR, adr);
	enc28j60_bfs(MICMD, MICMD_MIIRD);
	while(enc28j60_rcr(MISTAT) & MISTAT_BUSY)
		;
	enc28j60_bfc(MICMD, MICMD_MIIRD);
	return enc28j60_rcr16(MIRD);
}

// Write PHY register
void enc28j60_write_phy(uint8_t adr, uint16_t data)
{
	enc28j60_wcr(MIREGADR, adr);
	//enc28j60_wcr16(MIWR, data);
	enc28j60_wcr(MIWRL, data);
	enc28j60_wcr(MIWRH, data>>8);
	//Delay(100);
	while(enc28j60_rcr(MISTAT) & MISTAT_BUSY)
		;
}

void enc28j60_interrupt_enable(uint16_t level)
{   // switch to bank 0
    enc28j60_set_bank(ECON1);
    enc28j60_write_op(ENC28J60_SPI_BFS, EIE, level);
}

void enc28j60_init(uint8_t *macadrs)  {

    printf("\nStarting ENC28J60 init procedure\n");
    printf("MAC %s\n", macadrs);

    //spi_set_format(SPI_PORT, 8, 0, 0, SPI_MSB_FIRST);
    spi_deinit(SPI_PORT);
    spi_init(SPI_PORT, 50000 * 1000); //20 MHz

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
	
// BANK 1
	// do bank 1 stuff, packet filter:
        // For broadcast packets we allow only ARP packtets
        // All other packets should be unicast only for our mac (MAADR)
        //
        // The pattern to match on is therefore
        // Type     ETH.DST
        // ARP      BROADCAST
        // 06 08 -- ff ff ff ff ff ff -> ip checksum for theses bytes=f7f9
        // in binary these poitions are:11 0000 0011 1111
        // This is hex 303F->EPMM0=0x3f,EPMM1=0x30

	enc28j60_wcr(ERXFCON, ERXFCON_UCEN|ERXFCON_CRCEN|ERXFCON_PMEN);
	enc28j60_wcr(EPMM0, 0x3f);
	enc28j60_wcr(EPMM1, 0x30);
	enc28j60_wcr(EPMCSL, 0xf9);
	enc28j60_wcr(EPMCSH, 0xf7);

// BANK 2

	// Setup MAC
	enc28j60_wcr(MACON1, MACON1_TXPAUS| // Enable flow control
		MACON1_RXPAUS|MACON1_MARXEN); // Enable MAC Rx

	enc28j60_wcr(MACON2, 0); // Clear reset
	enc28j60_wcr(MACON3, MACON3_PADCFG0| // Enable padding,
		MACON3_TXCRCEN|MACON3_FRMLNEN|MACON3_FULDPX); // Enable crc & frame len chk

	//enc28j60_wcr16(MAMXFL, ENC28J60_MAXFRAME);
	enc28j60_wcr(MABBIPG, 0x15); // Set inter-frame gap 12 for HAL DUPLEX!
    // Allow infinite deferals if the medium is continuously busy
    // (do not time out a transmission if the half duplex medium is
    // completely saturated with other people's data)
	enc28j60_wcr(MACON4, MACON4_DEFER);
	// Late collisions occur beyond 63+8 bytes (8 bytes for preamble/start of frame delimiter)
	// 55 is all that is needed for IEEE 802.3, but ENC28J60 B5 errata for improper link pulse
	// collisions will occur less often with a larger number.
	enc28j60_wcr(MACLCON2, 63);

	enc28j60_wcr(MAIPGL, 0x12);
	enc28j60_wcr(MAIPGH, 0x0c);

	// Set the maximum packet size which the controller will accept
        // Do not send packets longer than ENC28J60_MAXFRAME
	enc28j60_wcr(MAMXFLL, ENC28J60_MAXFRAME&0xFF);
	enc28j60_wcr(MAMXFLH, ENC28J60_MAXFRAME>>8);

	//BANK 3
	enc28j60_wcr(MAADR5, macadrs[0]); // Set MAC address
    enc28j60_wcr(MAADR4, macadrs[1]);
	enc28j60_wcr(MAADR3, macadrs[2]);
	enc28j60_wcr(MAADR2, macadrs[3]);
	enc28j60_wcr(MAADR1, macadrs[4]);       
	enc28j60_wcr(MAADR0, macadrs[5]);

	// Disable the CLKOUT output to reduce EMI generation
	enc28j60_wcr(ECOCON, 0x00); // Output off (0V)

	// Setup PHY
	enc28j60_write_phy(PHCON1, PHCON1_PDPXMD); // Force full-duplex mode
	enc28j60_write_phy(PHCON2, PHCON2_HDLDIS); // Disable loopback
	enc28j60_write_phy(PHLCON, PHLCON_LACFG2| // Configure LED ctrl
		PHLCON_LBCFG2|PHLCON_LBCFG1|PHLCON_LBCFG0|
		PHLCON_LFRQ0|PHLCON_STRCH);

	// switch to bank 0
	enc28j60_set_bank(ECON1);

	// Enable Received Packet interrupts only
	enc28j60_wcr(EIE, EIE_INTIE | EIE_PKTIE);

	// Enable Rx packets
	enc28j60_bfs(ECON1, ECON1_RXEN);

	enc28j60_release();
    
	printf("ENC28J60 init procedure done.\n");
}

void enc28j60_send_packet(uint8_t *data, uint16_t len)
{
	while(enc28j60_rcr(ECON1) & ECON1_TXRTS)
	{
		// TXRTS may not clear - ENC28J60 bug. We must reset
		// transmit logic in cause of Tx error
		if(enc28j60_rcr(EIR) & EIR_TXERIF)
		{
			enc28j60_bfs(ECON1, ECON1_TXRST);
			enc28j60_bfc(ECON1, ECON1_TXRST);
		}
	}

	enc28j60_wcr(EWRPTL, ENC28J60_TXSTART & 0xFF);
	enc28j60_wcr(EWRPTH, ENC28J60_TXSTART >> 8);

	enc28j60_wcr(ETXNDL, (ENC28J60_TXSTART + len) & 0xFF);
	enc28j60_wcr(ETXNDH, (ENC28J60_TXSTART + len) >> 8);

    enc28j60_write_op(ENC28J60_SPI_WBM, 0, 0x00);
	enc28j60_write_buffer(data, len);

    enc28j60_write_op(ENC28J60_SPI_BFS, ECON1, ECON1_TXRTS);

    // Reset the transmit logic problem. See Rev. B4 Silicon Errata point 12.
    if ((enc28j60_rcr(EIR) & EIR_TXERIF))
    {
    	enc28j60_write_op(ENC28J60_SPI_BFC, ECON1, ECON1_TXRTS);
    }
}

uint16_t enc28j60_recv_packet(uint8_t *buf, uint16_t buflen)
{
	uint16_t len = 0, rxlen, status;

	if (enc28j60_rcr(EPKTCNT) == 0)
	{
		return(0);
	}

	enc28j60_wcr(ERDPTL, enc28j60_rxrdpt);
	enc28j60_wcr(ERDPTH, (enc28j60_rxrdpt) >> 8);

	// read the next packet pointer
	enc28j60_rxrdpt = enc28j60_read_op(ENC28J60_SPI_RBM, 0); // read low byte
	enc28j60_rxrdpt |= enc28j60_read_op(ENC28J60_SPI_RBM, 0) << 8;	// read high byte

	// read the packet length (see datasheet page 43)
	rxlen = enc28j60_read_op(ENC28J60_SPI_RBM, 0);	// read low byte
	rxlen |= enc28j60_read_op(ENC28J60_SPI_RBM, 0) << 8;   // read high byte
	rxlen -= 4; //remove the CRC count

	// read the receive status (see datasheet page 43)
	status = enc28j60_read_op(ENC28J60_SPI_RBM, 0);
	status |= enc28j60_read_op(ENC28J60_SPI_RBM, 0) << 8;

	// limit retrieve length
	if (rxlen > buflen - 1)
	{
		rxlen = buflen - 1;
	}

	// check CRC and symbol errors (see datasheet page 44, table 7-3):
	// The ERXFCON.CRCEN is set by default. Normally we should not
	// need to check this.
	if ((status & 0x80) == 0)
	{
		// invalid
		len = 0;
	}
	else
	{
		// copy the packet from the receive buffer
		//enc28j60_read_buffer((void*)&enc28j60_rxrdpt, rxlen);
		enc28j60_read_buffer(buf, rxlen);
	}

	// Move the RX read pointer to the start of the next received packet
	// This frees the memory we just read out
	enc28j60_wcr(ERXRDPTL, enc28j60_rxrdpt);
	enc28j60_wcr(ERXRDPTH, (enc28j60_rxrdpt) >> 8);

	// decrement the packet counter indicate we are done with this packet
    enc28j60_write_op(ENC28J60_SPI_BFS, ECON2, ECON2_PKTDEC);

    enc28j60_release();
	return(rxlen);
}

void enc28j60_drop_packet(void)
{
	// check if a packet has been received and buffered
	//if( !(enc28j60Read(EIR) & EIR_PKTIF) ){
	// The above does not work. See Rev. B4 Silicon Errata point 6.
	if (enc28j60_rcr(EPKTCNT) == 0)
	{
		return;
	}

	// Set the read pointer to the start of the received packet
	enc28j60_wcr(ERDPTL, enc28j60_rxrdpt);
	enc28j60_wcr(ERDPTH, (enc28j60_rxrdpt) >> 8);

	// read the next packet pointer
	enc28j60_rxrdpt = enc28j60_read_op(ENC28J60_SPI_RBM, 0); // read low byte
	enc28j60_rxrdpt |= enc28j60_read_op(ENC28J60_SPI_RBM, 0) << 8;	// read high byte


	// Move the RX read pointer to the start of the next received packet
	// This frees the memory without reading it
	enc28j60_wcr(ERXRDPTL, enc28j60_rxrdpt);
	enc28j60_wcr(ERXRDPTH, (enc28j60_rxrdpt) >> 8);

	// decrement the packet counter indicate we are done with this packet
    enc28j60_write_op(ENC28J60_SPI_BFS, ECON2, ECON2_PKTDEC);
}
