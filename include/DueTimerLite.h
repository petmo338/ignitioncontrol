/*
  DueTimerLite.h - DueTimerLite header file, definition of methods and attributes...
  For instructions, go to https://github.com/ivanseidel/DueTimer

  Created by Ivan Seidel Gomes, March, 2013.
  Modified by Philipp Klaus, June 2013.
  Released into the public domain.
*/

#ifdef __SAM3X8E__

#ifndef DueTimerLite_h
#define DueTimerLite_h

#define NR_OF_TIMERS 1

#include "Arduino.h"

#include <inttypes.h>

class DueTimerLite
{
protected:

	// Represents the timer id (index for the array of Timer structs)
	int timer;
	
	uint8_t selected_clock;

	// Stores the object timer frequency
	// (allows to access current timer period and frequency):
	static double _frequency[NR_OF_TIMERS];

	// Picks the best clock to lower the error
	static uint8_t bestClock(double frequency, uint32_t& retRC);

public:
	struct Timer
	{
		Tc *tc;
		uint32_t channel;
		IRQn_Type irq;
	};

	//static DueTimerLite getAvailable();

	// Store timer configuration (static, as it's fix for every object)
	static const Timer Timers[1];

	// Needs to be public, because the handlers are outside class:
	static void (*callbacks[1])();

	DueTimerLite(int _timer);
	DueTimerLite attachInterrupt(void (*isr)());
	DueTimerLite detachInterrupt();
	DueTimerLite start(double frequency = 50.0);
	DueTimerLite stop();
	DueTimerLite setFrequency(double frequency);
	DueTimerLite updateFrequency(double frequency);
//	DueTimerLite setPeriod(long microseconds);


	//double getFrequency();
	//long getPeriod();
};

// Just to call Timer.getAvailable instead of Timer::getAvailable() :
//extern DueTimerLite Timer;
//
//extern DueTimerLite Timer0;
//extern DueTimerLite Timer1;
//extern DueTimerLite Timer2;
//extern DueTimerLite Timer3;
//extern DueTimerLite Timer4;
//extern DueTimerLite Timer5;
//extern DueTimerLite Timer6;
//extern DueTimerLite Timer7;
//extern DueTimerLite Timer8;

#endif

#else
	#error Oops! Trying to include DueTimerLite on another device?
#endif
