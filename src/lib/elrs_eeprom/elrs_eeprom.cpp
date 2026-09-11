#include "elrs_eeprom.h"
#include "targets.h"
#include "logging.h"

#if !defined(TARGET_NATIVE)
#if defined(PLATFORM_STM32)
#if defined(TARGET_USE_EEPROM) && defined(USE_I2C)
#include <Wire.h>
#include <extEEPROM.h>
extEEPROM EEPROM(kbits_2, 1, 1, TARGET_EEPROM_ADDR);
#else
#define STM32_USE_FLASH
#include <utility/stm32_eeprom.h>
#endif
#else
#include <EEPROM.h>
#endif

void
ELRS_EEPROM::Begin()
{
#if defined(PLATFORM_STM32)
#if defined(STM32_USE_FLASH)
    eeprom_buffer_fill();
#else
    EEPROM.begin(extEEPROM::twiClock100kHz);
#endif
#else
    EEPROM.begin(RESERVED_EEPROM_SIZE);
#endif
}

uint8_t
ELRS_EEPROM::ReadByte(const uint32_t address)
{
    if (address >= RESERVED_EEPROM_SIZE)
    {
        // address is out of bounds
        ERRLN("EEPROM address is out of bounds");
        return 0;
    }
#if defined(STM32_USE_FLASH)
    return eeprom_buffered_read_byte(address);
#else
    return EEPROM.read(address);
#endif
}

void
ELRS_EEPROM::WriteByte(const uint32_t address, const uint8_t value)
{
    if (address >= RESERVED_EEPROM_SIZE)
    {
        // address is out of bounds
        ERRLN("EEPROM address is out of bounds");
        return;
    }
#if defined(STM32_USE_FLASH)
    eeprom_buffered_write_byte(address, value);
#elif defined(PLATFORM_STM32)
    EEPROM.update(address, value);
#else
    EEPROM.write(address, value);
#endif
}

void
ELRS_EEPROM::Commit()
{
#if !defined(PLATFORM_STM32)
#if defined(PLATFORM_ESP8266)
    // EEPROM.commit() erases a flash sector, and the flash window is unmapped for the
    // duration. Any interrupt that reaches code living in IROM therefore fetches garbage
    // and dies with Exception(0) IllegalInstruction, abandoning the erase and leaving the
    // config sector invalid -- which zeroes the UID and drops the receiver into binding
    // mode. Seen on every packet-rate change, because TentativeConnection() persists the
    // acquired rate and CheckConfigChangePending() commits it. The ISRs themselves are
    // IRAM, but callees such as LQCALC's accessors are not: GCC silently drops the
    // section attribute for those COMDAT template members, so they link into .text/IROM.
    // Observed faulting in both LQCALC<100>::currentIsSet() and hardware_pin(), i.e. more
    // than one callee is affected, so mask here rather than chase individual functions.
    // A sector erase is tens of milliseconds and every caller has already dropped the
    // link, so blocking interrupts across it is safe.
    noInterrupts();
#endif
    const bool ok = EEPROM.commit();
#if defined(PLATFORM_ESP8266)
    interrupts();
#endif
    if (!ok)
    {
      ERRLN("EEPROM commit failed");
    }
#endif
}

#endif /* !TARGET_NATIVE */
