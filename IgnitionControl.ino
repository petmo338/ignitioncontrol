#include "DueTimerLite.h"
#include "TFT8352.h"
#include "ads7843.h"

// #define EXTRA_TRACE

#define Serial SerialUSB

#define TFT_UPDATE_INTERVAL 250000
#define FLYWHEEL_MAGNET_SENSOR_PIN 13
#define IGNITION_OUT_PIN 12
#define PRE_IGITION_SLOPE_DEFAULT 10.0 // Degrees @ 1600 rpm. These two combined gives 25 deg + BIAS @ 1600 rpm
#define PRE_IGITION_SLOPE_DIVISOR 9600.0 // Angular Frequency @ 1600 rpm. Slope is deg/ang_freq => time (s)
#define PRE_IGITION_BIAS_DEFAULT 2
#define DWELL_TIME_DEFAULT 0.0070
#define DWELL_TIME_LONG 0.0095
#define LONG_DWELL_TIME_RPM_THRESHOLD 600
#define LONG_DWELL_TIME_RPM_HYSTERESIS 100
#define RPM_SMOOTHING_LENGTH 4
#define DELTA_TIME_HISTORY_LENGTH 4


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
int32_t true_crank_angle[NR_OF_MAGNETS] = {90, 180, 270, 0};
#define ENGINE_STOPPING_DELTA_TIME 100000
#elif NR_OF_MAGNETS == 10
int32_t true_crank_angle[NR_OF_MAGNETS] = {36, 72, 108, 144, 180, 216, 252, 288, 324, 0};
#define ENGINE_STOPPING_DELTA_TIME 50000
#else
#error "NR_OF_MAGNETS not defined"
#endif

enum State {STATE_STOPPED, STATE_PHASE_FIND, STATE_STARTING, STATE_RUNNING};
enum Stroke {STROKE_COMPRESSION, STROKE_EXHAUST};

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
volatile int32_t estimated_crank_angle;
volatile int32_t angle_delta;
volatile int32_t current_magnet = -1;
volatile uint32_t rpm;
volatile uint32_t angular_frequency;
volatile int32_t delta_time_history[DELTA_TIME_HISTORY_LENGTH];
volatile int32_t magnet_start_time = 0;
volatile int32_t magnet_time_delta = 0;
volatile int32_t revolution_time = 0;
volatile int32_t dwell_start_angle = 0;
volatile int32_t ignition_angle = 0;
volatile int32_t nr_of_magnets_passed = 0;

ADS7843 touch(26, 25, 27, 29, 30);

void calculate_angles(float dwell_time);
void set_state(State state);
void estimate_angle_handler();
void magnet_handler();
void update_tft();

void setup()
{
	pinMode(FLYWHEEL_MAGNET_SENSOR_PIN, INPUT);
	pinMode(IGNITION_OUT_PIN, OUTPUT);
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
		int send_size = sprintf(serial_send_buffer, "{\"ht\":%lu,\"ia\":%ld,\"ea\":%ld,\"ad\":%ld,\"cm\":%ld,"
				"\"rp\":%lu,\"st\":%d,\"af\":%lu,\"da\":%ld,\"nm\":%lu}\n",
			delta_time_history[0], ignition_angle, estimated_crank_angle, angle_delta, current_magnet, rpm,
			current_state, angular_frequency, dwell_start_angle, nr_of_magnets_passed);
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
	}
}

void calculate_angles(float dwell_time)
{
	uint32_t revolution_time_sum = 0;
	for (int i = 0; i < RPM_SMOOTHING_LENGTH; i++)
	{
		revolution_time_sum += revolution_time_history[i];
	}
	rpm = (60 * RPM_SMOOTHING_LENGTH) / (revolution_time_sum / 1000000.0);
	float angular_freq_dependent_pre_ignition = float(pre_ignition_slope) / PRE_IGITION_SLOPE_DIVISOR;
	int32_t ang_freq = 360 / ((float)revolution_time_history[0] / 1000000.0);
	dwell_start_angle = 360 - pre_ignition_bias -
		ang_freq * (dwell_time + angular_freq_dependent_pre_ignition);
	ignition_angle = 360 - pre_ignition_bias -
		ang_freq * angular_freq_dependent_pre_ignition;
}

void estimate_angle_handler()
{
	estimated_crank_angle++;
	if (estimated_crank_angle >= 360)
	{
		estimated_crank_angle -= 360;
	}
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
					angular_frequency = (360.0 / NR_OF_MAGNETS) /
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
					for (int i = RPM_SMOOTHING_LENGTH - 1; i > 0; i--)
					{
						revolution_time_history[i] = revolution_time_history[i - 1];
					}
					revolution_time_history[0] = now - revolution_time;
					revolution_time = now;
				}
				angle_delta = estimated_crank_angle - true_crank_angle[current_magnet];
				estimated_crank_angle = true_crank_angle[current_magnet];
				angular_frequency = (360.0 / NR_OF_MAGNETS) / (magnet_time_delta / 1000000.0);
				delta_time_history[0] = magnet_time_delta;
			}
			break;
		default:
			break;
		}
	}
	if (pin_state == HIGH)
	{
		if (current_magnet >= 0)
		{
			magnet_passed = true;
		}
		current_magnet++;
		current_magnet %= NR_OF_MAGNETS;
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
	uint32_t temperature = 123; // For now
	uint8_t flag;
	Point p = touch.getpos(&flag);
	if (flag != 0)
	{
		p.x -= 270;
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