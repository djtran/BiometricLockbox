/*
 * LCD.h
 *
 *  Created on: Apr 24, 2016
 *      Author: David
 */

#ifndef LCD_H_
#define LCD_H_

void setupLCD_UART(); 							// Sets 1 MHz base clock, USCI A0 settings including 1MHz 9600 baud rate

void clearLCD();								// clears the LCD screen
void moveCursor(char pos); 						// moves the cursor to a pos between 1 - 32 (16 char lines, 2 rows)
void writeLCD(char* word, int wordLength);		// writes to the screen


#endif /* LCD_H_ */
