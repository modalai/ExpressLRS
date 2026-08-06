#include "RXEndpoint.h"

#if !defined(UNIT_TEST)
#include "config.h"
#include "devMSPVTX.h"
#include "devVTXSPI.h"
#include "freqTable.h"
#include "rxtx_intf.h"
#include "logging.h"
#if defined(M0139)
#include "devServoOutput.h"
#endif

extern void reset_into_bootloader();

RXEndpoint::RXEndpoint()
    : RxTxEndpoint(CRSF_ADDRESS_CRSF_RECEIVER)
{
}

#if defined(M0139)
void RXEndpoint::sendBootDeviceInformation(CRSFConnector *connector)
{
    sendDeviceInformationPacketTo(CRSF_ADDRESS_FLIGHT_CONTROLLER, connector);
}
#endif

/**
 * Handle any non-CRSF commands that we receive
 * @param message
 * @return
 */
bool RXEndpoint::handleRaw(const crsf_header_t *message)
{
    if (message->sync_byte == CRSF_ADDRESS_CRSF_RECEIVER && message->frame_size >= 4 && message->type == CRSF_FRAMETYPE_COMMAND)
    {
        uint8_t *payload = (uint8_t *)message + sizeof(crsf_header_t);
        // Non CRSF, dest=b src=l -> reboot to bootloader
        if (payload[0] == 'b' && payload[1] == 'l')
        {
            reset_into_bootloader();
            return true;
        }
        if (payload[0] == 'b' && payload[1] == 'd')
        {
            EnterBindingModeSafely();
            return true;
        }
        if (payload[0] == 'm' && payload[1] == 'm')
        {
            config.SetModelId(payload[2]);
            return true;
        }
#if defined(M0139)
        if (payload[0] == 'u' && payload[1] == 'b')
        {
            EnterUnbindMode();
            return true;
        }
        if (payload[0] == 'i' && payload[1] == 'd' && message->frame_size >= CRSF_FRAME_SIZE(2 + UID_LEN))
        {
            UpdateUID(payload + 2);
            return true;
        }
        if (payload[0] == SET_PWM_CH && message->frame_size >= CRSF_FRAME_SIZE(1 + sizeof(modal_pwm_config_t)))
        {
            modal_pwm_config_t newConfig;
            memcpy(&newConfig, payload + 1, sizeof(newConfig));
            servoApplyConfig(newConfig);
            return true;
        }
#endif
    }
#if defined(M0139)
    if (message->type == CRSF_FRAMETYPE_COMMAND && message->frame_size >= CRSF_EXT_FRAME_SIZE(sizeof(modal_pwm_override_t)))
    {
        const auto *extMessage = (const crsf_ext_header_t *)message;
        if (extMessage->dest_addr == CRSF_ADDRESS_CRSF_RECEIVER &&
            extMessage->orig_addr == CRSF_ADDRESS_FLIGHT_CONTROLLER &&
            extMessage->payload[0] == SET_PWM_VAL)
        {
            modal_pwm_override_t overrideValue;
            memcpy(&overrideValue, extMessage->payload, sizeof(overrideValue));
            servoOverrideChannel(overrideValue);
            return true;
        }
    }
#endif
    return false;
}

void RXEndpoint::handleMessage(const crsf_header_t *message)
{
    const auto extMessage = (crsf_ext_header_t *)message;

    if (handleRxTxMessage(message))
    {
        return;
    }
    else if (message->type == CRSF_FRAMETYPE_COMMAND && extMessage->payload[0] == CRSF_COMMAND_SUBCMD_RX && extMessage->payload[1] == CRSF_COMMAND_SUBCMD_RX_BIND)
    {
        EnterBindingModeSafely();
    }
#if defined(PLATFORM_ESP32)
    else if (message->type == CRSF_FRAMETYPE_MSP_RESP)
    {
        mspVtxProcessPacket((uint8_t *)message);
    }
    else if (OPT_HAS_VTX_SPI && message->type == CRSF_FRAMETYPE_MSP_WRITE && extMessage->payload[2] == MSP_SET_VTX_CONFIG)
    {
        vtxSPIFrequency = getFreqByIdx(extMessage->payload[3]);
        if (extMessage->payload[1] >= 4) // If packet has 4 bytes it also contains power idx and pitmode.
        {
            vtxSPIPowerIdx = extMessage->payload[5];
            vtxSPIPitmode = extMessage->payload[6];
        }
        devicesTriggerEvent(EVENT_VTX_CHANGE);
    }
#endif
    else if (message->type == CRSF_FRAMETYPE_DEVICE_PING ||
             message->type == CRSF_FRAMETYPE_PARAMETER_READ ||
             message->type == CRSF_FRAMETYPE_PARAMETER_WRITE)
    {
        parameterUpdateReq(
            extMessage->orig_addr,
            extMessage->type,
            extMessage->payload[0],  // parameter index
            extMessage->payload + 1, // start of parameter payload
            message->frame_size >= 5 ? message->frame_size - 5 : 0
        );
    }
}
#endif
