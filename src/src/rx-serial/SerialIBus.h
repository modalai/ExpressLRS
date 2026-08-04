#pragma once

#include "SerialIO.h"

class SerialIBus final : public SerialIO {
public:
    explicit SerialIBus(Stream &out, Stream &in) : SerialIO(&out, &in) {}
    ~SerialIBus() override = default;

    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) override;

private:
    void processBytes(uint8_t *bytes, uint16_t size) override {}
};
