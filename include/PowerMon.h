// ****************************************************************
// * Authors: T. LUM and R. MARTIN
// * CPEG222 PowerMon module, 11/3/26, V2
// * ADC3 voltage monitors and charger status pins for the shield
// *   VIN_MON   PA0  ADC3_IN0   divider 150k/100k (x2.5)
// *   VBAT_MON  PF10 ADC3_IN8   divider 100k/100k (x2.0)
// *   VMOT_MON  PF4  ADC3_IN14  divider 100k/100k (x2.0)
// *   CHARGING  PF1  low = charging
// *   DONE      PF0  low = charge complete
// * Revision history:
// *   V1  10/31/26  Original (float volts, ADC set up on every read)
// *   V2  11/3/26   ADC set up once, conversion timeout, pull-ups on
// *                 PF0/PF1, divider ratios as defines, integer mV API
// ****************************************************************
#ifndef POWERMON_H
#define POWERMON_H

#include "stm32f4xx.h"

void PowerMon_Init(void);

// Millivolts (integer), the preferred API
uint32_t ReadInputVoltage_mV(void);
uint32_t ReadMotorVoltage_mV(void);
uint32_t ReadBatteryVoltage_mV(void);

// Volts (float), same signatures as V1
float ReadInputVoltage(void);
float ReadMotorVoltage(void);
float ReadBatteryVoltage(void);

uint16_t ReadAnalogValue(void);   // raw 12-bit count on PC2 (ADC3_IN12)
int ReadChargeState(void);        // 1 = charging
int ReadDoneState(void);          // 1 = charge complete

#endif // POWERMON_H
