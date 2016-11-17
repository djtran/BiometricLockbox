#include "msp430.h"
/*
 * LCD.c
 *
 *  Created on: Apr 24, 2016
 *      Author: David
 */

/*	List of functions:
 *
 * 	void setupLCD_UART() - Sets 1 MHz base clock, USCI A0 settings including 1MHz 9600 baud rate
 *
 * 	void clearLCD()					-> clears the LCD screen
 * 	void moveCursor(char pos) 		-> moves the cursor to a pos between 1 - 32 (16 char lines, 2 rows)
 * 	void writeLCD(char* word, int wordLength) 	-> writes to the screen
 *
 */

char* LCDPhrase[16];
int LCDIt = 0;
int LCDwordLength = 0;

int LCDEndPhrase = 0;
int LCDCount = 0;

int wdtcount = 0;
int wdtstate = 0;

void setupLCD_UART()	//9600 baud
{
	DCOCTL = 0; // Select lowest DCOx and MODx settings
	BCSCTL1 = CALBC1_1MHZ; // Set DCO
	DCOCTL = CALDCO_1MHZ;

	P1SEL |= BIT1 + BIT2; // P1.1 = RXD, P1.2=TXD
	P1SEL2 |= BIT1 + BIT2; // P1.1 = RXD, P1.2=TXD

	UCA0CTL1 = UCSWRST;
	UCA0CTL1 |= UCSSEL_2; // SMCLK
	UCA0BR0 = 104; // 1MHz 9600 (Divider 104)
	UCA0BR1 = 0; // 1MHz 9600
	UCA0MCTL = UCBRS0; // Modulation UCBRSx = 1
					  // To set UCA0MCTL you += it with the bits UCBRS0, UCBRS1, etc, where the bits make up the desired binary number UCBRSx
	UCA0CTL1 &= ~UCSWRST; // **Initialize USCI state machine**

	return;
}

void moveCursor(char pos)	//2 lines of 16 chars, send 1 thru 32 to place cursor in that position.
{
	while (!(IFG2 & UCA0TXIFG));
	LCDIt = 0;
	LCDwordLength = 2;
	LCDEndPhrase = 2;

	if(pos > 1 && pos <= 16)
	{
		LCDPhrase[0] = 254;
		LCDPhrase[1] =  127 + pos;
	}
	else if(pos > 16 && pos < 32)
	{
		LCDPhrase[0] = 254;
		LCDPhrase[1] =  191 + pos;
	}
	else
	{
		return;
	}

	UC0IE|= UCA0TXIE;

}

void writeLCD(char* word, int wordLength)//Send a line to the LCD.
{
	while (!(IFG2 & UCA0TXIFG));
	//LCD writes to first line, then to second line, then loops back to first line.
	int i = 0;

	for(i;i<wordLength;i++)
	{
		LCDPhrase[i] = word[i];
	}
	LCDIt = 0;
	LCDCount = 0;
	LCDwordLength = wordLength;
	LCDEndPhrase = wordLength;
	UC0IE |= UCA0TXIE; // Enable USCI_A0 TX interrupt
	return;

}

void clearLCD()
{
	while (!(IFG2 & UCA0TXIFG));
	LCDPhrase[0] = 254;
	LCDPhrase[1] = 1;
	LCDIt = 0;
	LCDCount = 0;
	LCDwordLength = 2;
	LCDEndPhrase = 2;
	UC0IE |= UCA0TXIE;
	return;
}


//////////////////////////
//	Output to Screen	//
//////////////////////////
#pragma vector=USCIAB0TX_VECTOR
__interrupt void USCI0TX_ISR(void)
{
	//Wake up the watchdog
	wdtcount = 0;
	IE2 |= WDTIE;

	UCA0TXBUF = LCDPhrase[LCDIt++]; // TX next character
	LCDCount++;
	if(LCDCount >= LCDEndPhrase)
	{
		UC0IE &= ~UCA0TXIE;
		LCDEndPhrase = 16;


	}
	else if(LCDIt > LCDwordLength)
	{
		LCDIt = 0;
	}
}

//////////////////////////
//		WDT Handler		//
//////////////////////////
#pragma vector = WDT_VECTOR
__interrupt void WDTHandles(void)
{
	wdtcount++;

	if(wdtcount > 350)
	{
		clearLCD();
		moveCursor(1);
		IE2 &= ~WDTIE;
	}

	switch(wdtstate)
	{
	case 1:

		wdtstate = 0;
		break;

	case 2:

		wdtstate = 0;
		break;


	default:
		//do nothing
	}

}
