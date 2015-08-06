/*
  DueTimerLite.cpp - Implementation of Timers defined on DueTimerLite.h
  For instructions, go to https://github.com/ivanseidel/DueTimer

  Created by Ivan Seidel Gomes, March, 2013.
  Modified by Philipp Klaus, June 2013.
  Thanks to stimmer (from Arduino forum), for coding the "timer soul" (Register stuff)
  Released into the public domain.
*/

#include "DueTimerLite.h"
struct {
	uint8_t flag;
	uint8_t divisor;
} clockConfig[] = {
	{ TC_CMR_TCCLKS_TIMER_CLOCK1,   2 },
	{ TC_CMR_TCCLKS_TIMER_CLOCK2,   8 },
	{ TC_CMR_TCCLKS_TIMER_CLOCK3,  32 },
	{ TC_CMR_TCCLKS_TIMER_CLOCK4, 128 }
};

const DueTimerLite::Timer DueTimerLite::Timers[NR_OF_TIMERS] = {
	{TC0,0,TC0_IRQn},
	//{TC0,1,TC1_IRQn},
	//{TC0,2,TC2_IRQn},
	//{TC1,0,TC3_IRQn},
	//{TC1,1,TC4_IRQn},
	//{TC1,2,TC5_IRQn},
	//{TC2,0,TC6_IRQn},
	//{TC2,1,TC7_IRQn},
	//{TC2,2,TC8_IRQn},
};

void (*DueTimerLite::callbacks[NR_OF_TIMERS])() = {};
double DueTimerLite::_frequency[NR_OF_TIMERS] = {-1,
	// -1,-1,-1,-1,-1,-1,-1,-1,
	};

/*
	Initializing all timers, so you can use them like this: Timer0.start();
*/
//DueTimerLite Timer(0);

//DueTimerLite Timer0(0);
//DueTimerLite Timer1(1);
//DueTimerLite Timer2(2);
//DueTimerLite Timer3(3);
//DueTimerLite Timer4(4);
//DueTimerLite Timer5(5);
//DueTimerLite Timer6(6);
//DueTimerLite Timer7(7);
//DueTimerLite Timer8(8);

DueTimerLite::DueTimerLite(int _timer){
	/*
		The constructor of the class DueTimerLite 
	*/

	timer = _timer;
}

//DueTimerLite DueTimerLite::getAvailable(){
	///*
		//Return the first timer with no callback set
	//*/
//
	//for(int i = 0; i < 9; i++){
		//if(!callbacks[i])
			//return DueTimerLite(i);
	//}
	//// Default, return Timer0;
	//return DueTimerLite(0);
//}

DueTimerLite DueTimerLite::attachInterrupt(void (*isr)()){
	/*
		Links the function passed as argument to the timer of the object
	*/

	callbacks[timer] = isr;

	return *this;
}

DueTimerLite DueTimerLite::detachInterrupt(){
	/*
		Links the function passed as argument to the timer of the object
	*/

	stop(); // Stop the currently running timer

	callbacks[timer] = NULL;

	return *this;
}

DueTimerLite DueTimerLite::start(double frequency){
	/*
		Start the timer
		If a period is set, then sets the period and start the timer
	*/

	
	if(_frequency[timer] <= 0)
		setFrequency(frequency);

	NVIC_ClearPendingIRQ(Timers[timer].irq);
	NVIC_EnableIRQ(Timers[timer].irq);
	
	TC_Start(Timers[timer].tc, Timers[timer].channel);

	return *this;
}

DueTimerLite DueTimerLite::stop(){
	/*
		Stop the timer
	*/

	NVIC_DisableIRQ(Timers[timer].irq);
	
	TC_Stop(Timers[timer].tc, Timers[timer].channel);

	return *this;
}

uint8_t DueTimerLite::bestClock(double frequency, uint32_t& retRC){
	/*
		Pick the best Clock, thanks to Ogle Basil Hall!

		Timer		Definition
		TIMER_CLOCK1	MCK /  2
		TIMER_CLOCK2	MCK /  8
		TIMER_CLOCK3	MCK / 32
		TIMER_CLOCK4	MCK /128
	*/
	
	float ticks;
	float error;
	int clkId = 3;
	int bestClock = 3;
	float bestError = 1.0;
	do
	{
		ticks = (float) VARIANT_MCK / frequency / (float) clockConfig[clkId].divisor;
		error = abs(ticks - round(ticks));
		if (abs(error) < bestError)
		{
			bestClock = clkId;
			bestError = error;
		}
	} while (clkId-- > 0);
	ticks = (float) VARIANT_MCK / frequency / (float) clockConfig[bestClock].divisor;
	retRC = (uint32_t) round(ticks);
	return bestClock;
}


DueTimerLite DueTimerLite::setFrequency(double frequency){
	/*
		Set the timer frequency (in Hz)
	*/

	// Prevent negative frequencies
	if(frequency <= 0) { frequency = 1; }

	// Remember the frequency
	_frequency[timer] = frequency;

	// Get current timer configuration
	Timer t = Timers[timer];

	uint32_t rc = 0;

	// Tell the Power Management Controller to disable 
	// the write protection of the (Timer/Counter) registers:
	pmc_set_writeprotect(false);

	// Enable clock for the timer
	pmc_enable_periph_clk((uint32_t)t.irq);

	// Find the best clock for the wanted frequency
	selected_clock = bestClock(frequency, rc);

	// Set up the Timer in waveform mode which creates a PWM
	// in UP mode with automatic trigger on RC Compare
	// and sets it up with the determined internal clock as clock input.
	TC_Configure(t.tc, t.channel, TC_CMR_WAVE | TC_CMR_WAVSEL_UP_RC | clockConfig[selected_clock].flag);
	// Reset counter and fire interrupt when RC value is matched:
	TC_SetRC(t.tc, t.channel, rc);
	// Enable the RC Compare Interrupt...
	t.tc->TC_CHANNEL[t.channel].TC_IER=TC_IER_CPCS;
	// ... and disable all others.
	t.tc->TC_CHANNEL[t.channel].TC_IDR=~TC_IER_CPCS;

	return *this;
}

DueTimerLite DueTimerLite::updateFrequency(double frequency){
//	noInterrupts();
	TC_Stop(Timers[timer].tc, Timers[timer].channel);
//	Timers[timer].tc->TC_CHANNEL[Timers[timer].channel].TC_IDR=0xff;
	float ticks = (float) VARIANT_MCK / frequency / (float) clockConfig[selected_clock].divisor;	
	TC_SetRC(Timers[timer].tc, Timers[timer].channel, (uint32_t) round(ticks));
//	Timers[timer].tc->TC_CHANNEL[Timers[timer].channel].TC_IER=TC_IER_CPCS;
//	Timers[timer].tc->TC_CHANNEL[Timers[timer].channel].TC_IDR=~TC_IER_CPCS;
	TC_Start(Timers[timer].tc, Timers[timer].channel);
//	interrupts();
	return *this;	
}

//DueTimerLite DueTimerLite::setPeriod(long microseconds){
	///*
		//Set the period of the timer (in microseconds)
	//*/
//
	//// Convert period in microseconds to frequency in Hz
	//double frequency = 1000000.0 / microseconds;	
	//setFrequency(frequency);
	//return *this;
//}

//double DueTimerLite::getFrequency(){
	///*
		//Get current time frequency
	//*/
//
	//return _frequency[timer];
//}
//
//long DueTimerLite::getPeriod(){
	///*
		//Get current time period
	//*/
//
	//return 1.0/getFrequency()*1000000;
//}
//

/*
	Implementation of the timer callbacks defined in 
	arduino-1.5.2/hardware/arduino/sam/system/CMSIS/Device/ATMEL/sam3xa/include/sam3x8e.h
*/
void TC0_Handler(){
	TC_GetStatus(TC0, 0);
	DueTimerLite::callbacks[0]();
}
//void TC1_Handler(){
	//TC_GetStatus(TC0, 1);
	//DueTimerLite::callbacks[1]();
//}
//void TC2_Handler(){
	//TC_GetStatus(TC0, 2);
	//DueTimerLite::callbacks[2]();
//}
//void TC3_Handler(){
	//TC_GetStatus(TC1, 0);
	//DueTimerLite::callbacks[3]();
//}
//void TC4_Handler(){
	//TC_GetStatus(TC1, 1);
	//DueTimerLite::callbacks[4]();
//}
//void TC5_Handler(){
	//TC_GetStatus(TC1, 2);
	//DueTimerLite::callbacks[5]();
//}
//void TC6_Handler(){
	//TC_GetStatus(TC2, 0);
	//DueTimerLite::callbacks[6]();
//}
//void TC7_Handler(){
	//TC_GetStatus(TC2, 1);
	//DueTimerLite::callbacks[7]();
//}
//void TC8_Handler(){
	//TC_GetStatus(TC2, 2);
	//DueTimerLite::callbacks[8]();
//}
//