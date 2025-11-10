#pragma once
#if defined(GPIO_PIN_PWM_OUTPUTS)

#include "device.h"
#include "common.h"
#include "telemetry.h"

#if (defined(PLATFORM_ESP32))
#include "DShotRMT.h"
#endif

extern device_t ServoOut_device;
#define HAS_SERVO_OUTPUT
#define OPT_HAS_SERVO_OUTPUT (GPIO_PIN_PWM_OUTPUTS_COUNT > 0)


extern bool updatePWM;
extern rx_pwm_config_in pwmInput;
extern bool overridePWM;
extern pwm_val_override_t pwmOverride;
extern bool pwmIsArmed;

uint16_t servoGetLastOutputUs(uint8_t ch);

// Notify this unit that new channel data has arrived
void servoNewChannelsAvailable();
// Re-initialize PWM outputs (called when configuration changes)
void reinitializePWM();
#else
inline void servoNewChannelsAvailable(){};
inline uint16_t servoGetLastOutputUs(uint8_t) { return 0; }
#endif
