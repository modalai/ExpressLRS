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
    SET_PIN_OUTPUT = 0xF5,
    SET_PWM_DEFAULT = 0xFF
};

typedef struct pin_output_override_t {
    uint8_t command;    // 0xF5
    uint8_t pin_index;  // 0-based physical pin
    uint8_t mode;       // eServoOutputMode value
    uint8_t val_high;
    uint8_t val_low;
} __attribute__((packed)) pin_output_override_t;

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
    bool ShouldCallPinOverride();
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
    pin_output_override_t GetPinOverride(){ return pinOverrideValue; }
#endif
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
    bool callPinOverride;
    bool callParameterRequest;
    uint8_t modelMatchId;
    uint8_t paramRequestType;
    uint8_t paramRequestIndex;
    uint8_t paramRequestArg;
#if defined(TARGET_RX)
    pin_output_override_t pinOverrideValue;
    rx_pwm_config_in pwmInput;
#endif
};
