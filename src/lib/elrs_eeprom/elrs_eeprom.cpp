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
    if (!EEPROM.commit())
    {
      ERRLN("EEPROM commit failed");
    }
#endif
}

#endif /* !TARGET_NATIVE */
