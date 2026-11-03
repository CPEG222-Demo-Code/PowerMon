#include "stm32f446xx.h"
#include "NeoPixel.h"

#define NUM_LEDS        4
#define BITS_PER_LED    24
#define RESET_SLOTS     50  // 50 * 1.25us = 62.5us reset pulse (>50us required)

// Total PWM buffer size: 4 LEDs * 24 bits + Reset period
#define PWM_BUF_SIZE    ((NUM_LEDS * BITS_PER_LED) + RESET_SLOTS)

// TIM2 Period = 225 clock cycles @ 180MHz -> 800 kHz (1.25us bit period)
#define PWM_PERIOD      225
#define PWM_BIT_0       72   // ~0.4us High (~32% duty cycle)
#define PWM_BIT_1       144  // ~0.8us High (~64% duty cycle)

typedef struct {
    uint8_t g;
    uint8_t r;
    uint8_t b;
} Pixel_t;

Pixel_t leds[NUM_LEDS];
uint32_t pwm_buffer[PWM_BUF_SIZE];

// --- 1. System Clock Configuration (180 MHz via HSE) ---


// --- 2. GPIO & Timer Configuration (TIM2 CH4 on PB11) ---
void TIM2_PWM_Init(void) {
    // Enable GPIOB and TIM2 clocks
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    // Set PB11 to Alternate Function mode (10)
    GPIOB->MODER &= ~(GPIO_MODER_MODE11);
    GPIOB->MODER |=  (2U << GPIO_MODER_MODE11_Pos);

    // Set PB11 to High Speed
    GPIOB->OSPEEDR |= (3U << GPIO_OSPEEDR_OSPEED11_Pos);

    // Set PB11 Alternate Function to AF1 (TIM2_CH4)
    GPIOB->AFR[1] &= ~(0xF << GPIO_AFRH_AFSEL11_Pos);
    GPIOB->AFR[1] |=  (0x1 << GPIO_AFRH_AFSEL11_Pos);

    // TIM2 Prescaler: APB1 Timer clock is 90MHz -> Prescaler = 0 (runs at 90MHz)
    TIM2->PSC = 0;

    // Auto-Reload Register: 90 MHz / 800 kHz = 112.5 -> Period = 112 - 1
    // (If using 180MHz TIM2, set ARR to 225 - 1)
    TIM2->ARR = 112 - 1;

    // Configure Channel 4 as PWM Mode 1 (110) & Enable Preload
    TIM2->CCMR2 &= ~(TIM_CCMR2_OC4M);
    TIM2->CCMR2 |=  (6U << TIM_CCMR2_OC4M_Pos) | TIM_CCMR2_OC4PE;

    // Enable Channel 4 output
    TIM2->CCER |= TIM_CCER_CC4E;

    // Enable TIM2 DMA Request on CC4 Event
    TIM2->DIER |= TIM_DIER_CC4DE;
}

// --- 3. DMA1 Stream 6 Configuration ---
void DMA1CH3_Init(void) {
    // Enable DMA1 Clock
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;

    // Disable Stream 6 before configuring
    DMA1_Stream6->CR &= ~DMA_SxCR_EN;
    while (DMA1_Stream6->CR & DMA_SxCR_EN);

    // Configure DMA1 Stream 6 Channel 3 (TIM2_CH4)
    DMA1_Stream6->CR = (3U << DMA_SxCR_CHSEL_Pos)  | // Channel 3
                       (2U << DMA_SxCR_PL_Pos)     | // High Priority
                       (2U << DMA_SxCR_MSIZE_Pos)  | // Memory size = 32-bit (Word)
                       (2U << DMA_SxCR_PSIZE_Pos)  | // Peripheral size = 32-bit (Word)
                       DMA_SxCR_MINC               | // Memory Increment
                       (1U << DMA_SxCR_DIR_Pos)    | // Direction: Memory-to-Peripheral
                       DMA_SxCR_TCIE;                // Transfer Complete Interrupt

    // Destination: TIM2 CCR4 Register Address
    DMA1_Stream6->PAR = (uint32_t)&(TIM2->CCR4);

    // Enable DMA Interrupt in NVIC
    NVIC_EnableIRQ(DMA1_Stream6_IRQn);
}

// --- 4. Render & DMA Start Function ---
void WS2812_Render(void) {
    uint32_t buf_idx = 0;

    // Populate PWM values for each LED (GRB order)
    for (int i = 0; i < NUM_LEDS; i++) {
        uint32_t color = (leds[i].g << 16) | (leds[i].r << 8) | leds[i].b;

        for (int8_t bit = 23; bit >= 0; bit--) {
            if (color & (1 << bit)) {
                pwm_buffer[buf_idx++] = 72; // ~65% Duty Cycle for '1'
            } else {
                pwm_buffer[buf_idx++] = 36; // ~32% Duty Cycle for '0'
            }
        }
    }

    // Fill remaining buffer with 0 for the Reset latch
    while (buf_idx < PWM_BUF_SIZE) {
        pwm_buffer[buf_idx++] = 0;
    }

    // Configure DMA Memory Address and Length
    DMA1_Stream6->M0AR = (uint32_t)pwm_buffer;
    DMA1_Stream6->NDTR = PWM_BUF_SIZE;

    // Clear flags and enable DMA Stream
    DMA1->HIFCR |= DMA_HIFCR_CTCIF6 | DMA_HIFCR_CDMEIF6 | DMA_HIFCR_CTEIF6;
    DMA1_Stream6->CR |= DMA_SxCR_EN;

    // Start Timer
    TIM2->CR1 |= TIM_CR1_CEN;
}

// --- 5. DMA Complete Interrupt Handler ---
void DMA1_Stream6_IRQHandler(void) {
    if (DMA1->HISR & DMA_HISR_TCIF6) {
        DMA1->HIFCR |= DMA_HIFCR_CTCIF6; // Clear flag

        // Disable DMA Stream and Timer upon completion
        DMA1_Stream6->CR &= ~DMA_SxCR_EN;
        TIM2->CR1 &= ~TIM_CR1_CEN;
    }
}

// --- Helper Functions & Main ---
void Set_LED_Color(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < NUM_LEDS) {
        leds[index].r = r;
        leds[index].g = g;
        leds[index].b = b;
    }
}
// Standard 256-color wheel helper function (maps 0-255 to an RGB color)
void Set_LED_Hue(uint8_t index, uint8_t WheelPos) {
    WheelPos = 255 - WheelPos;
    
    if (WheelPos < 85) {
        // Red -> Blue transition
        Set_LED_Color(index, 255 - WheelPos * 3, 0, WheelPos * 3);
    } else if (WheelPos < 170) {
        WheelPos -= 85;
        // Blue -> Green transition
        Set_LED_Color(index, 0, WheelPos * 3, 255 - WheelPos * 3);
    } else {
        WheelPos -= 170;
        // Green -> Red transition
        Set_LED_Color(index, WheelPos * 3, 255 - WheelPos * 3, 0);
    }
}

void Rainbow_Effect(uint32_t speed_ms) {
    static uint8_t start_hue = 0;

    // Distribute color wheel positions evenly across the 4 LEDs
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        uint8_t pixel_hue = start_hue + (i * (256 / NUM_LEDS));
        Set_LED_Hue(i, pixel_hue);
    }

    WS2812_Render(); // Transmit updated buffer to NeoPixels
    
    start_hue++;     // Shift colors for the next frame
    delay_ms(speed_ms);
}