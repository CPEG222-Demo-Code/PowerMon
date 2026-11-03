#ifndef OLED_H
#define OLED_H

#include <stdint.h>

typedef enum {
	OLED_FONT_SMALL_5X7 = 0,
	OLED_FONT_MEDIUM_8X16 = 1
} oled_font_t;

void OLED_Init(void);
void OLED_Clear(void);
void OLED_Print(uint8_t page, uint8_t col, const char *s);
void OLED_PrintEx(uint8_t page, uint8_t col, const char *s, oled_font_t font);

#endif
