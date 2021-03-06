#include "DueTimerLite.h"
#include "TFT8352.h"
#include "ads7843.h"

// #define EXTRA_TRACE

//#define Serial SerialUSB

#define TFT_UPDATE_INTERVAL 250000
#define FLYWHEEL_MAGNET_SENSOR_PIN 6 // Port PC23 on SAM3
#define IGNITION_OUT_PIN 7
#define NTC_PIN 0
#define PRE_IGITION_SLOPE_DEFAULT 10.0 // Degrees @ 1600 rpm. These two combined gives 25 deg + BIAS @ 1600 rpm
#define PRE_IGITION_SLOPE_DIVISOR 9600.0 // Angular Frequency @ 1600 rpm. Slope is deg/ang_freq => time (s)
#define DWELL_TIME_DEFAULT 0.0022 // Gives 400V over IGBT during discharge
#define DWELL_TIME_LONG 0.0022 // Compensate for inaccuracy at low rpm
#define LONG_DWELL_TIME_RPM_THRESHOLD 500 // DWELL_TIME_DEFAULT over this RPM
#define LONG_DWELL_TIME_RPM_HYSTERESIS 50 // DWELL_TIME_LONG below THRESHOLD - HYSTERESIS
#define RPM_SMOOTHING_LENGTH 4
#define DELTA_TIME_HISTORY_LENGTH 4
#define REFERENCE_RESISTANCE 100 // Ohm
#define ANALOG_REF_VOLTAGE 3.3


#define BASE_X_OFFSET 80
#define BASE_Y_OFFSET 160
#define RPM_X_OFFSET BASE_X_OFFSET
#define RPM_Y_OFFSET BASE_Y_OFFSET
#define BIAS_X_OFFSET BASE_X_OFFSET
#define BIAS_Y_OFFSET (BASE_Y_OFFSET + FONTHEIGHT) 
#define SLOPE_X_OFFSET BASE_X_OFFSET
#define SLOPE_Y_OFFSET (BASE_Y_OFFSET + FONTHEIGHT * 2)
#define TEMP_X_OFFSET BASE_X_OFFSET
#define TEMP_Y_OFFSET (BASE_Y_OFFSET + FONTHEIGHT * 3)
#define SERIAL_SEND_BUFFER_SIZE 250

#define NR_OF_MAGNETS 10
#if NR_OF_MAGNETS == 4
int32_t true_crank_angle[NR_OF_MAGNETS] = {90, 180, 270, 360};
#define ENGINE_STOPPING_DELTA_TIME 100000
#define PRE_IGITION_BIAS_DEFAULT -20
#elif NR_OF_MAGNETS == 10
int32_t true_crank_angle[NR_OF_MAGNETS] = {36, 72, 108, 144, 180, 216, 252, 289, 324, 360};
#define ENGINE_STOPPING_DELTA_TIME 50000
#define PRE_IGITION_BIAS_DEFAULT 15
#else
#error "NR_OF_MAGNETS not defined"
#endif

enum State {STATE_STOPPED, STATE_PHASE_FIND, STATE_STARTING, STATE_RUNNING};
enum Stroke {STROKE_COMPRESSION, STROKE_EXHAUST};

const float ntc_table[25] = { 3226, 2515, 1976, 1565, 1248,
							1000.0, 809.6, 657.3, 537.0, 441.7,
							365.3, 303.3, 253.1, 212.7, 179.6,
							152.2, 129.5, 110.7, 94.95, 81.78,
							70.69, 61.38, 53.49, 46.73, 40.95 };


State current_state = STATE_STOPPED;
State new_state = STATE_STOPPED;
DueTimerLite dueTimer(0);
TFT8352 tft;
uint32_t last_tft_update;
volatile int32_t pre_ignition_slope = PRE_IGITION_SLOPE_DEFAULT;
volatile int32_t pre_ignition_bias = PRE_IGITION_BIAS_DEFAULT;
char serial_send_buffer[SERIAL_SEND_BUFFER_SIZE];
volatile uint32_t revolution_time_history[RPM_SMOOTHING_LENGTH];
volatile bool magnet_passed = true;
volatile int32_t estimated_crank_angle; // In 0.1 degrees
volatile int32_t angle_delta;
volatile int32_t current_magnet = -1;
volatile uint32_t rpm;
volatile uint32_t angular_frequency; // In 0.1 degrees / second
volatile int32_t delta_time_history[DELTA_TIME_HISTORY_LENGTH];
volatile int32_t low_time_history[DELTA_TIME_HISTORY_LENGTH];
volatile int32_t magnet_start_time = 0;
volatile int32_t magnet_time_delta = 0;
volatile int32_t revolution_time = 0;
volatile int32_t dwell_start_angle = 0;
volatile int32_t ignition_angle = 0;
volatile int32_t nr_of_magnets_passed = 0;
volatile uint32_t temperature_value = 0;
uint32_t temperature = 123;

ADS7843 touch(26, 25, 27, 29, 30);

void calculate_angles(float dwell_time);
void estimate_temperature(float resistance);
void set_state(State state);
void estimate_angle_handler();
void magnet_handler();
void update_tft();

void setup()
{
	pinMode(FLYWHEEL_MAGNET_SENSOR_PIN, INPUT_PULLUP);
	pinMode(IGNITION_OUT_PIN, OUTPUT);
	pinMode(NTC_PIN, INPUT);
	digitalWrite(IGNITION_OUT_PIN, LOW);

	tft.begin();
	tft.setRotation(0);
	tft.fillScreen(0xffff);
	Serial.begin(115200);
	touch.begin();
	update_tft();
	last_tft_update = micros();
	memset(serial_send_buffer, 0, SERIAL_SEND_BUFFER_SIZE);
	attachInterrupt(FLYWHEEL_MAGNET_SENSOR_PIN, magnet_handler, CHANGE);
	dueTimer.attachInterrupt(estimate_angle_handler);

	temperature_value = analogRead(NTC_PIN);

	ADC->ADC_MR |= 0x80; // these lines set free running mode
	ADC->ADC_CR = 2; // Start ADC
	ADC->ADC_CHER = (1 << ADC7) | (1 << ADC15) ; // Enable ADC channel 7

	// Set up debounching on FLYWHEEL_MAGNET_SENSOR_PIN
	// Assuming Slow Clock of 32 kHz, so < 30us pulses will be filtered
	//	PIOC->PIO_PER = PIO_PC24;  //Enable PIO
	PIOC->PIO_IFER |= PIO_PC24;
	PIOC->PIO_DIFSR |= PIO_PC24;
	PIOC->PIO_SCDR = 0;
}

void loop()
{
	if (current_state != new_state)
	{
		set_state(new_state);
	}

	if (magnet_passed == true)
	{
		magnet_passed = false;

		switch (current_state)
		{
		case STATE_STOPPED:
			break;
		case STATE_PHASE_FIND:
			break;
		case STATE_STARTING:
			if (current_magnet != 0) // No slip in est angle around TDC.
			{
				dueTimer.updateFrequency(angular_frequency);
			}
			else
			{
				calculate_angles(DWELL_TIME_LONG);
			}
			if (rpm > LONG_DWELL_TIME_RPM_THRESHOLD)
			{
				new_state = STATE_RUNNING;
			}
			break;
		case STATE_RUNNING:
			if (current_magnet != 0) 
			{
				dueTimer.updateFrequency(angular_frequency);
			}
			else
			{
				calculate_angles(DWELL_TIME_DEFAULT);
			}
			if (rpm < LONG_DWELL_TIME_RPM_THRESHOLD - LONG_DWELL_TIME_RPM_HYSTERESIS)
			{
				new_state = STATE_STARTING;
				current_state = new_state; // Bypass state machine logic
			}

			// Add guard for too low dwell start / ignition angle
			break;
		default:
			break;
		}

		noInterrupts();
		int send_size = sprintf(serial_send_buffer, "{\"ht\":%lu,\"lt\":%lu,\"ia\":%ld,\"ea\":%ld,\"ad\":%ld,\"cm\":%ld,"
				"\"rp\":%lu,\"st\":%d,\"af\":%lu,\"da\":%ld,\"nm\":%lu,\"tp\":%lu}\n",
			delta_time_history[0], low_time_history[0], ignition_angle, estimated_crank_angle, angle_delta, current_magnet, rpm,
			current_state, angular_frequency, dwell_start_angle, nr_of_magnets_passed, temperature);
		interrupts();
		Serial.write((uint8_t*)serial_send_buffer, send_size);
	}

	if (micros() - last_tft_update > TFT_UPDATE_INTERVAL)
	{
		update_tft();
		if ((int32_t)(last_tft_update - magnet_start_time) > 1e6)
		{
#ifdef EXTRA_TRACE
			Serial.println("Too long since magnet start time. Engine probably stopped");
#endif				
			new_state = STATE_STOPPED;
		}
		last_tft_update = micros();

		while ((ADC->ADC_ISR & (1 << ADC7)) == 0); // wait for conversion
		temperature_value = ADC->ADC_CDR[ADC7]; // read data from ADC channel 7
		float voltage = (ANALOG_REF_VOLTAGE * ((float)temperature_value / (1 << ADC_RESOLUTION)));
		float current =  voltage / REFERENCE_RESISTANCE;
		float ntc_resistance = (ANALOG_REF_VOLTAGE - voltage) / current;
		estimate_temperature(ntc_resistance);
	}
}

void estimate_temperature(float resistance)
{
	uint32_t i = 0;
	if (resistance >= ntc_table[0])
	{
		temperature = 0;
		return;
	}
	else if (resistance <= ntc_table[sizeof(ntc_table) / sizeof(ntc_table[0])])
	{
		temperature = 120;
		return;
	}
	while (resistance < ntc_table[i])
	{
		i++;
	}
	float dy = (resistance - ntc_table[i - 1]) / (ntc_table[i] - ntc_table[i - 1]);
	temperature = (((float)i - 1.0 + dy) * 5.0);
}

void calculate_angles(float dwell_time)
{
	uint32_t revolution_time_sum = 0;
	for (int i = 0; i < RPM_SMOOTHING_LENGTH; i++)
	{
		revolution_time_sum += revolution_time_history[i];
	}
	rpm = (60 * RPM_SMOOTHING_LENGTH) / (revolution_time_sum / 1000000.0);
	float angular_freq_dependent_pre_ignition = (float)pre_ignition_slope / PRE_IGITION_SLOPE_DIVISOR;
//	float ang_freq = 3600.0 / ((float)revolution_time_history[0] / 1000000.0);
	float ang_freq = rpm * 60;
	dwell_start_angle = 3600.0 - (pre_ignition_bias * 10) -
		ang_freq * (dwell_time + angular_freq_dependent_pre_ignition);
	ignition_angle = 3600.0 - (pre_ignition_bias * 10) -
		ang_freq * angular_freq_dependent_pre_ignition;
}

void estimate_angle_handler()
{
	estimated_crank_angle++;
	//if (estimated_crank_angle >= 3600)
	//{
	//	estimated_crank_angle -= 3600;
	//}
	switch (current_state)
	{
	case STATE_STOPPED:
		break;
	case STATE_PHASE_FIND:
		break;
	case STATE_STARTING:
	case STATE_RUNNING:
		if (estimated_crank_angle < ignition_angle && estimated_crank_angle >= dwell_start_angle)
		{
			digitalWrite(IGNITION_OUT_PIN, HIGH);
		}
		else
		{
			digitalWrite(IGNITION_OUT_PIN, LOW);
		}
		break;
	default:
		break;
	}
}

void magnet_handler()
{
	int32_t now = micros();
	int32_t pin_state = digitalRead(FLYWHEEL_MAGNET_SENSOR_PIN);
	if (pin_state == LOW)
	{
		magnet_time_delta = now - magnet_start_time;
		magnet_start_time = now;
		switch (current_state)
		{
		case STATE_STOPPED:
			new_state = STATE_PHASE_FIND;
			break;
		case STATE_PHASE_FIND:
			nr_of_magnets_passed++;
			if (nr_of_magnets_passed < 2)
			{
				return;
			}
			if (nr_of_magnets_passed > DELTA_TIME_HISTORY_LENGTH + 2)
			{
				int32_t delta_time_sum = 0;
				for (int i = 0; i < DELTA_TIME_HISTORY_LENGTH; i++)
				{
					delta_time_sum += delta_time_history[i];
				}
				if (delta_time_sum / 8 > magnet_time_delta)
				{
					new_state = STATE_STARTING;
					angular_frequency = (3600.0 / NR_OF_MAGNETS) /
						(delta_time_history[(nr_of_magnets_passed - 1) % DELTA_TIME_HISTORY_LENGTH] / 1000000.0);
					estimated_crank_angle = (magnet_time_delta / 1000000.0) * angular_frequency;
					current_magnet = -1; // Don't count this magnet
				}
			}
			delta_time_history[nr_of_magnets_passed % DELTA_TIME_HISTORY_LENGTH] = magnet_time_delta;
			break;
		case STATE_STARTING:
		case STATE_RUNNING:
			if (delta_time_history[0] / 2 > magnet_time_delta)
			{
				current_magnet = -1; // Don't count this magnet
				magnet_start_time -= magnet_time_delta;
				return;
			}
			else if (magnet_time_delta > ENGINE_STOPPING_DELTA_TIME)
			{
				new_state = STATE_STOPPED;
			}
			else
			{
				if (current_magnet == 0)
				{
					if ((rpm > 300) && (now - revolution_time) * 1.1 > revolution_time_history[0]) // Allow 10% change in RPM
					{
						for (int i = RPM_SMOOTHING_LENGTH - 1; i > 0; i--)
						{
							revolution_time_history[i] = revolution_time_history[i - 1];
						}
						revolution_time_history[0] = now - revolution_time;
						revolution_time = now;
					}
					else
					{
						for (int i = RPM_SMOOTHING_LENGTH - 1; i > 0; i--)
						{
							revolution_time_history[i] = revolution_time_history[i - 1];
						}
						revolution_time_history[0] = now - revolution_time;
						revolution_time = now;
					}

				}
				angle_delta = estimated_crank_angle - (true_crank_angle[current_magnet] * 10);
				estimated_crank_angle = true_crank_angle[current_magnet] * 10;
				/* Explicity setting the estimated crank angle in this way may lead to an extra
				unintentional dwell/ignition sequence if the estimation have run ahead of the actual
				and we have a VERY late ignition point (>360). 
				The line above will put the logic back in the dwell period.*/
				angular_frequency = (3600.0 / NR_OF_MAGNETS) / (magnet_time_delta / 1000000.0);
				delta_time_history[0] = magnet_time_delta;
			}
			break;
		default:
			break;
		}
	}
	if (pin_state == HIGH)
	{
		//if (current_magnet >= 0)
		//{
		//	magnet_passed = true;
		//}
		magnet_passed = true;
		current_magnet++;
		current_magnet %= NR_OF_MAGNETS;
		low_time_history[0] = now - magnet_start_time;
	}
}

void set_state(State state)
{
#ifdef EXTRA_TRACE
	Serial.print("Entering set_state() with state: ");
	Serial.println(state);
#endif
	noInterrupts();
	switch (state)
	{
	case STATE_STOPPED:
		digitalWrite(IGNITION_OUT_PIN, LOW);
		dueTimer.stop();
		rpm = 0;
		break;
	case STATE_PHASE_FIND:
		digitalWrite(IGNITION_OUT_PIN, LOW);
		dueTimer.stop();
		nr_of_magnets_passed = 0;
		break;
	case STATE_STARTING:
		for (int i = 0; i < RPM_SMOOTHING_LENGTH; i++)
		{
			revolution_time_history[i] = UINT32_MAX;
		}
		dueTimer.start(angular_frequency);
		// revolution_time = micros();
		break;
	case STATE_RUNNING:
		break;
	}
	current_state = state;
	interrupts();
}

void update_tft()
{
	uint8_t flag;
	Point p = touch.getpos(&flag);
	if (flag != 0)
	{
		p.x -= 270; // Magic stuff
		p.x /= 14.58;
		p.y -= 230;
		p.y /= 9;
		if (p.x > BIAS_X_OFFSET && p.x < (BIAS_X_OFFSET + FONTWIDTH * 2))
		{
			if (p.y > BIAS_Y_OFFSET && p.y < (BIAS_Y_OFFSET + FONTHEIGHT))
			{
				--pre_ignition_bias;
				if (pre_ignition_bias < 0)
				{
					tft.setAddrWindow(BIAS_X_OFFSET + FONTWIDTH * 1.2, BIAS_Y_OFFSET + FONTHEIGHT * 0.5,
						BIAS_X_OFFSET + FONTWIDTH * 1.8, BIAS_Y_OFFSET + FONTHEIGHT * 0.53);
					tft.flood(0, FONTWIDTH * 0.6 * FONTHEIGHT * 0.1);
				}
			}
			else if (p.y > SLOPE_Y_OFFSET && p.y < (SLOPE_Y_OFFSET + FONTHEIGHT))
			{
				if (pre_ignition_slope > 0)
				{
					--pre_ignition_slope;
				}
			}
		}
		else if (p.x > (BIAS_X_OFFSET + FONTWIDTH * 2) && p.x < (BIAS_X_OFFSET + FONTWIDTH * 4))
		{
			if (p.y > BIAS_Y_OFFSET && p.y < (BIAS_Y_OFFSET + FONTHEIGHT))
			{
				++pre_ignition_bias;
				if (pre_ignition_bias >= 0)
				{
					tft.setAddrWindow(BIAS_X_OFFSET + FONTWIDTH * 1.2, BIAS_Y_OFFSET + FONTHEIGHT * 0.5,
						BIAS_X_OFFSET + FONTWIDTH * 1.8, BIAS_Y_OFFSET + FONTHEIGHT * 0.53);
					tft.flood(0xffff, FONTWIDTH * 0.6 * FONTHEIGHT * 0.1);
				}
			}
			else if (p.y > SLOPE_Y_OFFSET && p.y < (SLOPE_Y_OFFSET + FONTHEIGHT))
			{
				++pre_ignition_slope;
			}

		}
	}

	//if (false == true)
	//{
	//	tft.setAddrWindow(RPM_X_OFFSET - FONTWIDTH, RPM_Y_OFFSET, RPM_X_OFFSET, RPM_Y_OFFSET + FONTHEIGHT);
	//	tft.flood(0, FONTHEIGHT * FONTWIDTH);
	//}
	else
	{
		tft.setAddrWindow(RPM_X_OFFSET - FONTWIDTH, RPM_Y_OFFSET, RPM_X_OFFSET, RPM_Y_OFFSET + FONTHEIGHT);
		tft.flood(0xffff, FONTHEIGHT * FONTWIDTH);
	}
	tft.drawFigure(RPM_X_OFFSET + FONTWIDTH * 0, RPM_Y_OFFSET, ((rpm / 1000) % 10));
	tft.drawFigure(RPM_X_OFFSET + FONTWIDTH * 1, RPM_Y_OFFSET, ((rpm / 100) % 10));
	tft.drawFigure(RPM_X_OFFSET + FONTWIDTH * 2, RPM_Y_OFFSET, ((rpm / 10) % 10));
	tft.drawFigure(RPM_X_OFFSET + FONTWIDTH * 3, RPM_Y_OFFSET, rpm % 10);

	tft.drawFigure(BIAS_X_OFFSET + FONTWIDTH * 2, BIAS_Y_OFFSET, (abs((pre_ignition_bias / 10)) % 10));
	tft.drawFigure(BIAS_X_OFFSET + FONTWIDTH * 3, BIAS_Y_OFFSET, abs(pre_ignition_bias) % 10);

	tft.drawFigure(SLOPE_X_OFFSET + FONTWIDTH * 2, SLOPE_Y_OFFSET, ((pre_ignition_slope / 10) % 10));
	tft.drawFigure(SLOPE_X_OFFSET + FONTWIDTH * 3, SLOPE_Y_OFFSET, pre_ignition_slope % 10);

	tft.drawFigure(TEMP_X_OFFSET + FONTWIDTH * 1, TEMP_Y_OFFSET, ((temperature / 100) % 10));
	tft.drawFigure(TEMP_X_OFFSET + FONTWIDTH * 2, TEMP_Y_OFFSET, ((temperature / 10) % 10));
	tft.drawFigure(TEMP_X_OFFSET + FONTWIDTH * 3, TEMP_Y_OFFSET, temperature % 10);
}