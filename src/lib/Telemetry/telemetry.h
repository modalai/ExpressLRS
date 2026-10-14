#pragma once

#include <cstdint>
#include "crsf_protocol.h"
#include "CRSF.h"
#include "config.h"

enum CustomTelemSubTypeID : uint8_t {
    CRSF_AP_CUSTOM_TELEM_SINGLE_PACKET_PASSTHROUGH = 0xF0,
    CRSF_AP_CUSTOM_TELEM_STATUS_TEXT = 0xF1,
    CRSF_AP_CUSTOM_TELEM_MULTI_PACKET_PASSTHROUGH = 0xF2,
};

enum PWMCmd : uint8_t {
    SET_PWM_CH = 0xF3,
    SET_PWM_VAL = 0xF4,
    SET_PIN_OUTPUT = 0xF5,
    SET_PWM_DEFAULT = 0xFF
};

typedef struct pwm_val_override_t {
    uint8_t command;
    uint8_t rc_channel;
    uint16_t crsf_channel_value;
} __attribute__((packed)) pwm_val_override_t;

// Result of a SET_PIN_OUTPUT command, returned to the FC in the acknowledgement
enum PinOutputStatus : uint8_t {
    PIN_OUTPUT_OK = 0,          // the pin outputs the commanded value
    PIN_OUTPUT_RELEASED = 1,    // the pin follows its input channel or failsafe again
    PIN_OUTPUT_NEEDS_ARM = 2,   // accepted, but the pin holds its failsafe until the outputs are armed
    PIN_OUTPUT_BAD_PIN = 3,     // no such pin
    PIN_OUTPUT_BAD_MODE = 4,    // the pin's output mode is not servo PWM, on/off or duty
    PIN_OUTPUT_BAD_VALUE = 5,   // the value is outside PIN_OUTPUT_MIN_US..PIN_OUTPUT_MAX_US
};

// Drives one pin with a raw pulse width, bypassing its input channel and map.
// The pin returns to its input channel or failsafe 1 s after the last command,
// or at once when us_output is 0.
typedef struct pin_output_t {
    uint8_t command;    // SET_PIN_OUTPUT
    uint8_t pin_index;  // 0-based
    uint16_t us_output; // big-endian
} __attribute__((packed)) pin_output_t;

// The RX answers every SET_PIN_OUTPUT with this frame, sent to the FC
typedef struct pin_output_ack_t {
    uint8_t command;    // SET_PIN_OUTPUT
    uint8_t pin_index;
    uint8_t status;     // PinOutputStatus
    uint16_t us_output; // big-endian, what the pin outputs now (0 for no pulses or unknown)
} __attribute__((packed)) pin_output_ack_t;

constexpr uint16_t PIN_OUTPUT_MIN_US = 800U;
constexpr uint16_t PIN_OUTPUT_MAX_US = 2200U;

typedef enum {
    TELEMETRY_IDLE = 0,
    RECEIVING_LENGTH,
    RECEIVING_DATA
} telemetry_state_s;

typedef struct crsf_telemetry_package_t {
    const uint8_t type;
    const uint8_t size;
    volatile bool locked;
    volatile bool updated;
    uint8_t *data;
} crsf_telemetry_package_t;

#define PAYLOAD_DATA(type0, type1, type2, type3, type4, type5, type6)\
    uint8_t PayloadData[\
        CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type0##_PAYLOAD_SIZE) + \
        CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type1##_PAYLOAD_SIZE) + \
        CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type2##_PAYLOAD_SIZE) + \
        CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type3##_PAYLOAD_SIZE) + \
        CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type4##_PAYLOAD_SIZE) + \
        CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type5##_PAYLOAD_SIZE) + \
        CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type6##_PAYLOAD_SIZE) + \
        CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_GENERAL_RESP_PAYLOAD_SIZE) + \
        CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_GENERAL_RESP_PAYLOAD_SIZE)]; \
    crsf_telemetry_package_t payloadTypes[] = {\
    {CRSF_FRAMETYPE_##type0, CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type0##_PAYLOAD_SIZE), false, false, 0},\
    {CRSF_FRAMETYPE_##type1, CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type1##_PAYLOAD_SIZE), false, false, 0},\
    {CRSF_FRAMETYPE_##type2, CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type2##_PAYLOAD_SIZE), false, false, 0},\
    {CRSF_FRAMETYPE_##type3, CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type3##_PAYLOAD_SIZE), false, false, 0},\
    {CRSF_FRAMETYPE_##type4, CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type4##_PAYLOAD_SIZE), false, false, 0},\
    {CRSF_FRAMETYPE_##type5, CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type5##_PAYLOAD_SIZE), false, false, 0},\
    {CRSF_FRAMETYPE_##type6, CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type6##_PAYLOAD_SIZE), false, false, 0},\
    {0, CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_GENERAL_RESP_PAYLOAD_SIZE), false, false, 0},\
    {0, CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_GENERAL_RESP_PAYLOAD_SIZE), false, false, 0}};\
    const uint8_t payloadTypesCount = (sizeof(payloadTypes)/sizeof(crsf_telemetry_package_t))

class Telemetry
{
public:
    Telemetry();
    bool RXhandleUARTin(uint8_t data);
    void ResetState();
    bool ShouldCallBootloader();
    bool ShouldCallEnterBind();
    bool ShouldCallUnbind();
    bool ShouldCallUpdatePWM();
    bool ShouldCallOverridePWM();
    bool ShouldCallPinOutput();
    bool ShouldCallUpdateModelMatch();
    bool ShouldSendDeviceFrame();
    void CheckCrsfBatterySensorDetected();
    void SetCrsfBatterySensorDetected();
    bool GetCrsfBatterySensorDetected() { return crsfBatterySensorDetected; };
    void CheckCrsfBaroSensorDetected();
    void SetCrsfBaroSensorDetected();
    bool GetCrsfBaroSensorDetected() { return crsfBaroSensorDetected; };
    uint8_t GetUpdatedModelMatch() { return modelMatchId; }
    bool GetNextPayload(uint8_t* nextPayloadSize, uint8_t **payloadData);
    uint8_t UpdatedPayloadCount();
    uint8_t ReceivedPackagesCount();
    bool AppendTelemetryPackage(uint8_t *package);
    bool ShouldCallUpdateUID();
    uint8_t * GetNewUID(){ return newUID;}
    bool ShouldCallParameterRequest();
    uint8_t GetParameterRequestType() { return paramRequestType; }
    uint8_t GetParameterRequestIndex() { return paramRequestIndex; }
    uint8_t GetParameterRequestArg() { return paramRequestArg; }
    const uint8_t* GetParameterRequestData() { return CRSFinBuffer; }  // Returns full packet for STRING parameter writes
#if defined(TARGET_RX)
    rx_pwm_config_in GetPwmInput(){ return pwmInput;}
    pwm_val_override_t GetPwmOverride(){ return pwmValueOverride; }
#endif
    pin_output_t GetPinOutput(){ return pinOutputCommand; }
private:
    bool processInternalTelemetryPackage(uint8_t *package);
    void AppendToPackage(volatile crsf_telemetry_package_t *current);
    uint8_t CRSFinBuffer[CRSF_MAX_PACKET_LEN];
    telemetry_state_s telemetry_state;
    uint8_t currentTelemetryByte;
    uint8_t currentPayloadIndex;
    uint8_t twoslotLastQueueIndex;
    volatile crsf_telemetry_package_t *telemetryPackageHead;
    uint8_t receivedPackages;
    uint8_t newUID[6];
    bool callBootloader;
    bool callEnterBind;
    bool callUnbind;
    bool callUpdateModelMatch;
    bool sendDeviceFrame;
    bool crsfBatterySensorDetected;
    bool crsfBaroSensorDetected;
    bool callUpdateUID;
    bool callUpdatePWM;
    bool callOverridePWM;
    bool callPinOutput;
    bool callParameterRequest;
    uint8_t modelMatchId;
    uint8_t paramRequestType;
    uint8_t paramRequestIndex;
    uint8_t paramRequestArg;
#if defined(TARGET_RX)
    pwm_val_override_t pwmValueOverride;
    rx_pwm_config_in pwmInput;
#endif
    pin_output_t pinOutputCommand;
};
