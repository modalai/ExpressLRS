#include "SerialIO.h"

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

    while (_fifo.size() > _fifo.peek() && (bytesWritten + _fifo.peek()) <= maxBytesToSend)
    {
        uint8_t pktLen = _fifo.peek();

        // Check if there's enough space in the serial output buffer
        // If not, break and try again later
        if (_outputPort->availableForWrite() < pktLen)
        {
            break;
        }

        _fifo.lock();
        uint8_t OutPktLen = _fifo.pop();
        uint8_t OutData[OutPktLen];
        _fifo.popBytes(OutData, OutPktLen);
        _fifo.unlock();
        noInterrupts();
        this->_outputPort->write(OutData, OutPktLen); // write the packet out
        interrupts();
        bytesWritten += OutPktLen;
    }
}
