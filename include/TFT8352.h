#ifndef __TFT8352_H
#define __TFT8352_H


#if ARDUINO >= 100
 #include "Arduino.h"
#else
 #include "WProgram.h"
#endif

//#define ELECFREAKS_TFT_SHIELD_V2
#define ELECHOUSE_DUE_TFT_SHIELD_V1
#include "pin_magic.h"

#define FONTSIZE_48
#include "FontDef.h"
class TFT8352
{
public:
	TFT8352();
	void begin();
	void drawPixel(int16_t x, int16_t y, uint16_t color);
	void reset();
	void writeRegister16(uint16_t reg, uint16_t dat);
	void writeCommand16(uint16_t cmd);
	void writeData16(uint16_t dat);
	void setAddrWindow(int x1, int y1, int x2, int y2);
	void setRotation(uint8_t x);
	void flood(uint16_t color, uint32_t len);
	void fillScreen(uint16_t color);
	void pushColors(uint16_t *data, uint32_t len, boolean first=true);
	uint16_t color565(uint8_t r, uint8_t g, uint8_t b);
	void drawPic(uint8_t *data, uint32_t len);
	void drawFigure(int16_t x, int16_t y, uint8_t c);
	void renderChar(int16_t x, int16_t y, unsigned char c, uint16_t color, 
		uint16_t bg, uint8_t size, uint16_t* buffer, uint16_t row_length);
private:
	uint8_t rotation;    
};



#endif