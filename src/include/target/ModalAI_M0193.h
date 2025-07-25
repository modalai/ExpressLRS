
#ifndef MODALAI_M0193_H
#define MODALAI_M0193_H

#include "ModalAI_Common.h"

// Output Power - Default to SX1276 max output
// #define POWER_OUTPUT_FIXED 0 // -4 dbm input to FEM
#define MinPower                PWR_250mW
#define MaxPower                PWR_1000mW
#define DefaultPower            PWR_250mW

// -4.2 dbm input to FEM
// 0 = -4.2+30=25.8 dBm ~= 300mW
// 2 = -4.2+30=27.8 dBm ~= 600mW
// 4 = -4.2+30=29.8 dBm ~= 1000mW
#define POWER_OUTPUT_VALUES     {0, 2, 5} 

#define USE_SX1276_RFO_HF
#define OPT_USE_SX1276_RFO_HF true

#ifndef DEVICE_NAME
    #define DEVICE_NAME "ModalAI M0193"
#endif

#define HARDWARE_REV 0x6D313933 // m0184

#endif // Header guard