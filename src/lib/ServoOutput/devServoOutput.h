#pragma once
#if defined(GPIO_PIN_PWM_OUTPUTS) || defined(M0139)

#include "ServoMgr.h"

#include "telemetry.h"
#include "device.h"
#include "common.h"

extern device_t ServoOut_device;
#define HAS_SERVO_OUTPUT
#define OPT_HAS_SERVO_OUTPUT (GPIO_PIN_PWM_OUTPUTS_COUNT > 0)

#if defined(M0139)
extern bool updatePWM;
extern uint8_t pwmPin;
extern uint8_t pwmCmd;
extern uint8_t pwmOutputChannel;
extern uint8_t pwmInputChannel;
extern uint8_t pwmType;
extern uint16_t pwmValue; 
#endif // End M0139

// Notify this unit that new channel data has arrived
void servoNewChannelsAvaliable();
// Convert eServoOutputMode to microseconds, or 0 for non-servo modes
uint16_t servoOutputModeToUs(eServoOutputMode mode);
#else
inline void servoNewChannelsAvaliable(){};
#endif
