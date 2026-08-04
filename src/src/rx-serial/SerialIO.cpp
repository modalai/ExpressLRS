#include "SerialIO.h"

#if defined(PLATFORM_STM32)
#include "crsf_protocol.h"
static_assert(SERIAL_TX_BUFFER_SIZE > CRSF_MAX_PACKET_LEN,
              "STM32 TX buffer must hold one CRSF frame");
#endif

void SerialIO::setFailsafe(bool failsafe)
{
    this->failsafe = failsafe;
}

void SerialIO::processSerialInput()
{
    auto maxBytes = getMaxSerialReadSize();
    uint8_t buffer[maxBytes];
    auto size = min(_inputPort->available(), maxBytes);
    _inputPort->readBytes(buffer, size);
    processBytes(buffer, size);
}

void SerialIO::sendQueuedData(uint32_t maxBytesToSend)
{
    uint32_t bytesWritten = 0;

    while (_fifo.size() > _fifo.peek() && (bytesWritten + _fifo.peek()) < maxBytesToSend)
    {
#if defined(PLATFORM_STM32)
        // HardwareSerial needs TX interrupts when its ring lacks capacity.
        // Keep this write atomic only after the complete frame fits.
        noInterrupts();
        if (_outputPort->availableForWrite() < _fifo.peek())
        {
            interrupts();
            break;
        }
#endif
        _fifo.lock();
        uint8_t OutPktLen = _fifo.pop();
        uint8_t OutData[OutPktLen];
        _fifo.popBytes(OutData, OutPktLen);
        _fifo.unlock();
#if !defined(PLATFORM_STM32)
        noInterrupts();
#endif
        this->_outputPort->write(OutData, OutPktLen); // write the packet out
        interrupts();
        bytesWritten += OutPktLen;
    }
}
