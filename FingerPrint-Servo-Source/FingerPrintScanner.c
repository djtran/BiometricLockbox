#include <msp430.h> 

// Note: On the MSP430, the hole TP1 (next to the USB connector) has 5V out

/* Notes:
 * During an interrupt handler, in general all other interrupts are disabled in the status register
 * unsigned int are 2 bytes currently
 *
 */

// Command Packet
// [0x55] [0xAA] [0x0001] [PARAMETER] [COMMAND] [CHECKSUM]
// For the 'Open' Command:
// Parameter = 0x00000000 (DWORD)
// Command == 0x0001
// CheckSum = SUM(OFFSET[i]) for i = 0...9

// On the Fingerprint Scanner:
// |> TX RX GND Vin(5V)
// Where TX is connected to P1.1, RX is connected to P1.2

// Currently we have the 'Open' Command working in the form where parameter = 1 (you've requested the additional data packet)
// So you have to also eat the additional data packet out of the RXBUF every time you open.

// Receive Packet
// [0x55] [0xAA] [0x0001] [0x30] [0x30] [CHECKSUM]

// Note: Little Endian
// 2Byte Word

#include "msp430g2553.h"
#include "GPIOComms.h"

#define LED_RED BIT0 //red led
#define LED_GRN BIT6 //green led
#define TXD BIT2
#define RXD BIT1
#define BUTTON BIT3




// The button is "low" when pressed down
#define BUTTON_PRESSED 0

unsigned int i; //Counter for tx
unsigned int j; //Counter for rx

// flag for if the response packet has been successfully received
unsigned int response_read = 0;

// flag for if you entered a state for the first time
unsigned int first_time = 1;

// number of times you've waited for the finger to be pressed
unsigned int finger_wait_count = 0;

// id of fingerprint to be enrolled
unsigned int finger_id = 0;

// which iteration of enroll (1, 2, or 3) you're on
unsigned int enrollment_number = 0;

#define S_IDLE 99
#define S_ENROLL_START 0
#define S_LED_ON 1
#define S_CAPTURE_FINGER 2
#define S_LED_OFF 3
#define S_ENROLL_N 4
#define S_IS_PRESS_FINGER_ON_ENROLL 5
#define S_IS_PRESS_FINGER_OFF_ENROLL 6

#define S_VERIFY 7
#define S_IS_PRESS_FINGER_ON_VERIFY 8
#define S_IS_PRESS_FINGER_OFF_VERIFY 9
#define S_CLEAR_FINGER 10







unsigned int state = S_CLEAR_FINGER;
unsigned int next_state = S_ENROLL_START;

#define CMD_LED 0x12
#define CMD_ENROLL_START 0x22
#define CMD_ENROLL_1 0x23
#define CMD_ENROLL_2 0x24
#define CMD_ENROLL_3 0x25
#define CMD_IS_PRESS_FINGER 0x26
#define CMD_VERIFY 0x50
#define CMD_IDENTIFY 0x51
#define CMD_CAPTURE_FINGER 0x60
#define CMD_CLEAR_FINGER 0x41


#define PARAM_NONE 0x00

// parameters for LED control
#define PARAM_LED_ON 0x01
#define PARAM_LED_OFF 0x00

// parameters for capture finger
#define PARAM_CAPTURE_FAST 0x00
#define PARAM_CAPTURE_SLOW 0x01

volatile unsigned char last_button;

// Arrays which store the command and response packets for UART
char tx_cmd[12];
char rx_response[15];

// trace of states iterated through
char state_list[20];
unsigned int state_dex = 0;
char error_buf[3][12];

//char data[502] = {0x00};

//p1.0 LSB p1.4MSB are inputs
//p1.5 SUCCESS p1.7 INVALID are outputs

int verify=0;
unsigned int lock=750;

void init_GPIO(){

		P1DIR |= BIT5+BIT7; // pullup
		P1OUT&=~(BIT5+BIT7);
		P1IFG &= ~(BIT0+BIT4);// clear interrupt flag
		P1IE  |= BIT0+BIT4; // enable interrupt
		P1IES=0;

}
char save ;
void interrupt button_handler(){
	save = P1IN;

	if((save &(BIT0 + BIT4))==(BIT0+BIT4)){//FINGER PRINT UNLOCK REQUEST
		state = S_LED_ON;
		next_state = S_IS_PRESS_FINGER_ON_VERIFY;
		first_time = 1;


		}
	else if(save & BIT0){//UNLOCK BOX
					P1OUT^=LED_GRN;
					checkLock();
					sendSuccess();
				 	}
	else if(save & BIT4){//ENROLL FINGERPRINT
		state=S_LED_OFF;
		next_state=S_ENROLL_START;
		}
	P1IFG = 0;
	return;

}
ISR_VECTOR(button_handler,".int02"); // declare interrupt vector

void sendSuccess(){
	P1OUT|=BIT5;
	P1OUT&=~BIT5;

}
void sendReject(){
	P1OUT|=BIT7;
	P1OUT&=~BIT7;
}



void checkLock(){
	if(lock==2000){
	TA1CCR1 = 750;
	lock=750;
	}
	else if(lock==750){
		TA1CCR1 = 2000;
		lock=2000;
	}
}

void build_command_packet(unsigned int command, unsigned int param){
	if(state_dex < 20)
		state_list[state_dex++] = state;

	int a;
	for(a = 0; a < 12; a++)
		rx_response[a] = 0x00;

	//header
	tx_cmd[0] = 0x55;
	tx_cmd[1] = 0xAA;

	//id
	tx_cmd[2] = 0x01;
	tx_cmd[3] = 0x00;

	//param
	tx_cmd[4] = (param & 0x00FF);
	tx_cmd[5] = (param & 0xFF00) >> 8;
	tx_cmd[6] = 0x00;
	tx_cmd[7] = 0x00;

	//command
	tx_cmd[8] = (command & 0x00FF);
	tx_cmd[9] = (command & 0xFF00) >> 8;

	unsigned int checksum = 0;
	unsigned int temp = 0;
	for(temp = 0; temp < 10; temp++){
		checksum += tx_cmd[temp];
	}

	//checksum
	tx_cmd[10] = (checksum & 0x00FF);
	tx_cmd[11] = (checksum & 0xFF00) >> 8;

	i = 0;
	UC0IE |= UCA0TXIE; // Enable USCI_A0 TX interrupt
	j = 0;
	UC0IE |= UCA0RXIE; // Enable USCI_A0 RX interrupt
	response_read = 0;
}


// ===== Watchdog Timer Interrupt Handler =====
// This event handler is called to handle the watchdog timer interrupt,
//    which is occurring regularly at intervals of about 8K/1.1MHz ~= 7.4ms.

interrupt void WDT_interval_handler(){
	switch(state){
	case S_IDLE:
		//build_command_packet(CMD_LED, PARAM_LED_OFF);
//		P1OUT |= LED_GRN;
//		unsigned char button;
//		button = (P1IN & BUTTON);  // read the BUTTON bit
//		if(button == BUTTON_PRESSED){
//			state = S_LED_ON;
//			next_state = S_IS_PRESS_FINGER_ON_VERIFY;
//			first_time = 1;
//			P1OUT &= ~(LED_GRN + LED_RED);
//		}

		break;
	case S_ENROLL_START:
		if(first_time){
			//P1OUT |= LED_RED;
			build_command_packet(CMD_ENROLL_START, finger_id);
			response_read = 0;
			first_time = 0;
		}else if(response_read){
			if(rx_response[8] == 0x30){
				state = S_LED_ON;
				next_state = S_IS_PRESS_FINGER_ON_ENROLL;
				first_time = 1;
			}
		}
		break;
	case S_LED_ON:
		if(first_time){
			build_command_packet(CMD_LED, PARAM_LED_ON);
			response_read = 0;
			first_time = 0;
		}else if(response_read){
			if(rx_response[8] == 0x30){
				state = next_state;
				first_time = 1;
			}
		}
		break;
	case S_IS_PRESS_FINGER_ON_ENROLL:
		if(first_time){
			build_command_packet(CMD_IS_PRESS_FINGER, PARAM_NONE);
			response_read = 0;
			first_time = 0;
		}else if(response_read){
			if(rx_response[8] == 0x30){
				if(rx_response[4] != 0x00){//finger not pressed
					first_time = 1;
				}else{
					state = S_CAPTURE_FINGER;
					next_state = S_ENROLL_N;
					first_time = 1;
				}
			}
		}
		break;
	case S_CAPTURE_FINGER:
		if(first_time){
			build_command_packet(CMD_CAPTURE_FINGER, PARAM_CAPTURE_SLOW);
			response_read = 0;
			first_time = 0;
		}else if(response_read){
			if(rx_response[8] == 0x30){
				state = next_state;
				first_time = 1;
			}else{
				first_time = 1;//repeat capture until it succeeds
			}
		}
		break;
	case S_LED_OFF:
		if(first_time){
			build_command_packet(CMD_LED, PARAM_LED_OFF);
			response_read = 0;
			first_time = 0;
		}else if(response_read){
			if(rx_response[8] == 0x30){
				state = next_state;
				first_time = 1;
			}
		}
		break;
	case S_ENROLL_N:
		if(first_time){
			build_command_packet(CMD_ENROLL_1 + enrollment_number, PARAM_NONE);
			response_read = 0;
			first_time = 0;
		}else if(response_read){
			if(rx_response[8] == 0x30){
				if(enrollment_number == 2){
					state = S_LED_OFF;
					next_state = S_IDLE; //After the third enrollment (counting from 0), you're done
					sendSuccess();
					finger_id++;
					first_time = 1;
				}else{
					state = S_LED_OFF;
					next_state = S_IS_PRESS_FINGER_OFF_ENROLL;
					enrollment_number++;
					first_time = 1;
				}
			}else{// if enrollment fails, restart the process
				first_time = 1;
				enrollment_number = 0;
				state = S_LED_OFF;
				next_state = S_ENROLL_START;
			}
		}
		break;
	case S_IS_PRESS_FINGER_OFF_ENROLL:
		if(first_time){
			build_command_packet(CMD_IS_PRESS_FINGER, PARAM_NONE);
			response_read = 0;
			first_time = 0;
		}else if(response_read){
			if(rx_response[8] == 0x30){
				if(rx_response[4] == 0x00 && rx_response[5] == 0x00){//finger pressed
					first_time = 1;
				}else{
					state = S_LED_ON;
					next_state = S_IS_PRESS_FINGER_ON_ENROLL;
					first_time = 1;
				}
			}
		}
		break;
	case S_IS_PRESS_FINGER_ON_VERIFY:
		if(first_time){
			build_command_packet(CMD_IS_PRESS_FINGER, PARAM_NONE);
			response_read = 0;
			first_time = 0;
		}else if(response_read){
			if(rx_response[8] == 0x30){
				if(rx_response[4] != 0x00){//finger not pressed
					first_time = 1;
				}else{
					state = S_CAPTURE_FINGER;
					next_state = S_VERIFY;
					first_time = 1;
				}
			}
		}
		break;
	case S_VERIFY:
		if(first_time){
			build_command_packet(CMD_IDENTIFY, PARAM_NONE);
			response_read = 0;
			first_time = 0;
		}else if(response_read){
			if(rx_response[8] == 0x30){// finger identified
				//P1OUT |= LED_GRN;
				//build_command_packet(CMD_LED, PARAM_LED_OFF);
				sendSuccess();
				checkLock();
				state=S_LED_OFF;
				next_state=S_IDLE;
			}else{// finger rejected
				P1OUT ^= LED_GRN;
				 sendReject();
				state=S_LED_OFF;
				next_state=S_IDLE;

			}
			//state = S_IS_PRESS_FINGER_OFF_VERIFY;
			first_time = 1;
		}
		break;
	case S_CLEAR_FINGER:
		build_command_packet(CMD_CLEAR_FINGER, PARAM_NONE);
		state=S_IDLE;
		break;
	case S_IS_PRESS_FINGER_OFF_VERIFY:
		if(first_time){
			build_command_packet(CMD_IS_PRESS_FINGER, PARAM_NONE);
			response_read = 0;
			first_time = 0;
		}else if(response_read){
			if(rx_response[8] == 0x30){
				if(rx_response[4] == 0x00 && rx_response[5] == 0x00){//finger pressed
					first_time = 1;
				}else{
					P1OUT &= ~(LED_GRN + LED_RED);
					state = S_LED_ON;
					next_state = S_IS_PRESS_FINGER_ON_VERIFY;
					first_time = 1;
				}
			}
		}
	}
}

// DECLARE function WDT_interval_handler as handler for interrupt 10
// using a macro defined in the msp430g2553.h include file
ISR_VECTOR(WDT_interval_handler, ".int10")

int main(void)
{
	WDTCTL = WDTPW + WDTHOLD;
	WDTCTL = WDTPW + WDTTMSEL + WDTCNTCL;
	IE1 |= WDTIE;
	DCOCTL = 0; // Select lowest DCOx and MODx settings
	BCSCTL1 = CALBC1_1MHZ; // Set DCO
	DCOCTL = CALDCO_1MHZ;

//	P2DIR |= 0xFF; // All P2.x outputs
//	P2OUT &= 0x00; // All P2.x reset

	P1SEL |= RXD + TXD ; // P1.1 = RXD, P1.2=TXD
	P1SEL2 |= RXD + TXD ; // P1.1 = RXD, P1.2=TXD

	P1DIR |= LED_GRN;
	//P1DIR &= ~BUTTON;

	P1OUT &= 0x00;
	//P1OUT |= BUTTON;
	//P1REN |= BUTTON;

	P1OUT &= ~(LED_GRN);

	init_GPIO();

	//Motor Setup
	 P2DIR |= BIT2;
		P2OUT = 0; 								// Clear all outputs P2
	  	P2SEL |= BIT2;                          // P2.2 select TA1.1 option
	/*** Timer0_A Set-Up ***/
		TA1CCR0 = 20000-1;                           // PWM Period TA1.1
		TA1CCR0 |= 750;					// PWM period
	  	TA1CCTL1 = OUTMOD_7;                       // CCR1 reset/set
	  	TA1CTL   = TASSEL_2 + MC_1;                // SMCLK, up mode

	// Need 9600 Baud
	UCA0CTL1 = UCSWRST;
	UCA0CTL1 |= UCSSEL_2; // SMCLK
	UCA0BR0 = 104; // 1MHz 9600 (Divider 104)
	UCA0BR1 = 0; // 1MHz 9600
	UCA0MCTL = UCBRS0; // Modulation UCBRSx = 1
					  // To set UCA0MCTL you += it with the bits UCBRS0, UCBRS1, etc, where the bits make up the desired binary number UCBRSx
	UCA0CTL1 &= ~UCSWRST; // **Initialize USCI state machine**

	i = 0;
	UC0IE |= UCA0TXIE; // Enable USCI_A0 TX interrupt
	j = 0;
	UC0IE |= UCA0RXIE; // Enable USCI_A0 RX interrupt

	state = S_CLEAR_FINGER;
	next_state = S_ENROLL_START;
	finger_id = 1;

	UCA0TXBUF = tx_cmd[i++];
	__bis_SR_register(LPM0_bits + GIE); // Enter LPM0 w/ int until Byte RXed
}

#pragma vector=USCIAB0TX_VECTOR
__interrupt void USCI0TX_ISR(void)
{
    UCA0TXBUF = tx_cmd[i++]; // TX next character
    if (i >= sizeof tx_cmd){ // TX over?
    	UC0IE &= ~UCA0TXIE; // Disable USCI_A0 TX interrupt
    }
}

#pragma vector=USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR(void)
{
	if(j < 12){
		rx_response[j++] = UCA0RXBUF;
		if(j == 12){
			response_read = 1;
			UC0IE &= ~UCA0RXIE;
		}
	}
}
