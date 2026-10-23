#ifdef PLATFORM_STM32
#include "hwTimer.h"

void (*hwTimer::callbackTick)() = nullptr;
void (*hwTimer::callbackTock)() = nullptr;

volatile bool hwTimer::running = false;
volatile bool hwTimer::isTick = false;

volatile uint32_t hwTimer::HWtimerInterval = TimerIntervalUSDefault;
volatile int32_t hwTimer::PhaseShift = 0;
volatile int32_t hwTimer::FreqOffset = 0;

// Internal implementation specific variables
static volatile uint32_t PauseDuration;
static bool alreadyInit = false;

#ifdef FRSKY_R9MM 
static HardwareTimer *MyTim = new HardwareTimer(TIM3);   // Changed this to TIM3 so we can use TIM1 for PWMs  
#elif defined(M0139) && defined(TARGET_RX)
static HardwareTimer *MyTim = new HardwareTimer(TIM2);   // Changed this to TIM2 so we can use TIM1 and TIM3 for PWMs  
#elif defined(TIM1)
static HardwareTimer *MyTim = new HardwareTimer(TIM1);
#else
// FM30_mini (STM32F373xC) no advanced timer but TIM2 is 32-bit general purpose
static HardwareTimer *MyTim = new HardwareTimer(TIM2);
#endif

void hwTimer::init(void (*callbackTick)(), void (*callbackTock)())
{
    if (!alreadyInit)
    {
        hwTimer::callbackTick = callbackTick;
        hwTimer::callbackTock = callbackTock;
        MyTim->attachInterrupt(hwTimer::callback);
        MyTim->setMode(1, TIMER_DISABLED);
#if defined(TARGET_TX)
        // The TX changes its interval while the timer runs (SetRFLinkRate()). In
        // MICROSEC_FORMAT, setOverflow() picks a prescaler to suit each interval, but the
        // prescaler only loads at the next update event while the auto-reload (preload off)
        // applies at once, so the first half-period after a rate change ran with the old
        // prescaler and the new reload: 100Hz to 200Hz gave a 7.5ms period, not 5ms. The
        // receiver locks on to the first sync packets at the new rate, so that slip left it
        // half a slot off the transmitter until it timed out and re-acquired. A fixed 1us
        // tick keeps every period exact (16-bit reload: half-periods up to 65ms).
        MyTim->setPrescaleFactor(MyTim->getTimerClkFreq() / 1000000); // 1us per tick
        MyTim->setOverflow(HWtimerInterval >> 1, TICK_FORMAT);
#else
        MyTim->setOverflow(HWtimerInterval >> 1, MICROSEC_FORMAT); // 22(50Hz) to 3(500Hz) scaler
#endif
        MyTim->setPreloadEnable(false);
        alreadyInit = true;
    }
}

void hwTimer::stop()
{
    running = false;
    MyTim->pause();
    MyTim->setCount(0);
}

/*
 * Schedule a pause of the specified duration, in us -- TX Only
 * Will pause until the TICK interrupt, then the next timer will
 * fire Duration - interval/2 after that
 * 65535us max!
 */
#if false
void hwTimer::pause(uint32_t duration)
{
    PauseDuration = duration;
    while (PauseDuration)
        ;
}
#endif

void hwTimer::resume()
{
#if defined(TARGET_RX)
    isTick = false;
#endif
    running = true;
#if defined(TARGET_TX)
    MyTim->setOverflow(HWtimerInterval >> 1, TICK_FORMAT);
#else
    MyTim->setOverflow((HWtimerInterval >> 1), MICROSEC_FORMAT);
#endif
    MyTim->setCount(0);
    MyTim->resume();
    MyTim->refresh(); // will trigger the interrupt immediately, but will update the prescaler shadow reg
}

void hwTimer::updateInterval(uint32_t newTimerInterval)
{
    // timer should not be running when updateInterval() is called
    HWtimerInterval = newTimerInterval;
}

void hwTimer::phaseShift(int32_t newPhaseShift)
{
    int32_t minVal = -(HWtimerInterval >> 2);
    int32_t maxVal = (HWtimerInterval >> 2);
    PhaseShift = constrain(newPhaseShift, minVal, maxVal);
}

void hwTimer::callback(void)
{
    if (hwTimer::isTick)
    {
#if defined(TARGET_TX)
        // if (PauseDuration)
        // {
        //     MyTim->setOverflow(PauseDuration - (HWtimerInterval >> 1), TICK_FORMAT);
        //     PauseDuration = 0;
        // }
        // else
        // {
        //     MyTim->setOverflow(HWtimerInterval >> 1, TICK_FORMAT);
        // }
        // No tick callback
        MyTim->setOverflow((HWtimerInterval >> 1), TICK_FORMAT);
#else
        MyTim->setOverflow((HWtimerInterval >> 1), MICROSEC_FORMAT);
        uint32_t adjustedInterval = MyTim->getOverflow(MICROSEC_FORMAT) + FreqOffset;
        MyTim->setOverflow(adjustedInterval, MICROSEC_FORMAT);
        hwTimer::callbackTick();
#endif
    }
    else
    {
#if defined(TARGET_TX)
        MyTim->setOverflow((HWtimerInterval >> 1) + PhaseShift, TICK_FORMAT);
        hwTimer::callbackTock();
#else
        MyTim->setOverflow((HWtimerInterval >> 1) + PhaseShift, MICROSEC_FORMAT);
        uint32_t adjustedInterval = MyTim->getOverflow(MICROSEC_FORMAT) + FreqOffset;
        MyTim->setOverflow(adjustedInterval, MICROSEC_FORMAT);
        PhaseShift = 0;
        hwTimer::callbackTock();
#endif
    }
    hwTimer::isTick = !hwTimer::isTick;
}
#endif
