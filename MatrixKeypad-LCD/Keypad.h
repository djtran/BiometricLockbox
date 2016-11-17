/*
 * Keypad.h
 *
 *  Created on: Apr 24, 2016
 *      Author: David
 */

#ifndef KEYPAD_H_
#define KEYPAD_H_

 void setupKeypad();								//Sets up the pins and interrupt needed for interpreting the keypad.
 char readPorts();									//reads pins and returns the symbol shown on the button pressed.
 char genChar(char read, char pressNum);			//Returns an ASCII character that matches the key pressed and the number of times pressed

 void FPstate();									//Signal to leave state 3;


#endif /* KEYPAD_H_ */
