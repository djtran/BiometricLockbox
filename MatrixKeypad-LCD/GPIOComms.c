#include "msp430.h"
#include "LCD.h"
#include "Keypad.h"
/*
 * GPIOComms.c
 *
 *  Created on: Apr 25, 2016
 *      Author: David
 */

void setIOComms()
{
	P1DIR |= BIT5 + BIT6;		//Outputs			BIT5 = MSB, BIT6 = LSB
	P1OUT &= ~(BIT5 + BIT6);	//Default Off

	P1IE |= BIT0 + BIT4;		//Lo Hi edge		BIT0 = MSB, BIT4 = LSB
	P1IFG = 0;	//Clear flags
	return;
}

void sendUnlock()	//PW unlock - 01
{
	P1OUT |= BIT6;
	return;
}

void sendEnroll()	//Enroll FP	- 10
{
	P1OUT |= BIT5;
	return;
}

void sendFPUnlock()	//FP Unlock Request - 11
{
	P1OUT |= BIT5 + BIT6;
	P1IFG = 0;
	return;
}

void checkRX()
{
	char check = P1IFG;		//never 11

	if(check & BIT4)		//01
	{
		//good
		writeLCD("Success!        ", 16);
	}
	else if(check & BIT0)	//10
	{
		//bad
		writeLCD("INVALID.        ",16);
	}

	FPstate();	//return from state 3;
	return;
}

// Port 1 interrupt service routine
#pragma vector=PORT1_VECTOR
__interrupt void Port_1_Button(void)
{
	if(P1IFG & BIT3)
	{
		clearLCD();
		P1IFG = 0;
	}
	else
	{
		//You're in state 3 and you got a message.
		checkRX();					//receive msg & change your state
		P1OUT &= ~(BIT5 + BIT6);	//turn off signal pins

		P1IFG = 0;	//Be free like a bird and live your life

	}
}

