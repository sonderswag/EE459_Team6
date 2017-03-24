//
//  RMF69.c
//  transReciever
//
//  Created by Christian Wagner on 3/9/17.
//  Copyright © 2017 Christian Wagner. All rights reserved.
//

#include <avr/interrupt.h>

#include <stdio.h>
#include "SPI_control.h"
#include "DigitalIo.h"
#include "RFM69.h"

//radio reset pin
#define RF_reset_pin 11 

// The crystal oscillator frequency of the RF69 module
#define RH_RF69_FXOSC 32000000.0

// The Frequency Synthesizer step = RH_RF69_FXOSC / 2^^19
#define RH_RF69_FSTEP  (RH_RF69_FXOSC / 524288)

#define Max_Message_length 60


void RFM_init(char cs)
{
		// this is manulally resetting the transreciever 
	pinMode(RF_reset_pin,1); 
	digitalWrite(25,1);
	_delay_ms(100);
	digitalWrite(RF_reset_pin,0);
	_delay_ms(100);


	// Configure important RH_RF69 registers
	// defaults to fixed packet format 
    // Here we set up the standard packet format for use by the RH_RF69 library:
    // 4 bytes preamble
    // 2 SYNC words 2d, d4
    // 2 CRC CCITT octets computed on the header, length and data (this in the modem config data)
    // 0 to 60 bytes data
    // RSSI Threshold -114dBm
    // We dont use the RH_RF69s address filtering: instead we prepend our own headers to the beginning
    // of the RH_RF69 payload

	// RH_RF69_REG_3C_FIFOTHRESH : 0x3c == Tx start 
	// RH_RF69_FIFOTHRESH_TXSTARTCONDITION_NOTEMPTY : 0x80  setting threshold for fif0 to 0x8f as recommended 
    RFM_writeReg(RH_RF69_REG_3C_FIFOTHRESH, RH_RF69_FIFOTHRESH_TXSTARTCONDITION_NOTEMPTY | 0x0f, cs); // thresh 15 is default

    RFM_writeReg(RH_RF69_REG_6F_TESTDAGC, RH_RF69_TESTDAGC_CONTINUOUSDAGC_IMPROVED_LOWBETAOFF, cs);

    RFM_writeReg(RH_RF69_REG_5A_TESTPA1, RH_RF69_TESTPA1_NORMAL, cs);
    RFM_writeReg(RH_RF69_REG_5C_TESTPA2, RH_RF69_TESTPA2_NORMAL, cs);

    char syncwords[] = {0x2d, 0x4d};
    RFM_setSyncWords(syncwords, cs);

    RHFM_setPreambleLength(4,cs); 

    RFM_setFrequency(434.0, cs);

}

char Read_FIFO(char* buffer, char cs)
{
	cli();
	digitalWrite(cs, 0); 
	SPI_transfer(RH_RF69_REG_00_FIFO);

	char payload = SPI_transfer(0); //get length of bytes 
	int buf_len = 0 ; 

	if (payload != 0)
	{
		for (buf_len = 0 ; buf_len <payload ; buf_len++)
		{
			buffer[buf_len] = SPI_transfer(0); 
		}
	}

	digitalWrite(cs, 1); 
	sei(); 

	return payload; // the length of the message 
}

void RFM_send(char* data, char* currentMode, char length, char cs)
{
	if (length > Max_Message_length)
	{
		return ; 
	}

	cli(); 
	digitalWrite(cs, 0); 
	char message[2] = {RH_RF69_REG_00_FIFO | RH_SPI_WRITE_MASK, length};
	SPI_multiWrite(message,2);

	while(length--)
	{
		SPI_transfer(*data++); 
	}
	digitalWrite(cs, 1); 
	sei();

	RFM_setModeTx(currentMode,cs);
}

void RFM_spiConfig(char cs) 
{
	pinMode(cs, OUTPUT); 
	cli(); // stopping interrupts 
	
	// spi values corresponding to datasheet 
	SPI_setDataMode(SPI_MODE0); // setting the polarity of SPI 
	SPI_setBitOrder(1); //want MSB first 
	SPI_setClockDivider(SPI_CLOCK_DIV4);

	sei(); // starting interrupts 
}

void RFM_unselect(char slaveSelectPin)
{
	digitalWrite(slaveSelectPin,1); // turning off chip select 

	// will need to readjust SPI variables for screen or have a select for the screen to select the right values 

}

/* chip select must be on for this deivce 
	
	so would want to call 
	RFM_select(cs);
	RFM_wrtieREG 

*/ 

// write a single byte to a given register 
void RFM_writeReg(char address, char data, char cs)
{
	cli(); 
	//MSB == 1 for write it is 0 for read 
	// next 7 bits are address to write to 

	digitalWrite(cs, 0); 

	char message[2] = {address | RH_SPI_WRITE_MASK, data };

	address |= RH_SPI_WRITE_MASK; // putting 1 in MSB 
	SPI_multiWrite(message,2);
	digitalWrite(cs, 1); 
	sei(); 
}


// read a single byte for a given register 
char RFM_readReg(char address, char cs)
{
	cli(); 
	digitalWrite(cs, 0);
	address &= ~RH_SPI_WRITE_MASK; // putting 0 in MSB

	char message[] = {address, 0x00};
	SPI_multiTransfer(message,2); 

	digitalWrite(cs, 1);
	sei(); 
	return message[1] ; 
}



/* Write multiple bytes to radio 
address :: address to write multiple bytes (0x00 is fifo )
src :: data to be written 
len :: number of bytes to be writen 
radio :: holding the slave select pi 
*/ 
void RFM_burstWrtie(char address, char* src, char len, char cs)
{
	cli(); 
	digitalWrite(cs, 0);
	SPI_transfer(address |= RH_SPI_WRITE_MASK); // putting 1 in MSB )
	SPI_multiTransfer(src,len);
	digitalWrite(cs, 1);
	sei(); 
}


void RFM_burstRead(char address, char* dest, char len, char cs)
{
	cli(); 
	digitalWrite(cs, 0);
	SPI_transfer(address |= RH_SPI_WRITE_MASK); // putting 1 in MSB )

	while(len--) 
	{
		*dest++ = SPI_transfer(0x00); // reading the FIFO 
	}

	digitalWrite(cs, 1);
	sei(); 
}

void RFM_setSyncWords(char* syncwords, char cs)
{
	// restricting number of sync words to 2 for now 
	// getting the current syncConfig
	// default number of sync words is 2 
	// syncwords is on by default  

	// currently not changing any of the default values 
	// char synConfig = RFM_read(RH_RF69_REG_2E_SYNCCONFIG,cs) ; 

	// setting the sync words 
	RFM_writeReg(0x2f,syncwords[0],cs);
	RFM_writeReg(0x30,syncwords[1],cs);

	
}

void RHFM_setPreambleLength(uint16_t bytes, char cs)
{
    RFM_writeReg(RH_RF69_REG_2C_PREAMBLEMSB, bytes >> 8, cs);
    RFM_writeReg(RH_RF69_REG_2D_PREAMBLELSB, bytes & 0xff,cs);
}

void RFM_setFrequency(float centre, char cs )
{

	uint32_t frf = (uint32_t)((centre * 1000000.0) / RH_RF69_FSTEP);
	RFM_writeReg(RH_RF69_REG_07_FRFMSB, (frf >> 16) & 0xff, cs);
    RFM_writeReg(RH_RF69_REG_08_FRFMID, (frf >> 8) & 0xff,cs);
    RFM_writeReg(RH_RF69_REG_09_FRFLSB, frf & 0xff,cs);

}

/*	Modes of operation 
000 → Sleep mode (SLEEP) 			   :: RH_RF69_OPMODE_MODE_SLEEP  
001 → Standby mode (STDBY) 			   :: RH_RF69_OPMODE_MODE_STDBY  
010 → Frequency Synthesizer mode (FS)  :: RH_RF69_OPMODE_MODE_FS 
011 → Transmitter mode (TX) 		   :: RH_RF69_OPMODE_MODE_TX   
100 → Receiver mode (RX) 			   :: RH_RF69_OPMODE_MODE_RX   
*/ 

void RFM_setOpMode(char mode, char cs)
{
    char opmode = RFM_readReg(RH_RF69_REG_01_OPMODE, cs); // access 0x01 register which holds operation mode 
    opmode &= ~RH_RF69_OPMODE_MODE; // setting bits 4-2 to zero 
    opmode |= (mode & RH_RF69_OPMODE_MODE); // setting bits 4-2 to the mode we want 
    RFM_writeReg(RH_RF69_REG_01_OPMODE, opmode, cs);

    // Wait for mode to change. this could cause problems 
    // while (!(RFM_readReg(RH_RF69_REG_27_IRQFLAGS1,cs) & RH_RF69_IRQFLAGS1_MODEREADY));
	
}

void RFM_setModeRx(char* currentMode, char cs)
{
	if (*currentMode == 1 ) // checking to see if it is already in rx mode 
	{
		return ; 
	}

	RFM_writeReg(RH_RF69_REG_5A_TESTPA1, RH_RF69_TESTPA1_BOOST, cs); // used to boost power to transmitter / reciever 
	RFM_writeReg(RH_RF69_REG_5C_TESTPA2, RH_RF69_TESTPA2_BOOST, cs); 

	RFM_setOpMode(RH_RF69_OPMODE_MODE_RX,cs); 

	*currentMode = 1 ; 

}

void RFM_setModeTx(char* currentMode, char cs)
{
	if (*currentMode == 2) // 
	{
		return ; 
	}

	RFM_writeReg(RH_RF69_REG_5A_TESTPA1, RH_RF69_TESTPA1_BOOST, cs); // used to boost power to transmitter / reciever 
	RFM_writeReg(RH_RF69_REG_5C_TESTPA2, RH_RF69_TESTPA2_BOOST, cs); 

	RFM_setOpMode(RH_RF69_OPMODE_MODE_TX,cs); 

	*currentMode = 2; 
}

void RFM_setModeIdle(char* currentMode, char cs)
{
	if (*currentMode == 0) // idle 
	{
		return ; 
	}

	*currentMode = 0; 

	RFM_setOpMode(RH_RF69_OPMODE_MODE_STDBY,cs);

}


