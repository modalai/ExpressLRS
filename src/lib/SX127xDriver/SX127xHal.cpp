#ifndef UNIT_TEST

#include "SX127xHal.h"
#include "SX127xRegs.h"
#include "logging.h"
#if defined(M0139)
#include <SPI.h>
#else
#include <SPIEx.h>
#endif

SX127xHal *SX127xHal::instance = NULL;

#if defined(M0139)
#include "pinmap.h"
#include "PeripheralPins.h"

static SPIClass SPI_1;
static SPIClass SPI_2;

// Direct-register SPI transfer path for the M0139 family.
//
// A 2-byte register access through SPIClass::transfer() + digitalWrite() costs
// ~9us on the STM32F103 (measured with the DWT cycle counter) against ~4us via
// direct register access; inside the radio ISR, where the UART ISR preempts it,
// each transaction cost ~20us. The DIO0 ISR does ~14 transactions per received
// telemetry packet and runs at a higher NVIC priority than the RF timer ISR, so
// every microsecond spent here delays the transmit instant. SPIClass still owns
// peripheral configuration (begin/clock/mode); only the byte transfers and the
// chip-select are done here.
struct FastSpi
{
    SPI_TypeDef *spi;
    GPIO_TypeDef *nssPort;
    uint32_t nssPin;
};
static FastSpi fastSpi[2];

static void fastSpiInit(FastSpi &f, uint8_t sckPin, uint8_t nssPin)
{
    const PinName nss = digitalPinToPinName(nssPin);
    f.nssPort = get_GPIO_Port(STM_PORT(nss));
    f.nssPin = STM_LL_GPIO_PIN(nss);
    f.spi = (SPI_TypeDef *)pinmap_peripheral(digitalPinToPinName(sckPin), PinMap_SPI_SCLK);
}

// Full-duplex transfer of buf[0..n) with NSS asserted for the whole frame.
// buf is overwritten with the received bytes.
//
// The frame is atomic. Both the radio ISRs and the main loop drive these buses
// (on the RX nearly all radio access is in ISR context, and the DIO0 EXTI can
// preempt the RF timer ISR mid-frame). A preempted frame is unrecoverable: the
// inner transfer consumes the byte the outer one is waiting for, so the outer
// `while (!RXNE)` never completes and the firmware hangs. A frame is at most
// PayloadLength+1 bytes, about 12us at 9MHz, so this is a bounded and much
// smaller interrupt delay than the SPIClass path it replaced.
static inline void ICACHE_RAM_ATTR fastSpiTransfer(const FastSpi &f, uint8_t *buf, uint8_t n)
{
    SPI_TypeDef *const spi = f.spi;
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();

    LL_GPIO_ResetOutputPin(f.nssPort, f.nssPin);
    // discard anything left in the receive register
    while (spi->SR & SPI_SR_RXNE)
    {
        (void)spi->DR;
    }
    for (uint8_t i = 0; i < n; i++)
    {
        while (!(spi->SR & SPI_SR_TXE)) {}
        spi->DR = buf[i];
        while (!(spi->SR & SPI_SR_RXNE)) {}
        buf[i] = (uint8_t)spi->DR;
    }
    LL_GPIO_SetOutputPin(f.nssPort, f.nssPin);

    __set_PRIMASK(primask);
}
#endif

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
#if defined(M0139)
    SPI_1.end();
    SPI_2.end();
#else
    SPIEx.end();
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
    SPIEx.begin(GPIO_PIN_SCK, GPIO_PIN_MISO, GPIO_PIN_MOSI, GPIO_PIN_NSS); // sck, miso, mosi, ss (ss can be any GPIO)
    gpio_pullup_en((gpio_num_t)GPIO_PIN_MISO);
    SPIEx.setFrequency(10000000);
    SPIEx.setHwCs(true);
    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        pinMode(GPIO_PIN_NSS_2, OUTPUT);
        digitalWrite(GPIO_PIN_NSS_2, HIGH);
        spiAttachSS(SPIEx.bus(), 1, GPIO_PIN_NSS_2);
    }
    spiEnableSSPins(SPIEx.bus(), SX12XX_Radio_All);
#elif defined(PLATFORM_ESP8266)
    DBGLN("PLATFORM_ESP8266");
    SPIEx.begin();
    SPIEx.setHwCs(true);
    SPIEx.setBitOrder(MSBFIRST);
    SPIEx.setDataMode(SPI_MODE0);
    SPIEx.setFrequency(10000000);
#elif defined(M0139)
    SPI_1.setMOSI(GPIO_PIN_MOSI);
    SPI_1.setMISO(GPIO_PIN_MISO);
    SPI_1.setSCLK(GPIO_PIN_SCK);
    SPI_1.setBitOrder(MSBFIRST);
    SPI_1.setDataMode(SPI_MODE0);
    SPI_1.begin();
    SPI_1.setClockDivider(SPI_CLOCK_DIV4);

    SPI_2.setMOSI(GPIO_PIN_MOSI_2);
    SPI_2.setMISO(GPIO_PIN_MISO_2);
    SPI_2.setSCLK(GPIO_PIN_SCK_2);
    SPI_2.setBitOrder(MSBFIRST);
    SPI_2.setDataMode(SPI_MODE0);
    SPI_2.begin();
    SPI_2.setClockDivider(SPI_CLOCK_DIV4);

    fastSpiInit(fastSpi[0], GPIO_PIN_SCK, GPIO_PIN_NSS);
    fastSpiInit(fastSpi[1], GPIO_PIN_SCK_2, GPIO_PIN_NSS_2);

#endif

    attachInterrupt(digitalPinToInterrupt(GPIO_PIN_DIO0), this->dioISR_1, RISING);
    if (GPIO_PIN_DIO0_2 != UNDEF_PIN)
    {
        attachInterrupt(digitalPinToInterrupt(GPIO_PIN_DIO0_2), this->dioISR_2, RISING);
    }
}

void SX127xHal::reset(void)
{
    DBGLN("SX127x Reset");

    if (GPIO_PIN_RST != UNDEF_PIN)
    {
        pinMode(GPIO_PIN_RST, OUTPUT);
        digitalWrite(GPIO_PIN_RST, LOW);
        if (GPIO_PIN_RST_2 != UNDEF_PIN)
        {
            pinMode(GPIO_PIN_RST_2, OUTPUT);
            digitalWrite(GPIO_PIN_RST_2, LOW);
        }
        delay(50); // Safety buffer. Busy takes longer to go low than the 1ms timeout in WaitOnBusy().
        pinMode(GPIO_PIN_RST, INPUT); // leave floating
        if (GPIO_PIN_RST_2 != UNDEF_PIN)
        {
            pinMode(GPIO_PIN_RST_2, INPUT);
        }
    }

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
    WORD_ALIGNED_ATTR uint8_t buf[WORD_PADDED(numBytes + 1)];
    buf[0] = reg | SPI_READ;

#if defined(M0139)
    if (radioNumber & SX12XX_Radio_1)
    {
        fastSpiTransfer(fastSpi[0], buf, numBytes + 1);
    }
    else if (radioNumber & SX12XX_Radio_2)
    {
        fastSpiTransfer(fastSpi[1], buf, numBytes + 1);
    }
#else
    SPIEx.read(radioNumber, buf, numBytes + 1);
#endif

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
    WORD_ALIGNED_ATTR uint8_t buf[WORD_PADDED(numBytes + 1)];
    buf[0] = reg | SPI_WRITE;
    memcpy(buf + 1, data, numBytes);

#if defined(M0139)
    WORD_ALIGNED_ATTR uint8_t buf2[WORD_PADDED(numBytes + 1)];
    if (radioNumber & SX12XX_Radio_2)
    {
        memcpy(buf2, buf, numBytes + 1);
    }
    if (radioNumber & SX12XX_Radio_1)
    {
        fastSpiTransfer(fastSpi[0], buf, numBytes + 1);
    }
    if (radioNumber & SX12XX_Radio_2)
    {
        fastSpiTransfer(fastSpi[1], buf2, numBytes + 1);
    }
#else
    SPIEx.write(radioNumber, buf, numBytes + 1);
#endif
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
