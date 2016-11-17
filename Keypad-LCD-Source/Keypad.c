#include "msp430.h"
#include "LCD.h"
#include "GPIOComms.h"

/*
 * Keypad.c
 *
 *  Created on: Apr 24, 2016
 *      Author: David
 */

/*		MATRIX KEYPAD TO PIN MAPPINGS
 *
 *	COLUMNS WILL BE HIGH INPUTS- PULLED LOW BY BUTTON PRESS
 *
 * 		P2.0	P2.1	P2.2
 *
 *
 * 		1		2		3			P2.3			ROWS ARE LOW OUTPUTS- PULL THE COLUMNS LOW ON PRESS
 *
 *
 * 		4		5		6			P2.4
 *
 *
 * 		7		8		9			P2.5
 *
 *
 * 		*		0		#			P1.7
 *
 *
 * 		LIST OF FUNCTIONS:
 *
 * 		void setupKeypad();								//Sets up the pins and interrupt needed for interpreting the keypad.
 * 		char readPorts();								//reads pins and returns the symbol shown on the button pressed.
 * 		char genChar(char read, char pressNum);			//Returns an ASCII character that matches the key pressed and the number of times pressed
 *
 * 		//PORT 2 State machine for operating the keypad.
 *
 */

char botRow = 0;	//Use as boolean. Check P1.7 and write to here.

char state = 0;		//State machine
char index = 0;
char count = 0;


char Port1In = 0;	//Save snapshots of the port inputs
char Port2In = 0;

char read;			//Key pressed
char currentKey;
char pressNum;		//# of times pressed, translates to char shown. Ex: press '2' 3 times shows C

char password[4] = {'4','0','9','6'};	//2^12
char Code[3] = "256";
char ClrCode[3] = "111";
char codeIndex = 0;

void setupKeypad()
{
	//PULLUP RESISTER FOR BUTTONS//
	P1DIR |= BIT7;
	P1OUT &= ~BIT7;

	P2DIR &= ~(BIT0+BIT1+BIT2); 		// all needs to be input
	P2REN |= (BIT0+BIT1+BIT2);			// Enable internal pullups
	P2OUT |= (BIT0+BIT1+BIT2);			// Make pull resister a pullup instead of pulldown
	//
	P2DIR |= BIT3 + BIT4 + BIT5;	//loljk Bits 3-5 are rows, will be output low.
	P2OUT &= ~(BIT3 + BIT4 + BIT5);

	P2IE |= BIT0 + BIT1 + BIT2;	//Entire Port 2 triggers interrupt on lo/hi edge
	P2IES |= BIT0 + BIT1 + BIT2;	//now it's hi/lo edge bitcheees
	P2IFG = 0;
}

char readPorts()	//Return the key pressed
{
	char ColDown;

	Port2In = P2IFG;
	//Save snapshots of inputs in memory.
	if(P2IFG & BIT0)	//if both bits are low
	{
		ColDown = BIT0;
	}
	else if(P2IFG & BIT1)
	{
		ColDown = BIT1;
	}
	else if(P2IFG & BIT2)
	{
		ColDown = BIT2;
	}


	//Check bottom Row
	P2OUT |= BIT3 + BIT4 + BIT5;
	if(~P2IN & ColDown)	//if column bit still low
	{
		switch(ColDown)
		{
		case BIT0:
			return '*';
		case BIT1:
			return '0';
		case BIT2:
			return '#';
		}
	}
	//Top row
	P2OUT &= ~BIT3;
	P1OUT |= BIT7;
	if(~P2IN & ColDown)
	{
		switch(ColDown)
		{
		case BIT0:
			return '1';
		case BIT1:
			return '2';
		case BIT2:
			return '3';
		}
	}
	//Mid row
	P2OUT &= ~BIT4;
	P2OUT |= BIT3;
	if(~P2IN & ColDown)
	{
		switch(ColDown)
		{
		case BIT0:
			return '4';
		case BIT1:
			return '5';
		case BIT2:
			return '6';
		}
	}

	//Bot row
	P2OUT &= ~BIT5;
	P2OUT |= BIT4;
	if(~P2IN & ColDown)
	{
		switch(ColDown)
		{
		case BIT0:
			return '7';
		case BIT1:
			return '8';
		case BIT2:
			return '9';
		}
	}
	return 'F';	//Should never reach here. F for fail.
}

void FPstate()
{
	state = 0;
	return;
}

#pragma vector = PORT2_VECTOR
__interrupt void StateMachine()
{
	read = readPorts();	//Take in the read char

	P2OUT &= ~(BIT3 + BIT4 + BIT5);
	P1OUT &= ~BIT7;

	moveCursor(1);

	switch(state)
	{
	//////////////////////////
	//		IDLE STATE		//	* -> Add user, requires PW		# -> PW to unlock	Combination(420) -> Change default PW
	//////////////////////////
	case 0:
		if(read == '*')				//ADD USER -> Requires PW
		{
			state = 2;
			index = 0;
			count = 1;
			clearLCD();
			writeLCD("ENTER PW:       ",16);
		}
		else if(read == '#')		//PASS UNLOCK
		{
			state = 1;
			count = 1;
			clearLCD();
			writeLCD("ENTER PW:       ",16);
		}
		else if(read == '0')
		{
			//Signal MST to authenticate fingerprint
			IE1 |= WDTIE;
			sendFPUnlock();
			state = 3;	//Blank state - no inputs allowed
		}
		else
		{
			writeLCD("DS Lockbox v1.0 ", 16);

			//CHANGE PASSWORD.
			if(read == Code[index])
			{
				codeIndex = 0;
				index++;
				if(index == 3)		//420 input successfully
				{
					state = 25;
					index = 0;
					clearLCD();
					writeLCD("CHANGE PW:      ", 16);
				}
			}
			else if(read == ClrCode[codeIndex])
			{
				index = 0;
				codeIndex++;
				if(codeIndex == 3)
				{
					codeIndex = 0;
					clearLCD();
				}
			}

			else
			{
				index = 0;
				codeIndex = 0;
			}
		}
		break;

	//////////////////////////
	//		PASS UNLOCK		//
	//////////////////////////
	case 1:
		if(count == 5 && index < count)	//Too many entries incorrect
		{
			state = 0;		//INVALID msg
			writeLCD("INVALID", 7);
			index = 0;
			count = 0;
			break;
		}
		if(read == password[index])
		{
			index++;
			if(index == 4)
			{
				state = 0;
				index = 0;
				//Unlock the box.
				sendUnlock();
				state = 3;

				break;
			}
		}
		else
		{

		}
		count++;
		break;

	//////////////////////////
	//		ADD USER - PW	//
	//////////////////////////
	case 2:
		if(count == 5 && count > index)
		{
			state = 0;		//INVALID msg
			writeLCD("INVALID.        ", 16);
			index = 0;
			count = 0;
			break;
		}
		if(read == password[index])
		{
			index++;
			if(index == 4)
			{
				state++;				//Blank state - no inputs allowed until msg received from FP scan. -- After fingerprints received in state 3, state = 0
				index = 0;
				writeLCD("ENROLL FINGER    ", 16);
				sendEnroll();
			}
		}
		count++;
		break;

	//////////////////////////
	//		ADD USER - FP	//
	//////////////////////////
	//Fingerprints pls
	//Enroll 3, save, state = 0;


	//////////////////////////
	//		CHANGE PW		//
	//////////////////////////
	case 25:
		//Length 4 password
		password[index] = read;
		index++;
		if(index == 4)
		{
			writeLCD("Success!        ", 16);
			state = 0;
			index = 0;
			count = 0;
		}
		writeLCD(&read, 1);
		break;

	//////////////////////////
	//		SUCCESS MSG		//
	//////////////////////////
	case 40:
		//Display Success message;

		clearLCD();
		writeLCD("Success!        ", 16);

		state = 0;
		break;

	//////////////////////////
	//		BAD FP/PW		//	Output "INVALID"
	//////////////////////////
	case 41:
		//Display INVALID message

		clearLCD();
		writeLCD("INVALID.        ", 16);

		state = 0;
		break;

	default:
		break;

	}


	//Clear IFG when done
	P2IFG = 0;
}
