#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/debug.h"
#include "driverlib/sysctl.h"
#include "driverlib/adc.h"
#include "driverlib/interrupt.h"
#include "inc/tm4c123gh6pm.h"
#include "driverlib/gpio.h"
#include "driverlib/timer.h"

#define LED_MASK			GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3

uint32_t ui32Sample;
uint32_t sampleSequence[4096];
uint32_t MAD = 0;
uint16_t count = 0;
uint32_t sum = 0;
uint16_t avg = 2048;
uint32_t absVal = 0;
uint16_t i = 0;
int32_t v = 0;

bool first = true;
bool init = true;


// GPIO initialization
void GPIO_Init(void){
	  //
    // Enable Peripheral Clocks 
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
		SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

    //
    // Enable pin PE3 for ADC AIN0
    //
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3);
	
		// Enable pins PF1, PF2, and PF3 for LED output
		//
		GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, LED_MASK);
}

//ADC0 initializaiton
void ADC0_Init(void)
{
	
		SysCtlClockSet(SYSCTL_SYSDIV_5|SYSCTL_USE_PLL|SYSCTL_OSC_MAIN|SYSCTL_XTAL_16MHZ); // configure the system clock to be 40MHz
		SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);	//activate the clock of ADC0
		SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0); //activate the clock of Timer0
		SysCtlDelay(2);	//insert a few cycles after enabling the peripheral to allow the clock to be fully activated.
	
	
		TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC); // configure Timer0 as periodic
		TimerLoadSet(TIMER0_BASE, TIMER_A, 1000); //load Timer0A with period of 10000 clock cycles
		//IntPrioritySet(INT_TIMER0A, 0x00);  	 // configure Timer0A interrupt priority as 0
		
		ADCSequenceDisable(ADC0_BASE, 3); //disable ADC0 before the configuration is complete
		ADCSequenceConfigure(ADC0_BASE, 3, ADC_TRIGGER_TIMER, 0); // will use ADC0, SS3, processor-trigger, priority 0
	
		//ADC0 SS3 Step 0, sample from ADC SS3 channel 0, completion of this step will set RIS, last sample of the sequence
		ADCSequenceStepConfigure(ADC0_BASE,3,0,ADC_CTL_CH0|ADC_CTL_IE|ADC_CTL_END); 
		
		IntPrioritySet(INT_ADC0SS3, 0x00);  	 // configure ADC0 SS3 interrupt priority as 0
		IntEnable(INT_ADC0SS3);    				// enable interrupt on ADC0 SS3
		ADCIntEnable(ADC0_BASE, 3);      // arm interrupt of ADC0 SS3
	
		TimerControlTrigger(TIMER0_BASE, TIMER_A, true); // specifies trigger timer
		ADCSequenceEnable(ADC0_BASE, 3); //enable ADC0
	
		TimerEnable(TIMER0_BASE, TIMER_A);
}

void newData(uint32_t sample){
		
		// after first sample window is taken, the first 96 values in 
		// sampleSequence will be the last 96 values of the previous window
		// this allows for the simplicity of division by powers of 2 (bit shifting)
		// while keeping windows within 10/s
		
	
		if(first == false && i == 0){
			MAD = (absVal + 2048) >> 12;
			if(MAD > 60){
				if(init == true)
					init = false;
				if(count > 0)
				count--;
			}
			else if(MAD > 0 && init == false)
				count++;
			absVal = 0;
			i = 95;
		}
				
		sum -= sampleSequence[i];		// remove previous value at i from value of sum		
		sampleSequence[i] = sample;	// replace previous value at i with new value held in sample
		sum += sample;							// add sample to sum
		avg = (sum + 2048) >> 12;	  // average by right-shifting sum 12 bits, equivalent to dividing by 2^12 (the number of samples in sampleSequence)
																// 2048 added to sum before bit shift to account for loss of fractional values due to integer division
	
		v = sample - avg;						// set v to sample minus avg to obtain true voltage level
		
		// add absolute value of v to absVal
		if(v < 0)										
			absVal -= v;
		else
			absVal += v;
		
		//absVal = (absVal + 2048) >> 12;
		
		i++; 												// increment i
		i &= 4096;									// bitwise and to have i rollover to 0 when 4096 is reached
		
		if(first == true && i == 4095){
			absVal = 0;
			first = false;
			for(int j = 0; j < 96; j++){
				sum -= sampleSequence[j];
				sampleSequence[j] = avg;
				sum += avg;
			}
		}
}

//interrupt handler
void ADC0_Handler(void)
{
		ADCIntClear(ADC0_BASE, 3); // clear the interrupt
		ADCSequenceDataGet(ADC0_BASE, 3, &ui32Sample); // collect sample data from PE3
		newData(ui32Sample);
}

int main(void)
{				
		ADC0_Init();	// initialize ADC0 and Timer0
		GPIO_Init();				// initialize gpio
		IntMasterEnable();  // globally enable interrupt
	
		GPIOPinWrite(GPIO_PORTF_BASE, LED_MASK, 0x02);
	
		while(1)
		{	
			if(count > 30 && count < 80 && init == false)
				GPIOPinWrite(GPIO_PORTF_BASE, LED_MASK, 0x04);
			else if(count >= 80 && count < 150 && init == false){
				GPIOPinWrite(GPIO_PORTF_BASE, LED_MASK, 0x08);
			}
			else if(count >= 150 && count < 600 && init == false)
				GPIOPinWrite(GPIO_PORTF_BASE, LED_MASK, 0x00);
			else if(count >= 600){
				count = 0;
				init = true;
			}
			else
				GPIOPinWrite(GPIO_PORTF_BASE, LED_MASK, 0x02);
		}
}
