

#include "stm32l4xx.h"


/* Define function prototypes */
void Setup(void);
void PinSetup1(void);
void PinSetup2(void);
void InterruptSetup(void);
void PWMSetup(void);
void ADCSetup(void);
unsigned int callibrate(void);
void Delay(void);
void MillisDelay(unsigned int);
void Debounce(void);
void Keypad(void);
void EXTI0_IRQHandler(void);
void TIM6_IRQHandler(void);
void Count(void);



/* Define global variables */
static unsigned int ButtonValue;			//value of button pressed
static unsigned int column = 0;				//column that has been pressed
static unsigned int row = 0;				//row that has been pressed
static signed int columnNum;
static signed int rowNum;
static unsigned int i,k;
static unsigned int CountValue;				//value of Count (0-9)
static unsigned int counting = 0;
static unsigned int up = 1;
static const unsigned int PWMHertz = 5000;
static const unsigned int ARR = 4000000/PWMHertz;
static unsigned int DriveMode = 0;			// 0 when duty cycle driven, 1 when speed driven, D-toggled
static unsigned int target = 50;
static unsigned int ADCIn = 0;
static unsigned int ADCMax = 0;
static unsigned int targetSpeed = 0;
static int distance = 0;


/*------------------------------------------------------*/
/* 2D array to map columns and rows to keypad values	*/
/*------------------------------------------------------*/
static unsigned int KeypadMap [4][4] = {
	{0x01,0x02,0x03,0x0A},			//1st row
	{0x04,0x05,0x06,0x0B},			//2nd row
	{0x07,0x08,0x09,0x0C},			//3rd row
	{0x0E,0x00,0x0F,0x0D}			//4th row
};
//


/*--------------------------------------------------------------*/
/* Array to map buttonvalue to duty cycle, fraction of ARR	*/
/*--------------------------------------------------------------*/ 
static unsigned int DutyCycleMap [11] = {
	0, (int)(0.1*((double)ARR)), (int)(0.2*((double)ARR)), (int)(0.3*((double)ARR)), (int)(0.4*((double)ARR)), (int)(0.5*((double)ARR)), (int)(0.6*((double)ARR)), (int)(0.7*((double)ARR)), (int)(0.8*((double)ARR)), (int)(0.9*((double)ARR)), ARR+1
};
//


/*------------------------------------------------------*/
/* Function for Rerieving max ADC Value of motor	*/
/*------------------------------------------------------*/ 
unsigned int callibrate(void) {
	while (TIM2->CCR1 < (ARR + 1)) {
		TIM2->CCR1++;
		for (int l = 0; l < 1000; l++) { }
	}
	
	Delay();
	unsigned int max = ADC1->DR;
	
	while (TIM2->CCR1 > 0) {
		TIM2->CCR1--;
		for (int l = 0; l < 1000; l++) { }
	}
	
	TIM2->CCR1 = 0;
	
	return max;
}
//


/*------------------------------------------------------*/
/* Initialize Clocks					*/
/* Initialize GPIOB pins				*/
/* PB[0] = output of AND gate				*/
/* PB[6:3] = displayed value, CountValue or ButtonValue	*/
/*------------------------------------------------------*/
void Setup(void) {
	
	RCC->AHB2ENR |= (0x03u);	// enable GPIOA and GPIOB clock bits
	GPIOA->MODER &= (0xFFFFFFFCu);	// Set PA[0:1] = 00
	GPIOA->MODER |= (0x00C00006u);	// Set PA[0] = 10, PA[1] = 01, PA[12] = 11
	GPIOB->MODER &= (0xFFFFC03Cu);	// PB[6:3] and PB[0] = 00
	GPIOB->MODER |= (0x00001540u);	// PB[6:3] = 01
	
}
//


void PinSetup1(void) {
	Setup();
	GPIOA->MODER &= (0xFF00F00F);	//Set up PA[11:8] as Output Pins
	GPIOA->MODER |= (0x00550000);	//outputs of columns, PA[11:8] = 01
	GPIOA->PUPDR &= (0xFFFFF00F);	//pull-reset// row, PA[5:2] = 00
	GPIOA->PUPDR |= (0x00000550);	//pull-up// row, PA[5:2] = 01 
}
//


void PinSetup2(void) {
	GPIOA->MODER &= (0xFF00F00F);	//inputs column and row, PA[11:8,5:2] = 00
	GPIOA->MODER |= (0x00000550);	//outputs row, PA[5:2] = 01
	GPIOA->PUPDR &= (0xFF00FFFF);	//pull-reset row, PA[11:8] = 00 
	GPIOA->PUPDR |= (0x00550000);	//pull-up row, PA[11:8] = 01  
}
//


/*--------------------------------------*/
/* Initialize Interrupts		*/
/* EXTI1 = External Interrupt One	*/
/* TIM6  = Internal Interrupt Timer	*/
/*--------------------------------------*/
void InterruptSetup(void) { 
	
	RCC->APB2ENR |= 0x01;				//enable interrupt clock SYSCFG
	SYSCFG->EXTICR[0] &= ~SYSCFG_EXTICR1_EXTI0;	//clear EXTI1 bit in config reg ~(0xF)
	SYSCFG->EXTICR[0] |= SYSCFG_EXTICR1_EXTI0_PB;	//PB configuration in EXTI0
	EXTI->FTSR1 |= (0x0001u);			//falling edge trigger enabled
	EXTI->IMR1 |= (0x0001u);			//enable (unmask) EXTI0
	EXTI->PR1 = EXTI_PR1_PIF1;
	NVIC_ClearPendingIRQ(EXTI0_IRQn);		//Clear NVIC pending bit 
	NVIC_EnableIRQ(EXTI0_IRQn);			//enable IRQ 
	NVIC_SetPriority(EXTI0_IRQn,1);			//high priority
	
	RCC-> APB1ENR1 |= RCC_APB1ENR1_TIM6EN;		//enable reset&clock control module
	TIM6->CR1 |= (0x0001u);				//Disable Count
	TIM6->ARR = (0x82Au);				//Auto Reset Reload value 1999 . new 2090
	TIM6->PSC = (0x76Au);				//Prescale value 1999 . new 1898
	TIM6->DIER = TIM_DIER_UIE;			//Enable UIE
	NVIC_EnableIRQ(TIM6_IRQn);			//Enable Interupt handler
	NVIC_SetPriority(TIM6_IRQn,2);			//Lower priority than keypad
	
}
//


void PWMSetup(void) {
	
	RCC->AHB2ENR |= 0x01;			//enable GPIOA clock 
	GPIOA->MODER &= (0xFFFFFFFC);		//mask PA0 to 00
	GPIOA->MODER |= (0x00000002);		//configure PA0=10 for AF mode
	GPIOA->AFR[0] &= (0xFFFFFFF0);		//mask bit[3:0]=00 	
	GPIOA->AFR[0] |= (0x00000001);		//configure bit[3:0]=001, AF1 selected
	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;	//enable timer 2 module
	TIM2->CR1 |= 0x01;			//enable timer 2 counter
	TIM2->CCMR1 &= (0xFFFFFF8C);		//mask bit[6:4]=00 
	TIM2->CCMR1 |= (0x00000060);		//configure output mode for PWM mode 1
	TIM2->CCER &=	(0xFFFC);		//clear timer channel 1 output
	TIM2->CCER |=	(0x0001);		//enable timer channel 1 output
	TIM2->PSC = 0;				//load value for pulse width modulation
	TIM2->ARR = ARR;			//period (T) = arr_value - 1
	TIM2->CCR1 = 0;				//change ON time of duty cycle	
	
}
//


/*------------------------------------------------------*/
/* Set up ADC for 1 conversion on tachometer value	*/
/*------------------------------------------------------*/
void ADCSetup(void) {
	RCC->AHB2ENR |= 0x00002000;
	
	RCC->CCIPR &= 0xCFFFFFFF;
	RCC->CCIPR |= 0x30000000;

	ADC1->CR &= 0xCFFFFFFF;
	ADC1->CR |= 0x10000000;
    
	for (int z = 0; z < 1000; z++) { }	//~20us pause for regulator to start

	ADC1->CFGR |= 0x00003000;
	
	ADC1->SQR1 &= 0XFFFFF830;
	ADC1->SQR1 |= 0x00000180;

	ADC1->CR |= 0x00000001;
	while (!(ADC1->ISR & 0x00000001));
	ADC1->CR |= 0X00000004;
	
}
//


void Debounce(void) { 
	volatile int n, j;
	for (i=0; i<15; i++) {			//outer loop
		for (j=0; j<1000; j++) {	//inner loop
			n = j;			//dummy operation for single-step test
		}				//do nothing
	}
}
//


/*------------------------------------------------------*/
/* Delay Function - ~1 second Delay			*/
/* Only used for callibration - optimize and remove?	*/
/*------------------------------------------------------*/
void Delay(void) { 
	volatile int n, j;
	for (i=0; i<12; i++) {			//outer loop
		for (j=0; j<19000; j++) {	//inner loop 
			n = j;			//dummy operation for single-step test
		}				//do nothing
	}
}
//


/*--------------------------------------------------------------*/
/* Keypad Function - find which button has been pressed		*/
/* Global ButtonValue from previous lab, could return char	*/
/*--------------------------------------------------------------*/
void Keypad(void) {
	
	ButtonValue = 0;
	rowNum = -1;
	columnNum = -1;
	
	PinSetup1();
	GPIOA->ODR &= (0xF0FF);				//set column to output 0, PA[11:8] = 0
	for (k=0; k<34; k++) { }			//Delay for values to load
	row = (~GPIOA->IDR & 0x003C);			//get row inputs
	
	PinSetup2();
	GPIOA->ODR &= (0xFFC3);				//set row to output 0, PA[5:2] = 0
	for(k=0; k<3; k++) { }				//Delay for values to load
	column = (~GPIOA->IDR & 0xF00);			//get column inputs, PA[11:8] = 0
	
	column = column >> 8;				//shift right by 8
	row = row >> 2;					//shift right by 2
	
	do {
		row = row << 1;				//shift left by 1 to find row Count
		rowNum++;				//add to row Count
	} while (row < 0x10) ;				//can only shift four times
	
	do {
		column = column << 1;			//shift left by 1 to find columnumn Count
		columnNum++;				//add to columnumn Count
	} while (column < 0x10);			//can only shift four times
	
	ButtonValue = KeypadMap[rowNum][columnNum];
	
}
//


/*----------------------------------------------*/
/* Count Function - changes Counter value	*/
/*----------------------------------------------*/
void Count(void) {
	
	if (up) {
		if (CountValue < 9) {
			CountValue++;
		} else { 				
			CountValue = 0;	//roll over to zero after nine
		}
	} else {
		if (CountValue > 0) {
			CountValue--;
		} else {
			CountValue = 9;
		}
	}
	
	GPIOB->ODR &= 0xFF87;		//Masking Bits for Pins 6-3
	unsigned int mask = 0;		//maskA is set to 0
	mask = CountValue << 3;		//SETTING LSB TO PIN 3, GOES TO PIN 6 BC ITS A 4 BIT VALUE
	GPIOB->ODR |= mask;		//mask PB[6:3] with Count
	
}
//


/*------------------------------------------------------*/
/* Interrupt Handler EXTI0 - Keypad has been pressed	*/
/*------------------------------------------------------*/
void EXTI0_IRQHandler(void) {
	
	for (int b = 0; b < 2; b = b+1) {
		Debounce();
	}
	
	Keypad();
	switch (ButtonValue) {
		
		case 0xB :
			break;
		
		case 0xC :
			break;
		
		case 0xD :
							
			if (DriveMode == 0) {
				DriveMode = 1;
			} else {
				DriveMode = 0;
			}
			break;
							
		case 0xE :
	
			if (counting == 0) {
				counting = 1;
			} else {
				counting = 0;
			}
			break;
							
		case 0xF :

			if (up == 0) {
				up = 1;
			} else {
				up = 0;
			}
			break;
							
		default :
			
			if (DriveMode) {
				switch (ButtonValue) {
					case 0x1 :
						target = 50;
						break;
					case 0x2 :
						target = 60;
						break;
					case 0x3 :
						target = 70;
						break;
					case 0x4 :
						target = 80;
						break;
					default :
						
						break;
				}
			} else {
				TIM2->CCR1 = DutyCycleMap[ButtonValue];
			}
			break;
	
	}
	
	PinSetup1();
	
	for (int b = 0; b < 3; b = b+1) {
		Debounce();
	}
	
	EXTI->PR1 |= 0x0001;			//clear EXTI0 pending bit*
	NVIC_ClearPendingIRQ(EXTI0_IRQn);	//clear NVIC pending bit with EXTI0 interrupt
}
//


/*--------------------------------------*/
/* Interrupt Handler TIM6 - Timer	*/
/*--------------------------------------*/
void TIM6_IRQHandler(void) {
	TIM6->CNT = 0;
	if (counting) Count();	//Increment count
	TIM6->SR &= ~(0x01u);	//Clear overload flag
}
//


/*--------------------------------------*/
/* Main Program				*/
/* Setup and Constant Speed Mode	*/
/*--------------------------------------*/
int main(void) {
	
	Setup();			//configure GPIO clocks and GPIOB pins
	PinSetup1();			//configure keypad such that it drives interrupt gate low.
	InterruptSetup();		//configure interrupts
	PWMSetup();
	ADCSetup();
	
	GPIOB->ODR |= 0x80;
	
	for (int m = 0; m < 25000; m++) { }	//delay for ADC setup to complete, otherwise gets hung
	ADCMax = callibrate();
	
	
	while (1) {
		
		ADCIn = ADC1->DR; 
		targetSpeed = (unsigned int)(target*ADCMax*0.01);
		distance = (int)(targetSpeed - ADCIn);
		
		if (DriveMode) {
			
			if (distance > 5 && TIM2->CCR1 < ARR+1) {
				TIM2->CCR1 = TIM2->CCR1 + (unsigned int)((distance)*0.1);
				for (int m = 0; m < 25000; m++) { }			//delay for motor acceleration to occur
			} else if (distance < -5 && TIM2->CCR1 > 0) {
				TIM2->CCR1 = TIM2->CCR1 - (unsigned int)((-distance)*0.1);
				for (int m = 0; m < 25000; m++) { }			//delay for motor acceleration to occur
			}
				
		}
		

	}

}
//

