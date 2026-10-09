#if defined(PLATFORM_ESP8266)
#include "hwTimer.h"

void (*hwTimer::callbackTick)() = nullptr;
void (*hwTimer::callbackTock)() = nullptr;

volatile bool hwTimer::running = false;
volatile bool hwTimer::isTick = false;

volatile uint32_t hwTimer::HWtimerInterval = TimerIntervalUSDefault;
volatile int32_t hwTimer::PhaseShift = 0;
volatile int32_t hwTimer::FreqOffset = 0;

// Internal implementation specific variables
static uint32_t NextTimeout;

#define HWTIMER_TICKS_PER_US 5
#define HWTIMER_PRESCALER (clockCyclesPerMicrosecond() / HWTIMER_TICKS_PER_US)

// Arm CCOMPARE0 for NextTimeout. Each deadline is advanced from the previous one rather than
// from "now" so the phase never drifts, which means an interrupt serviced more than one step
// late -- ESP8266_flash_guard.cpp holds it off for tens of milliseconds while flash is erased
// or written -- leaves the new deadline already behind CCOUNT. CCOMPARE0 then does not match
// again until the 32-bit cycle counter wraps (~27 s at 160 MHz): the timer silently stops, and
// on the RX LostConnection() waited for it until the soft watchdog rebooted the receiver. Skip
// whole periods until the deadline is ahead again, keeping both the phase and tick/tock order.
static inline void ICACHE_RAM_ATTR armNextTimeout(uint32_t period)
{
    constexpr int32_t minLeadCycles = 2 * HWTIMER_TICKS_PER_US * HWTIMER_PRESCALER; // 2 us
    while ((int32_t)(NextTimeout - ESP.getCycleCount()) < minLeadCycles)
    {
        NextTimeout += period;
    }
    timer0_write(NextTimeout);
}

void hwTimer::init(void (*callbackTick)(), void (*callbackTock)())
{
    hwTimer::callbackTick = callbackTick;
    hwTimer::callbackTock = callbackTock;
    timer0_isr_init();
    running = false;
}

void hwTimer::stop()
{
    if (running)
    {
        timer0_detachInterrupt();
        running = false;
    }
}

void ICACHE_RAM_ATTR hwTimer::resume()
{
    if (!running)
    {
        noInterrupts();
        timer0_attachInterrupt(hwTimer::callback);
        // We want the timer to fire tock() ASAP after enabling
        // tock() should always be the first event to maintain consistency
        isTick = false;
#if defined(TARGET_TX)
        NextTimeout = ESP.getCycleCount() + HWtimerInterval;
#else
        // Fire the timer in 20us to get it started close to the correct starting time and phase.
        // The 20us delay is requied to allow the transceiver IRQ enough time to return and finish IsrCallback().
        // If it does not there is a chance that the IRQ will not be cleared and the radio will hang. 
        NextTimeout = ESP.getCycleCount() + (20 * HWTIMER_TICKS_PER_US * HWTIMER_PRESCALER);
#endif
        timer0_write(NextTimeout);
        interrupts();

        running = true;
    }
}

void hwTimer::updateInterval(uint32_t newTimerInterval)
{
    // timer should not be running when updateInterval() is called
    HWtimerInterval = newTimerInterval * (HWTIMER_TICKS_PER_US * HWTIMER_PRESCALER);
}

void ICACHE_RAM_ATTR hwTimer::phaseShift(int32_t newPhaseShift)
{
    int32_t minVal = -(HWtimerInterval >> 2);
    int32_t maxVal = (HWtimerInterval >> 2);

    // phase shift is in microseconds
    PhaseShift = constrain(newPhaseShift, minVal, maxVal) * (HWTIMER_TICKS_PER_US * HWTIMER_PRESCALER);
}

void ICACHE_RAM_ATTR hwTimer::callback()
{
    if (running)
    {
#if defined(TARGET_TX)
        NextTimeout += HWtimerInterval;
        armNextTimeout(HWtimerInterval);
        callbackTock();
#else
        NextTimeout += (HWtimerInterval >> 1) + (FreqOffset * HWTIMER_PRESCALER);
        if (hwTimer::isTick)
        {
            armNextTimeout(HWtimerInterval);
            callbackTick();
        }
        else
        {
            NextTimeout += PhaseShift;
            armNextTimeout(HWtimerInterval);
            PhaseShift = 0;
            callbackTock();
        }
        hwTimer::isTick = !hwTimer::isTick;
#endif
    }
}

#endif
