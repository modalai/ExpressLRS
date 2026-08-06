#pragma once
#if defined(TARGET_RX)

#include "device.h"
#include "common.h"
#include "telemetry_protocol.h"

#if defined(PLATFORM_ESP32)
#include "DShotRMT.h"
#endif

extern device_t ServoOut_device;

// Notify this unit that new channel data has arrived
void servoNewChannelsAvailable();
// Copy the current output values to the config's failsafe values
void servoCurrentToFailsafeConfig();

#if defined(M0139)
constexpr uint16_t MODAL_PWM_FAILSAFE_BASE_US = 800U;
bool servoApplyConfig(const modal_pwm_config_t &newConfig);
bool servoOverrideChannel(const modal_pwm_override_t &overrideValue);
uint16_t servoGetLastOutputUs(uint8_t ch);
void servoSetArmed(bool armed);
bool servoIsArmed();
#endif

#endif
