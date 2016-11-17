/*
 * GPIOComms.h
 *
 *  Created on: Apr 25, 2016
 *      Author: David
 */

#ifndef GPIOCOMMS_H_
#define GPIOCOMMS_H_

void setIOComms();	//Setup pins
void sendUnlock();	//send unlock code
void sendEnroll();	//send enroll code
void sendFPUnlock();//send FP scan request code
void checkRX();		//Cosmetic LCD feedback purposes


#endif /* GPIOCOMMS_H_ */
