// ****************************************************************
// * Authors: T. LUM and R. MARTIN
// * CPEG222 PowerMon module, 11/3/26, V2
// * See PowerMon.h for the pin list and revision history.
// ****************************************************************
#include "stm32f4xx.h"
#include "PowerMon.h"

// ---- Pins
#define BATT_V_PIN      10
#define BATT_V_PORT     GPIOF
#define MOTOR_V_PIN     4
#define MOTOR_V_PORT    GPIOF
#define INPUT_V_PIN     0
#define INPUT_V_PORT    GPIOA
#define ANALOG_PIN      2
#define ANALOG_PORT     GPIOC
#define CHRG_PIN        1
#define CHRG_PORT       GPIOF
#define DONE_PIN        0
#define DONE_PORT       GPIOF

// ---- ADC3 channels
#define CH_INPUT        0     // PA0
#define CH_BATT         8     // PF10
#define CH_ANALOG       12    // PC2
#define CH_MOTOR        14    // PF4

// ---- Conversion constants
#define VREF_MV         3300UL   // ADC reference (Nucleo 3.3 V supply)
#define ADC_MAX         4095UL   // 12-bit full scale
#define ADC_SAMPLES     16       // Readings averaged per measurement
#define ADC_TIMEOUT     100000UL // Loop guard so a stuck ADC cannot hang main

// Divider ratio = Vin / Vpin = (Rtop + Rbottom) / Rbottom
#define VIN_NUM         5UL      // (150k + 100k) / 100k = 5/2
#define VIN_DEN         2UL
#define VBAT_NUM        2UL      // (100k + 100k) / 100k = 2/1
#define VBAT_DEN        1UL
#define VMOT_NUM        2UL      // (100k + 100k) / 100k = 2/1
#define VMOT_DEN        1UL

static void SetAnalog(GPIO_TypeDef *port, int pin) {
    port->MODER |= (0x3UL << (pin * 2));        // 0b11 = analog
}

static void SetInputPullUp(GPIO_TypeDef *port, int pin) {
    port->MODER &= ~(0x3UL << (pin * 2));       // 0b00 = input
    port->PUPDR &= ~(0x3UL << (pin * 2));
    port->PUPDR |=  (0x1UL << (pin * 2));       // 0b01 = pull-up
}

void PowerMon_Init(void) {
    // Clocks
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIOFEN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC3EN;

    // Analog pins
    SetAnalog(INPUT_V_PORT, INPUT_V_PIN);
    SetAnalog(BATT_V_PORT, BATT_V_PIN);
    SetAnalog(MOTOR_V_PORT, MOTOR_V_PIN);
    SetAnalog(ANALOG_PORT, ANALOG_PIN);

    // Charger status pins: inputs with pull-ups (the charger outputs are
    // active low, so a floating pin must read high = not charging)
    SetInputPullUp(CHRG_PORT, CHRG_PIN);
    SetInputPullUp(DONE_PORT, DONE_PIN);

    // ADC3, configured once. PCLK2 / 4 keeps the ADC clock under 36 MHz.
    ADC->CCR = (ADC->CCR & ~ADC_CCR_ADCPRE) | ADC_CCR_ADCPRE_0;
    ADC3->CR1 = 0;                       // 12-bit, no scan, no interrupts
    ADC3->CR2 = 0;                       // single conversion, right aligned, no DMA
    ADC3->SQR1 = 0;                      // L = 0, one conversion per sequence
    // Maximum sample time (480 cycles) on all four channels: the dividers
    // present about 50 kohm, so a long sample time keeps readings accurate
    ADC3->SMPR2 = (7UL << (CH_INPUT * 3)) | (7UL << (CH_BATT * 3));
    ADC3->SMPR1 = (7UL << ((CH_ANALOG - 10) * 3)) | (7UL << ((CH_MOTOR - 10) * 3));
    ADC3->CR2 |= ADC_CR2_ADON;           // Power on the ADC
    for (volatile int i = 0; i < 1000; ++i) { __NOP(); }   // Wait for tSTAB (a few us)
}

// Average of ADC_SAMPLES conversions on one channel (0 to 4095).
// Returns 0xFFFF if the ADC never finishes (timeout).
static uint16_t ReadChannel(uint8_t channel) {
    ADC3->SQR3 = channel;
    uint32_t total = 0;
    for (int i = 0; i < ADC_SAMPLES; i++) {
        ADC3->CR2 |= ADC_CR2_SWSTART;
        uint32_t guard = ADC_TIMEOUT;
        while (!(ADC3->SR & ADC_SR_EOC)) {
            if (--guard == 0) return 0xFFFF;
        }
        total += ADC3->DR;               // Reading DR clears EOC
    }
    return (uint16_t)(total / ADC_SAMPLES);
}

// Counts to millivolts at the pin, then undo the divider
static uint32_t CountsToMilliVolts(uint16_t counts, uint32_t num, uint32_t den) {
    if (counts == 0xFFFF) return 0;
    uint32_t pin_mV = ((uint32_t)counts * VREF_MV) / ADC_MAX;
    return (pin_mV * num) / den;
}

uint32_t ReadInputVoltage_mV(void)   { return CountsToMilliVolts(ReadChannel(CH_INPUT), VIN_NUM, VIN_DEN); }
uint32_t ReadMotorVoltage_mV(void)   { return CountsToMilliVolts(ReadChannel(CH_MOTOR), VMOT_NUM, VMOT_DEN); }
uint32_t ReadBatteryVoltage_mV(void) { return CountsToMilliVolts(ReadChannel(CH_BATT), VBAT_NUM, VBAT_DEN); }

float ReadInputVoltage(void)   { return ReadInputVoltage_mV() / 1000.0f; }
float ReadMotorVoltage(void)   { return ReadMotorVoltage_mV() / 1000.0f; }
float ReadBatteryVoltage(void) { return ReadBatteryVoltage_mV() / 1000.0f; }

uint16_t ReadAnalogValue(void) { return ReadChannel(CH_ANALOG); }

int ReadChargeState(void) {
    return (CHRG_PORT->IDR & (1UL << CHRG_PIN)) ? 0 : 1;   // low = charging
}

int ReadDoneState(void) {
    return (DONE_PORT->IDR & (1UL << DONE_PIN)) ? 0 : 1;   // low = done
}
