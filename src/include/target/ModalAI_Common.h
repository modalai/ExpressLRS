/*
Credit to Jacob Walser (jaxxzer) for the pinout!!!
https://github.com/jaxxzer
*/

#ifndef MODALAI_COMMON_H
#define MODALAI_COMMON_H

#if !defined(__ASSEMBLER__)
#include <stdint.h>
#endif

#define TARGET_USE_EEPROM  1
#define TARGET_EEPROM_ADDR 0x50

#define GPIO_PIN_SDA            PB7  // EEPROM
#define GPIO_PIN_SCL            PB6  // EEPROM
#define USE_I2C

// /*  Radio 1
#define GPIO_PIN_NSS            PA4  // RADIO 1
#define GPIO_PIN_DIO0           PB5  // RADIO 1
#define GPIO_PIN_MOSI           PA7  // RADIO 1
#define GPIO_PIN_MISO           PA6  // RADIO 1
#define GPIO_PIN_SCK            PA5  // RADIO 1
#define GPIO_PIN_RST            PB2  // RADIO 1
// */

 // Radio 2
#define GPIO_PIN_NSS_2            PB12  // RADIO 2
#define GPIO_PIN_DIO0_2           PB4   // RADIO 2
#define GPIO_PIN_MOSI_2           PB15  // RADIO 2
#define GPIO_PIN_MISO_2           PB14  // RADIO 2
#define GPIO_PIN_SCK_2            PB13  // RADIO 2
#define GPIO_PIN_RST_2            PA15  // RADIO 2

#ifdef TARGET_TX
#define GPIO_PIN_RCSIGNAL_TX        PA9
#define GPIO_PIN_RCSIGNAL_RX        PA10 // TX used as internal module currently
#define GPIO_PIN_DEBUG_RX           PA3
#define GPIO_PIN_DEBUG_TX           PA2
// only used for external modules, not connected on internal module
#define GPIO_PIN_BUFFER_OE          PA11
#define GPIO_PIN_BUFFER_OE_INVERTED 0
#define GPIO_PIN_FAN_EN             PA8
#else
#define GPIO_PIN_RCSIGNAL_TX        PA9
#define GPIO_PIN_RCSIGNAL_RX        PA10
#define GPIO_PIN_DEBUG_RX           PA3
#define GPIO_PIN_DEBUG_TX           PA2
#endif

#define BACKPACK_LOGGING_BAUD       420000
#define PASSTHROUGH_BAUD            0
#define OPT_USE_TX_BACKPACK         false
#define GPIO_PIN_BACKPACK_BOOT      (-1)
#define GPIO_PIN_BACKPACK_EN        (-1)
#define GPIO_PIN_BOOT0              (-1)

// LED Pins
#define GPIO_PIN_LED_RED        PA12  // Red
#define GPIO_PIN_LED_GREEN      PB3   // Green
#define GPIO_PIN_BUTTON         PA1   // pullup e.g. LOW when pressed
#define GPIO_PIN_BUTTON2        (-1)
#define USER_BUTTON_LED         (-1)
#define USER_BUTTON2_LED        (-1)

#define GPIO_PIN_JOYSTICK       (-1)
#define JOY_ADC_VALUES          modalaiNoPowerValues
#define GPIO_PIN_FIVE_WAY_INPUT1 (-1)
#define GPIO_PIN_FIVE_WAY_INPUT2 (-1)
#define GPIO_PIN_FIVE_WAY_INPUT3 (-1)

#define GPIO_PIN_LED_BLUE       (-1)
#define GPIO_LED_BLUE_INVERTED  false
#define GPIO_LED_GREEN_INVERTED false
#define GPIO_LED_RED_INVERTED   false
#define GPIO_PIN_LED_WS2812     (-1)
#define OPT_WS2812_IS_GRB       false
#define WS2812_STATUS_LEDS      modalaiNoPowerValues
#define WS2812_STATUS_LEDS_COUNT 0
#define WS2812_VTX_STATUS_LEDS  modalaiNoPowerValues
#define WS2812_VTX_STATUS_LEDS_COUNT 0
#define WS2812_BOOT_LEDS        modalaiNoPowerValues
#define WS2812_BOOT_LEDS_COUNT  0

// PWM Channels
#define Ch1    PB0     // TIM3 CH3
#define Ch2    PB1     // TIM3 CH4
#define Ch3    PA8     // TIM1 CH1
#define Ch4    PA11    // TIM1 CH4

// No PWM on TX
#ifdef TARGET_RX
// External pads
// PWM
#define GPIO_PIN_PWM_OUTPUTS (int[]){Ch1, Ch2, Ch3, Ch4}
#define GPIO_PIN_PWM_OUTPUTS_COUNT 4
#define OPT_HAS_SERVO_OUTPUT true
#define OPT_PWM_OUT_ONLY false
#else
#define GPIO_PIN_PWM_OUTPUTS modalaiNoPowerValues
#define GPIO_PIN_PWM_OUTPUTS_COUNT 0
#define OPT_HAS_SERVO_OUTPUT false
#define OPT_PWM_OUT_ONLY false
// No PWM, use freerun if HWIL
#if defined(HWIL_TESTING)
#define DEBUG_TX_FREERUN
#endif
#endif

// Software inverter for half-duplex CRSF communication
#ifdef CRSF_INVERTER
#define INVERTER_HANDSET_PIN Ch2
#define INVERTER_RADIO_PIN Ch1

#define INVERTER_IRQ_HANDSET EXTI1_IRQn
#define INVERTER_IRQ_RADIO EXTI0_IRQn
#endif

#define M0139
#define DUAL_RADIO
#define STM32F1 1
#define STM32F1xx 1
#define SYSCLK_FREQ_72MHz
#define CUSTOM_DOMAIN_ENABLE
// #define GPIO_PIN_ANT_CTRL PB10 // Unused pin

#if !defined(__ASSEMBLER__)
extern const int16_t * const modalaiNoPowerValues;
#endif

#define GPIO_PIN_SERIAL1_RX       (-1)
#define GPIO_PIN_SERIAL1_TX       (-1)
#define GPIO_PIN_BUSY             (-1)
#define GPIO_PIN_BUSY_2           (-1)
#define GPIO_PIN_DIO1             (-1)
#define GPIO_PIN_DIO1_2           (-1)
#define GPIO_PIN_ANT_CTRL         (-1)
#define GPIO_PIN_ANT_GROUP        (-1)
#define GPIO_PIN_PA_ENABLE        (-1)
#define GPIO_PIN_RFamp_APC2       (-1)
#define GPIO_PIN_RX_ENABLE        (-1)
#define GPIO_PIN_TX_ENABLE        (-1)
#define GPIO_PIN_RX_ENABLE_2      (-1)
#define GPIO_PIN_TX_ENABLE_2      (-1)
#define LBT_RSSI_THRESHOLD_OFFSET_DB 0
#define OPT_USE_HARDWARE_DCDC     false
#define OPT_USE_SX1276_RFO_HF     false
#define LR1121_RFSW_CTRL          modalaiNoPowerValues
#define LR1121_RFSW_CTRL_COUNT    0
#define POWER_OUTPUT_DACWRITE     false
#define POWER_OUTPUT_VALUES_DUAL  modalaiNoPowerValues
#define POWER_OUTPUT_VALUES_DUAL_COUNT 0
#define OPT_HAS_VTX_SPI           false
#define OPT_HAS_SCREEN            false
#define OPT_HAS_OLED_I2C          false
#define OPT_HAS_OLED_SPI          false
#define OPT_HAS_TFT_SCREEN        false
#define OPT_SCREEN_REVERSED       false
#define OPT_SCREEN_MIRROR         false
#define GPIO_PIN_SCREEN_CS        (-1)
#define GPIO_PIN_SCREEN_DC        (-1)
#define GPIO_PIN_SCREEN_MOSI      (-1)
#define GPIO_PIN_SCREEN_RST       (-1)
#define GPIO_PIN_SCREEN_SCK       (-1)
#define GPIO_PIN_SCREEN_SDA       (-1)
#define GPIO_PIN_SCREEN_BL        (-1)
#define OPT_HAS_GSENSOR           false
#define OPT_HAS_GSENSOR_STK8xxx   false
#define GPIO_PIN_GSENSOR_INT      (-1)
#define OPT_HAS_THERMAL           false
#define OPT_HAS_THERMAL_LM75A     false
#define GPIO_PIN_FAN_PWM          (-1)
#define GPIO_PIN_FAN_TACHO        (-1)
#define GPIO_PIN_FAN_SPEEDS       modalaiNoPowerValues
#define GPIO_PIN_FAN_SPEEDS_COUNT 0

// Team race setting now persists across power cycles.
// Use "voxl-elrs configure" to reset it to disabled.

// #define DEV
#if defined(DEV)
#define DEBUG_LOG
#define DEBUG_RTT
//#define DEBUG_RX_SCOREBOARD
#endif

#endif // Header guard
