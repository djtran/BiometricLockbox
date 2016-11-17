#include "msp430.h"
#include "LCD.h"
#include "Keypad.h"
#include "GPIOComms.h"


int main(void)
{
	//stuff
	WDTCTL = WDTPW + WDTHOLD;
	WDTCTL = WDTPW + WDTTMSEL + WDTCNTCL;
//	IE1 |= WDTIE;

	setupLCD_UART();	//1 MHz base clock, 9600 baud rate, etc etc.
	setupKeypad();
	setIOComms();

	//////////////////
	//	 CLR Key	//
	//////////////////
	P1REN |= BIT3;
	P1OUT |= BIT3;

	P1IE |= BIT3;
	P1IES |= BIT3;	//hi lo edge
	P1IFG = 0;

	writeLCD("DS Lockbox v1.0 ", 16);

	_bis_SR_register(GIE+LPM0_bits); // Enter LPM0 w/ int until Byte RXed

}


