#ifndef NEIPIXEL_H
#define NEIPIXEL_H  

#include <stdint.h>

void TIM2_PWM_Init(void);
void DMA1CH3_Init(void);
void WS2812_Render(void);
void DMA1_Stream6_IRQHandler(void);
void Set_LED_Color(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
void Set_LED_Hue(uint8_t index, uint8_t WheelPos);
void Rainbow_Effect(uint32_t speed_ms);
#endif // NEIPIXEL_H  