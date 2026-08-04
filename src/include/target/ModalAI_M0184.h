
#ifndef MODALAI_M0184_H
#define MODALAI_M0184_H

#include "ModalAI_Common.h"

#if !defined(__ASSEMBLER__)
extern const int16_t modalaiPowerValues[1];
#endif
#define MODALAI_POWER_OUTPUT_VALUES {114}
#define POWER_OUTPUT_VALUES modalaiPowerValues
#define POWER_OUTPUT_VALUES_COUNT 1
#define POWER_OUTPUT_VALUES2 modalaiPowerValues
#define POWER_OUTPUT_VALUES2_COUNT 1
#define MinPower PWR_10mW
#define MaxPower PWR_10mW
#define DefaultPower PWR_10mW

// Output Power - Default to SX1276 max output
// #define POWER_OUTPUT_FIXED 127 //MAX power for 900 RXes
// TODO: investigate why higher power doesn't work
#define POWER_OUTPUT_FIXED 114 // The highest power which didn't saturate (4dbm)
// #define POWER_OUTPUT_FIXED 113 //Low power (3dbm)

#ifndef DEVICE_NAME
    #define DEVICE_NAME "ModalAI M0184"
#endif

#define HARDWARE_REV 0x6D313834 // m0184

#endif // Header guard
