#include "SerialIBus.h"

#include "config.h"
#include "crsf_protocol.h"
#include "device.h"

#if defined(TARGET_RX)

static constexpr uint8_t IBUS_CHANNEL_COUNT = 14;
static constexpr uint16_t IBUS_HEADER = 0x4020;
static constexpr uint16_t IBUS_CHECKSUM_START = 0xFFFF;
static constexpr uint16_t IBUS_CHANNEL_MIN = 1000;
static constexpr uint16_t IBUS_CHANNEL_MAX = 2000;
static constexpr uint8_t IBUS_PACKET_INTERVAL_MS = 7;
static constexpr uint8_t UNCONNECTED_CALLBACK_INTERVAL_MS = 10;

struct __attribute__((packed)) ibus_channels_packet_t
{
    uint16_t header;
    uint16_t channels[IBUS_CHANNEL_COUNT];
    uint16_t checksum;
};

uint32_t SerialIBus::sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData)
{
    static bool sendPackets;
    const bool effectivelyFailsafed = failsafe || !connectionHasModelMatch || !teamraceHasModelMatch;
    if ((effectivelyFailsafed && config.GetFailsafeMode() == FAILSAFE_NO_PULSES) ||
        (!sendPackets && connectionState != connected))
    {
        return UNCONNECTED_CALLBACK_INTERVAL_MS;
    }
    sendPackets = true;

    if ((!frameAvailable && !frameMissed && !effectivelyFailsafed) ||
        _outputPort->availableForWrite() < static_cast<int>(sizeof(ibus_channels_packet_t)))
    {
        return DURATION_IMMEDIATELY;
    }

    ibus_channels_packet_t packet{};
    packet.header = IBUS_HEADER;
    for (uint8_t channel = 0; channel < IBUS_CHANNEL_COUNT; ++channel)
    {
        const uint32_t value = constrain(channelData[channel], CRSF_CHANNEL_VALUE_STD_MIN, CRSF_CHANNEL_VALUE_STD_MAX);
        packet.channels[channel] = fmap(value,
                                        CRSF_CHANNEL_VALUE_STD_MIN,
                                        CRSF_CHANNEL_VALUE_STD_MAX,
                                        IBUS_CHANNEL_MIN,
                                        IBUS_CHANNEL_MAX);
    }

    packet.checksum = IBUS_CHECKSUM_START;
    const auto *bytes = reinterpret_cast<const uint8_t *>(&packet);
    for (size_t index = 0; index < offsetof(ibus_channels_packet_t, checksum); ++index)
    {
        packet.checksum -= bytes[index];
    }

    _outputPort->write((const uint8_t *)&packet, sizeof(packet));
    return IBUS_PACKET_INTERVAL_MS;
}

#endif
