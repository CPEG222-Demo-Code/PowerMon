// ****************************************************************
// * Authors: T. LUM and R. MARTIN
// * CPEG222 Power Monitor, 11/3/26, V2
// * NucleoF446ZE CMSIS STM32F4xx
// * Reads the input, motor, and battery voltages and the charger
// * status, shows them on the OLED, and colors the NeoPixels by
// * battery level.
// * Revision history:
// *   V1  10/31/26  Original (blocking delay, ADC set up on every read)
// *   V2  11/3/26   SysTick timing instead of NOP delay, battery
// *                 voltage smoothed, NeoPixel color clamped
// ****************************************************************
#include "stm32f4xx.h"
#include "SysClock180MHz.h"
#include "OLED.h"
#include "PowerMon.h"
#include "NeoPixel.h"
#include <stdio.h>
#define LEDS_AMOUNT 4

#define LOW_V       3.5f   // At or below: solid red
#define GOOD_V      4.0f   // At or above: solid green
#define BRIGHTNESS  100    // 0 to 255
#define UPDATE_MS   200    // Display and LED update period
#define FILTER      0.2f   // Smoothing: 0 = frozen, 1 = no smoothing

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} Led;
typedef struct {
    Led *led;
    uint16_t size;
    volatile uint8_t state;
    uint32_t *data;
} LedStrip;

volatile uint32_t ms_ticks = 0;   // Milliseconds since start

void SysTick_Handler(void) {
    ms_ticks++;
}

// Blocking delay. Not static: NeoPixel.c calls it (Rainbow_Effect).
// Needs SysTick running, so call SysTick_Config first.
void delay_ms(uint32_t ms) {
    uint32_t start = ms_ticks;
    while (ms_ticks - start < ms) { }
}

// Red at LOW_V, green at GOOD_V, blend in between, clamped outside
static void BatteryColor(float v, int *r, int *g, int *b) {
    float t = (v - LOW_V) / (GOOD_V - LOW_V);   // 0 = low, 1 = good
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    *r = (int)(BRIGHTNESS * (1.0f - t));
    *g = (int)(BRIGHTNESS * t);
    *b = 0;
}

int main(void) {
    SystemClock_Config();            // 16 MHz to 180 MHz
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000);   // 1 ms tick

    OLED_Init();
    OLED_Print(0, 0, "CPEG222 - 11/3/2026");
    OLED_Print(1, 0, "F26 Power Monitor");
    PowerMon_Init();                 // ADC3 and charger pins, set up once
    TIM2_PWM_Init();                 // TIM2 PWM output for the NeoPixels
    DMA1CH3_Init();                  // DMA transfers the PWM data to TIM2

    for (int i = 0; i < 255; ++i) {
        Rainbow_Effect(5);           // Start-up rainbow
    }
    delay_ms(200);
    Set_LED_Color(0, 255, 0, 0);     // Red
    Set_LED_Color(1, 0, 255, 0);     // Green
    Set_LED_Color(2, 0, 0, 255);     // Blue
    Set_LED_Color(3, 150, 150, 0);   // Yellow
    WS2812_Render();
    delay_ms(200);

    char buffer[40];
    float battFiltered = ReadBatteryVoltage();   // Seed the filter
    uint32_t lastUpdate = ms_ticks;

    while (1) {
        if (ms_ticks - lastUpdate < UPDATE_MS) continue;
        lastUpdate += UPDATE_MS;

        float inputVoltage = ReadInputVoltage();
        float motorVoltage = ReadMotorVoltage();
        float batteryVoltage = ReadBatteryVoltage();
        int chargingState = ReadChargeState();
        int doneState = ReadDoneState();

        battFiltered += FILTER * (batteryVoltage - battFiltered);   // Smooth load dips

        snprintf(buffer, sizeof(buffer), "  Input : %.3f V  ", inputVoltage);
        OLED_Print(2, 0, buffer);
        snprintf(buffer, sizeof(buffer), "  Motor : %.3f V  ", motorVoltage);
        OLED_Print(3, 0, buffer);
        snprintf(buffer, sizeof(buffer), "  Batt  : %.3f V  ", battFiltered);
        OLED_Print(4, 0, buffer);
        snprintf(buffer, sizeof(buffer), "  Charge: %s  ", chargingState ? "Yes" : "No");
        OLED_Print(5, 0, buffer);
        snprintf(buffer, sizeof(buffer), "  Done  : %s  ", doneState ? "Yes" : "No");
        OLED_Print(6, 0, buffer);

        int r, g, b;
        BatteryColor(battFiltered, &r, &g, &b);
        for (int i = 0; i < LEDS_AMOUNT; i++) {
            Set_LED_Color(i, r, g, b);
        }
        WS2812_Render();
    }
    return 0;
}
