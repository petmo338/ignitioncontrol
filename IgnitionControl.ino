#include "DueTimerLite.h"
#include "TFT8352.h"
#include "ads7843.h"

#define Serial SerialUSB



#define RPM_X_OFFSET 80
#define RPM_Y_OFFSET 80
#define BIAS_X_OFFSET 80
#define BIAS_Y_OFFSET (80 + FONTHEIGHT) 
#define SLOPE_X_OFFSET 80
#define SLOPE_Y_OFFSET (80 + FONTHEIGHT * 2)
#define TEMP_X_OFFSET 80
#define TEMP_Y_OFFSET (80 + FONTHEIGHT * 3)

DueTimerLite dueTimer(0);
TFT8352 tft;
uint32_t last_tft_update;

ADS7843 touch(26, 25, 27, 29, 30);

void setup()
{
	dueTimer.attachInterrupt(my_int_handler);
	dueTimer.setFrequency(300);
	dueTimer.start();
	tft.begin();
	tft.setRotation(0);
	tft.fillScreen(0xffff);
	Serial.begin(115200);
	touch.begin();
	update_tft();

}

void loop()
{

  /* add main program code here */

}

void my_int_handler()
{
	int i=0;
	if (i > 123)
		return;
}

void update_tft()
{
	uint32_t pre_ignition_bias = 0;
	uint32_t pre_ignition_slope = 0;
	uint32_t temperature = 123;

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
				// handle sign
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

		//Serial.print("getpos, x: ");
		//Serial.print(p.x, DEC);
		//Serial.print(", y: ");
		//Serial.println(p.y, DEC);
	}
	uint16_t rpm = 1234 / 6;
	if (false == true)
	{
		tft.setAddrWindow(RPM_X_OFFSET - FONTWIDTH, RPM_Y_OFFSET, RPM_X_OFFSET, RPM_Y_OFFSET + FONTHEIGHT);
		tft.flood(0, FONTHEIGHT * FONTWIDTH);
	}
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