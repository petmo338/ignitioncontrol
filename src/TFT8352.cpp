#include "TFT8352.h"
#include "glcdfont.c"


#define TFTWIDTH   240
#define TFTHEIGHT  400


TFT8352::TFT8352() : rotation(0)
{
#ifdef ELECFREAKS_TFT_SHIELD_V2
	for(int p=22; p<42; p++) {
		pinMode(p,OUTPUT);
	}
	for(int p=2; p<7; p++) {
		pinMode(p,OUTPUT);
	}
#elif defined ELECHOUSE_DUE_TFT_SHIELD_V1
	for(int p=34; p<42; p++) {
		pinMode(p,OUTPUT);
	}
	for(int p=44; p<52; p++) {
		pinMode(p,OUTPUT);
	}
	
	pinMode(23,OUTPUT);
	pinMode(24,OUTPUT);
	pinMode(22,OUTPUT);
	pinMode(31,OUTPUT);
	pinMode(33,OUTPUT);
	
#else
#error "initial io here"
#endif
	DB_WR_EN();
	RD_IDLE;
}

void TFT8352::begin()
{
	reset();
	CS_ACTIVE;
	writeRegister16(0x0083, 0x0002); // Test mode on
	writeRegister16(0x0085, 0x0003);
	writeRegister16(0x008B, 0x0001);
	writeRegister16(0x008C, 0x0093);
	writeRegister16(0x0091, 0x0001);
	writeRegister16(0x0083, 0x0000); // Test mode off
	writeRegister16(0x003E, 0x00B0); // Gamma control 1
	writeRegister16(0x003F, 0x0003); // Gamma control 2
	writeRegister16(0x0040, 0x0010); // Gamma control 3
	writeRegister16(0x0041, 0x0056); // Gamma control 4
	writeRegister16(0x0042, 0x0013); // Gamma control 5
	writeRegister16(0x0043, 0x0046); // Gamma control 6
	writeRegister16(0x0044, 0x0023); // Gamma control 7
	writeRegister16(0x0045, 0x0076); // Gamma control 8
	writeRegister16(0x0046, 0x0000); // Gamma control 9
	writeRegister16(0x0047, 0x005E); // Gamma control 10
	writeRegister16(0x0048, 0x004F); // Gamma control 11
	writeRegister16(0x0049, 0x0040); // Gamma control 12

	writeRegister16(0x0017, 0x0091); // Osc control 1
	writeRegister16(0x002B, 0x00F9); // Cycle control 1
	delay(10);

	writeRegister16(0x001B, 0x0014); // Power control 3
	writeRegister16(0x001A, 0x0011); // Power control 2
	writeRegister16(0x001C, 0x0006); // Power control 4
	writeRegister16(0x001F, 0x0042); // VCOM control
	delay(20);

	writeRegister16(0x0019, 0x000A); // Power control 1
	writeRegister16(0x0019, 0x001A); // Power control 1
	delay(40);

	writeRegister16(0x0019, 0x0012); // Power control 1
	delay(40);

	writeRegister16(0x001E, 0x0027); // Power control 6
	delay(100);

	writeRegister16(0x0024, 0x0060); // Display control 2
	writeRegister16(0x003D, 0x0040); // Source control 2
	writeRegister16(0x0034, 0x0038); // Cycle control 10
	writeRegister16(0x0035, 0x0038); // Cycle control 11
	writeRegister16(0x0024, 0x0038); // Display control 2
	delay(40);

	writeRegister16(0x0024, 0x003C); // Display control 2

	writeRegister16(0x0016, 0x001C); // Memory access control

	writeRegister16(0x0001, 0x0006); // Display Mode
	writeRegister16(0x0055, 0x0000); // Panel Control

	writeRegister16(0x0002, 0x0000); // Window bounds ...
	writeRegister16(0x0003, 0x0000);
	writeRegister16(0x0004, 0x0000);
	writeRegister16(0x0005, 0x00ef);
	writeRegister16(0x0006, 0x0000);
	writeRegister16(0x0007, 0x0000);
	writeRegister16(0x0008, 0x0001);
	writeRegister16(0x0009, 0x008f);

	CS_IDLE;

	rotation = 0;
}

void TFT8352::writeRegister16(uint16_t reg, uint16_t dat)
{
	CD_COMMAND;
	write16(reg);
	CD_DATA;
	write16(dat);
}

void TFT8352::writeCommand16(uint16_t cmd)
{
	CD_COMMAND;
	write16(cmd);
}

void TFT8352::writeData16(uint16_t dat)
{
	CD_DATA;
	write16(dat);
}

void TFT8352::setRotation(uint8_t x)
{
	rotation = x;
	CS_ACTIVE;
	switch(x) {
	case 0://All zero
	default:
		/**
			START: top left. END bottom right.
			DIR: top left --> top right
		*/
		writeRegister16(0x0016, 0x001C);
		break;
	case 1://MV = 0, MX = 1, MY = 0
		/**
			START: top right. END bottom left.
			DIR: top right --> top left
		*/
		writeRegister16(0x0016, 0x005C);
		break;
	case 3://MV = 0, MX = 0, MY = 1
		/**
			START: bottom left. END top right.
			DIR: bottom left --> bottom right
		*/
		writeRegister16(0x0016, 0x009C);
		break;
		break;
	case 2://MV = 0, MX = 1, MY = 1
		/**
			START: bottom right.  END top left.
			DIR: bottom right --> bottom left
		*/
		writeRegister16(0x0016, 0x00DC);
		break;
	}

	CS_IDLE;
}


void TFT8352::setAddrWindow(int x1, int y1, int x2, int y2)
{
	int x_b, y_b, x_e, y_e;

    switch(rotation) {
	case 0:
	default:
		x_b = x1;
		y_b = y1;
		x_e = x2;
		y_e = y2;
		break;
	case 1:
        x_b = y1;
		y_b = x1;
		x_e = y2;
		y_e = x2;
		break;
	case 3:
		x_b = y1;
		y_b = x1;
		x_e = y2;
		y_e = x2;
		break;
	case 2:
        x_b = x1;
		y_b = y1;
		x_e = x2;
		y_e = y2;
		break;
	}

	CS_ACTIVE;
	writeRegister16(0x0002,x_b>>8);
	writeRegister16(0x0003,x_b);
	writeRegister16(0x0006,y_b>>8);
	writeRegister16(0x0007,y_b);
	writeRegister16(0x0004,x_e>>8);
	writeRegister16(0x0005,x_e);
	writeRegister16(0x0008,y_e>>8);
	writeRegister16(0x0009,y_e);
	CS_IDLE;
}

void TFT8352::drawPixel(int16_t x, int16_t y, uint16_t color)
{
	setAddrWindow(x, y, x, y);
	CS_ACTIVE;
	writeCommand16(0x0022);
	writeData16(color);
	CS_IDLE;
}

void TFT8352::reset()
{
	CS_IDLE;
	CD_DATA;
	WR_IDLE;
	RD_IDLE;

	RST_IDLE;
	delay(5);
	RST_ACTIVE;
	delay(15);
	RST_IDLE;
	delay(15);
}

void TFT8352::flood(uint16_t color, uint32_t len)
{
	CS_ACTIVE;
	writeCommand16(0x0022);
	CD_DATA;
	for(uint32_t i = 0; i < len; i++)
	{
		write16(color);
	}
	CS_IDLE;
}

void TFT8352::fillScreen(uint16_t color)
{
	setAddrWindow(0, 0, TFTWIDTH - 1, TFTHEIGHT -  1);
    flood(color, (long)(TFTWIDTH * TFTHEIGHT));
}

void TFT8352::pushColors(uint16_t *data, uint32_t len, boolean first)
{
	CS_ACTIVE;
	if(first == true)
	{
		writeCommand16(0x0022);
	}
	CD_DATA;
	for(uint32_t i = 0; i < len; i++)
	{
		write16(data[i]);
	}
	CS_IDLE;
}

void TFT8352::drawPic(uint8_t *data, uint32_t len)
{
	uint16_t color;
	CS_ACTIVE;
	writeCommand16(0x0022);
	CD_DATA;
	for(uint32_t i = 0; i < len; i++) {
		color = ((uint16_t)(*data))|(((uint16_t)(*(data+1)))<<8);
		write16(color);
		data += 2;
	}
	CS_IDLE;
}

void TFT8352::drawFigure(int16_t x, int16_t y, uint8_t c)
{
	setAddrWindow(x, y, x + FONTWIDTH - 1, y + FONTHEIGHT);
	uint8_t buffer[FONTWIDTH * FONTHEIGHT * sizeof(uint16_t)];
	uint8_t* startAddr;
	if (c > 9)
	{
		c = 0;
	}
	if (c == 0)
	{
		startAddr = (uint8_t*)FONT + 9 * FONTWIDTH * sizeof(uint16_t);
	}
	else
	{
		startAddr = (uint8_t*)FONT + (c - 1) * FONTWIDTH * sizeof(uint16_t);		
	}
	for (int row = 0; row < FONTHEIGHT; ++row)
	{
		memcpy(buffer + row * FONTWIDTH * sizeof(uint16_t), startAddr + (row * (10 * FONTWIDTH)) * sizeof(uint16_t), FONTWIDTH * sizeof(uint16_t));	
	}
	drawPic(buffer, FONTWIDTH * FONTHEIGHT);
}

uint16_t TFT8352::color565(uint8_t r, uint8_t g, uint8_t b)
{
	return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// draw a character
void TFT8352::renderChar(int16_t x, int16_t y, unsigned char c,
uint16_t color, uint16_t bg, uint8_t size, uint16_t* buffer, uint16_t row_length) {

	for (uint8_t i = 0; i < 6; i++)
	{
		uint8_t line;
		if (i == 5)
		{
			line = 0x0;		
		}
		else
		{
			line = font[(c * 5) + i];
		}
		
		for (uint8_t j = 0; j < 8; j++)
		{
			uint16_t paint_color = line & 1 ? color : bg;
			for (int c_x = 0; c_x < size; ++c_x)
			{
				for (int c_y = 0; c_y < size; ++c_y)
				{
					buffer[(x + (i * size) + c_x) + ((y + (j * size) + c_y) * (row_length >> 0))] = paint_color;
				}
								
			}
			line >>= 1;
		}
	}
}