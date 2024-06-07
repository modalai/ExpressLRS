#ifndef UNIT_TEST

#include "SX127xHal.h"
#include "SX127xRegs.h"
#include "logging.h"
#include <SPI.h>

#ifdef M0139
#ifdef DUAL_RADIO
extern SPIClass SPI_2;
#endif
#endif
SX127xHal *SX127xHal::instance = NULL;

SX127xHal::SX127xHal()
{
    instance = this;
}

void SX127xHal::end()
{
    detachInterrupt(GPIO_PIN_DIO0);
    if (GPIO_PIN_DIO0_2 != UNDEF_PIN)
    {
        detachInterrupt(GPIO_PIN_DIO0_2);
    }
    SPI.end();
#ifdef M0139
#ifdef DUAL_RADIO
    SPI_2.end();
#endif
#endif
    IsrCallback_1 = nullptr; // remove callbacks
    IsrCallback_2 = nullptr; // remove callbacks
}

void SX127xHal::init()
{
    DBGLN("Hal Init");

    pinMode(GPIO_PIN_DIO0, INPUT);
    if (GPIO_PIN_DIO0_2 != UNDEF_PIN)
    {
        pinMode(GPIO_PIN_DIO0_2, INPUT);
    }

    pinMode(GPIO_PIN_NSS, OUTPUT);
    digitalWrite(GPIO_PIN_NSS, HIGH);
    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        pinMode(GPIO_PIN_NSS_2, OUTPUT);
        digitalWrite(GPIO_PIN_NSS_2, HIGH);
    }

#ifdef PLATFORM_ESP32
    SPI.begin(GPIO_PIN_SCK, GPIO_PIN_MISO, GPIO_PIN_MOSI, GPIO_PIN_NSS); // sck, miso, mosi, ss (ss can be any GPIO)
    gpio_pullup_en((gpio_num_t)GPIO_PIN_MISO);
    SPI.setFrequency(10000000);
    SPI.setHwCs(true);
    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        spiAttachSS(SPI.bus(), 1, GPIO_PIN_NSS_2);
    }
    spiEnableSSPins(SPI.bus(), 0xFF);
#elif defined(PLATFORM_ESP8266)
    DBGLN("PLATFORM_ESP8266");
    SPI.begin();
    SPI.setBitOrder(MSBFIRST);
    SPI.setDataMode(SPI_MODE0);
    SPI.setFrequency(10000000);
#elif defined(PLATFORM_STM32)
    DBGLN("Config SPI");
    SPI.setMOSI(GPIO_PIN_MOSI);
    SPI.setMISO(GPIO_PIN_MISO);
    SPI.setSCLK(GPIO_PIN_SCK);
    SPI.setBitOrder(MSBFIRST);
    SPI.setDataMode(SPI_MODE0);
    SPI.begin();
    SPI.setClockDivider(SPI_CLOCK_DIV4); // 72 / 8 = 9 MHz
#ifdef M0139
#ifdef DUAL_RADIO
    DBGLN("Config SPI_2");
    SPI_2.setMOSI(GPIO_PIN_MOSI_2);
    SPI_2.setMISO(GPIO_PIN_MISO_2);
    SPI_2.setSCLK(GPIO_PIN_SCK_2);
    SPI_2.setBitOrder(MSBFIRST);
    SPI_2.setDataMode(SPI_MODE0);
    SPI_2.begin();
    SPI_2.setClockDivider(SPI_CLOCK_DIV4); // 72 / 8 = 9 MHz
#endif //DUAL_RADIO
#endif //M0139
#endif

    attachInterrupt(digitalPinToInterrupt(GPIO_PIN_DIO0), this->dioISR_1, RISING);
    if (GPIO_PIN_DIO0_2 != UNDEF_PIN)
    {
        attachInterrupt(digitalPinToInterrupt(GPIO_PIN_DIO0_2), this->dioISR_2, RISING);
    }
}

void ICACHE_RAM_ATTR SX127xHal::setNss(uint8_t radioNumber, bool state)
{
#if defined(PLATFORM_ESP32)
    spiDisableSSPins(SPI.bus(), ~radioNumber);
    spiEnableSSPins(SPI.bus(), radioNumber);
#else
    if (radioNumber & SX12XX_Radio_1){
        // DBGLN("SETTING RADIO 1 NSS: %u", state);
        digitalWrite(GPIO_PIN_NSS, state);
    }
    if (GPIO_PIN_NSS_2 != UNDEF_PIN && radioNumber & SX12XX_Radio_2){
        // DBGLN("SETTING RADIO 2 NSS: %u", state);
        digitalWrite(GPIO_PIN_NSS_2, state);
    }
#endif
}

void SX127xHal::reset(void)
{
    DBGLN("SX127x Reset");

    if (GPIO_PIN_RST != UNDEF_PIN)
    {
        pinMode(GPIO_PIN_RST, OUTPUT);
        delay(100);
        digitalWrite(GPIO_PIN_RST, LOW);
        delay(100);
        pinMode(GPIO_PIN_RST, INPUT); // leave floating
    }

#ifdef M0139
    if (GPIO_PIN_RST_2 != UNDEF_PIN)
    {
        pinMode(GPIO_PIN_RST_2, OUTPUT);
        delay(100);
        digitalWrite(GPIO_PIN_RST_2, LOW);
        delay(100);
        pinMode(GPIO_PIN_RST_2, INPUT); // leave floating
    }
#endif 

    DBGLN("SX127x Ready!");
}

uint8_t ICACHE_RAM_ATTR SX127xHal::readRegisterBits(uint8_t reg, uint8_t mask, SX12XX_Radio_Number_t radioNumber)
{
    uint8_t rawValue = readRegister(reg, radioNumber);
    uint8_t maskedValue = rawValue & mask;
    return (maskedValue);
}

uint8_t ICACHE_RAM_ATTR SX127xHal::readRegister(uint8_t reg, SX12XX_Radio_Number_t radioNumber)
{
    uint8_t data;
    readRegister(reg, &data, 1, radioNumber);
    return data;
}

void ICACHE_RAM_ATTR SX127xHal::readRegister(uint8_t reg, uint8_t *data, uint8_t numBytes, SX12XX_Radio_Number_t radioNumber)
{
    WORD_ALIGNED_ATTR uint8_t buf[numBytes + 1];
    buf[0] = reg | SPI_READ;

    setNss(radioNumber, LOW);
#ifdef M0139
    if (radioNumber == SX12XX_Radio_1){
        // DBGLN("Reading Radio 1 SPI");
        SPI.transfer(buf, numBytes + 1);
    }
#ifdef DUAL_RADIO
    else if (radioNumber == SX12XX_Radio_2){
        // DBGLN("Reading Radio 2 SPI");
        SPI_2.transfer(buf, numBytes + 1);
    } 
    else{
        // DBGLN("Reading BOTH RADIOS");
        // DBGLN("Reading Radio 1 SPI");
        SPI.transfer(buf, numBytes + 1);

        // DBGLN("Reading Radio 2 SPI");
        SPI_2.transfer(buf, numBytes + 1);
    }
#endif
#else
    SPI.transfer(buf, numBytes + 1);
#endif
    setNss(radioNumber, HIGH);

    memcpy(data, buf + 1, numBytes);
}

void ICACHE_RAM_ATTR SX127xHal::writeRegisterBits(uint8_t reg, uint8_t value, uint8_t mask, SX12XX_Radio_Number_t radioNumber)
{
    if (radioNumber & SX12XX_Radio_1)
    {
        uint8_t currentValue = readRegister(reg, SX12XX_Radio_1);
        uint8_t newValue = (currentValue & ~mask) | (value & mask);
        writeRegister(reg, newValue, SX12XX_Radio_1);
    }
    
    if (GPIO_PIN_NSS_2 != UNDEF_PIN && radioNumber & SX12XX_Radio_2)
    {
        uint8_t currentValue = readRegister(reg, SX12XX_Radio_2);
        uint8_t newValue = (currentValue & ~mask) | (value & mask);
        writeRegister(reg, newValue, SX12XX_Radio_2);
    }
}

void ICACHE_RAM_ATTR SX127xHal::writeRegister(uint8_t reg, uint8_t data, SX12XX_Radio_Number_t radioNumber)
{
    writeRegister(reg, &data, 1, radioNumber);
}

void ICACHE_RAM_ATTR SX127xHal::writeRegister(uint8_t reg, uint8_t *data, uint8_t numBytes, SX12XX_Radio_Number_t radioNumber)
{
    WORD_ALIGNED_ATTR uint8_t buf[numBytes + 1];
    buf[0] = reg | SPI_WRITE;
    memcpy(buf + 1, data, numBytes);

    setNss(radioNumber, LOW);
#ifdef M0139
    if (radioNumber == SX12XX_Radio_1){
        // DBGLN("Writing Radio 1 SPI");
        SPI.transfer(buf, numBytes + 1);
    } 
#ifdef DUAL_RADIO
    else if (radioNumber == SX12XX_Radio_2){
        // DBGLN("Writing Radio 2 SPI");
        SPI_2.transfer(buf, numBytes + 1);
    } 
    else{
        // DBGLN("Writing BOTH RADIOS");
        // DBGLN("Writing Radio 1 SPI");
        SPI.transfer(buf, numBytes + 1);

        // DBGLN("Writing Radio 2 SPI");
        SPI_2.transfer(buf, numBytes + 1);
    }
#endif
#else
    SPI.transfer(buf, numBytes + 1);
#endif
    setNss(radioNumber, HIGH);
}

void ICACHE_RAM_ATTR SX127xHal::dioISR_1()
{
    if (instance->IsrCallback_1)
        instance->IsrCallback_1();
}

void ICACHE_RAM_ATTR SX127xHal::dioISR_2()
{
    if (instance->IsrCallback_2)
        instance->IsrCallback_2();
}

#endif // UNIT_TEST
