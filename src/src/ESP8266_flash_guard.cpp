#if defined(PLATFORM_ESP8266)
// Keep the RF interrupts out of SPI flash erase and write.
//
// The SDK leaves interrupts enabled while it erases or writes flash, but the instruction cache
// is off for the whole operation. The radio (GPIO) and RF timer (CCOMPARE0) handlers live in
// IRAM yet call flash-resident code -- hardware_pin(), FHSS, OTA unpacking -- so an edge or a
// tick during a config commit took Exception (0) and rebooted the receiver. It showed up on
// roughly one boot in six, when the power-on counter sector is erased while the link is still
// being acquired. Mask both sources for the duration: anything that arrives meanwhile stays
// pending and is serviced once the cache is back, and the RF timer catches up after the gap
// (see ESP8266_hwTimer.cpp). Hooked in with -Wl,-wrap in python/build_flags.py.
#include <Arduino.h>

extern "C" {
#include "spi_flash.h"
SpiFlashOpResult __real_spi_flash_erase_sector(uint16 sec);
SpiFlashOpResult __real_spi_flash_write(uint32 des_addr, uint32 *src_addr, uint32 size);
}

static constexpr uint32_t RF_INTERRUPTS = (1U << ETS_GPIO_INUM) | (1U << ETS_CCOMPARE0_INUM);

// Returns the sources that were enabled and are now masked, for unmaskRfInterrupts().
static uint32_t maskRfInterrupts()
{
    uint32_t enabled;
    asm volatile("rsr %0, intenable" : "=r"(enabled));
    const uint32_t masked = enabled & RF_INTERRUPTS;
    ets_isr_mask(masked);
    return masked;
}

static void unmaskRfInterrupts(uint32_t masked)
{
    ets_isr_unmask(masked);
}

extern "C" SpiFlashOpResult __wrap_spi_flash_erase_sector(uint16 sec)
{
    const uint32_t masked = maskRfInterrupts();
    const SpiFlashOpResult result = __real_spi_flash_erase_sector(sec);
    unmaskRfInterrupts(masked);
    return result;
}

extern "C" SpiFlashOpResult __wrap_spi_flash_write(uint32 des_addr, uint32 *src_addr, uint32 size)
{
    const uint32_t masked = maskRfInterrupts();
    const SpiFlashOpResult result = __real_spi_flash_write(des_addr, src_addr, size);
    unmaskRfInterrupts(masked);
    return result;
}
#endif
