#if defined(TARGET_RX)

#include "devServoOutput.h"
#include "OTA.h"
#include "PWM.h"
#include "config.h"
#include "crsf_protocol.h"
#include "logging.h"
#include "rxtx_intf.h"

#if defined(PLATFORM_ESP32)
#include <driver/periph_ctrl.h>
#endif

static int8_t servoPins[PWM_MAX_CHANNELS];
static pwm_channel_t pwmChannels[PWM_MAX_CHANNELS];
static uint16_t pwmChannelValues[PWM_MAX_CHANNELS];
static bool initialized = false;

#if defined(M0139)
static bool pwmIsArmed = false;
static uint32_t lastOverrideUpdate;
static uint32_t pwmConfigHash;
#endif

#if defined(PLATFORM_ESP32)
static DShotRMT *dshotInstances[PWM_MAX_CHANNELS] = {nullptr};
const uint8_t RMT_MAX_CHANNELS = 8;
#endif

// true when the RX has a new channels packet
static volatile bool newChannelsAvailable;
static uint32_t lastUpdate;
// Absolute max failsafe time if no update is received, regardless of LQ
static constexpr uint32_t FAILSAFE_ABS_TIMEOUT_MS = 1000U;
static constexpr uint32_t DISCONNECTED_UPDATE_MS = 10;

typedef void (*servoWrite_fn)(uint8_t ch, uint16_t us);

#if defined(M0139)
static uint16_t interpolate(uint16_t x, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    if (x1 == x2)
    {
        return y1;
    }

    const int32_t outputDelta = (int32_t)y2 - y1;
    const int32_t value = y1 + ((int32_t)(x - x1) * outputDelta) / (x2 - x1);
    return constrain(value, 0, UINT16_MAX);
}

static uint16_t mapChannelValue(const rx_config_pwm_t *cfg, uint16_t input)
{
    if (cfg->val.mapMode == mapModeOff)
    {
        return CRSF_to_US(input);
    }

    const uint16_t in1 = cfg->val.mapInVal1 << 1;
    const uint16_t in2 = cfg->val.mapInVal2 << 1;
    const uint16_t in3 = cfg->val.mapInVal3 << 1;
    const uint16_t out1 = cfg->val.mapOutVal1 << 1;
    const uint16_t out2 = cfg->val.mapOutVal2 << 1;
    const uint16_t out3 = cfg->val.mapOutVal3 << 1;

    if (cfg->val.mapMode == mapModeStepDown)
    {
        if (input < in2)
        {
            return out1;
        }
        return input < in3 ? out2 : out3;
    }

    if (input <= in1)
    {
        return out1;
    }
    if (input >= in3)
    {
        return out3;
    }
    if (input <= in2)
    {
        return interpolate(input, in1, out1, in2, out2);
    }
    return interpolate(input, in2, out2, in3, out3);
}
#endif

void ICACHE_RAM_ATTR servoNewChannelsAvailable()
{
    newChannelsAvailable = true;
}

uint16_t servoOutputModeToFrequency(eServoOutputMode mode)
{
    switch (mode)
    {
    case som50Hz:
        return 50U;
    case som60Hz:
        return 60U;
    case som100Hz:
        return 100U;
    case som160Hz:
        return 160U;
    case som333Hz:
        return 333U;
    case som400Hz:
        return 400U;
    case som10KHzDuty:
        return 10000U;
    default:
        return 0;
    }
}

static void servoWriteDshot(eServoOutputMode chMode, uint8_t ch, uint16_t us)
{
#if defined(PLATFORM_ESP32)
    // DBGLN("Writing DShot output: us: %u, ch: %d", us, ch);
    if (dshotInstances[ch] == nullptr)
        return;

    // check if we actually want a pulse (for no-pulse failsafe)
    if (us > 0)
    {
        uint16_t dshotVal;
        us = constrain(us, 1000, 2000);
        if (chMode == somDShot)
        {
            if (us == 1000) { // stopped
                dshotVal = DSHOT_CMD_MOTOR_STOP;
            }
            else {
                dshotVal = fmap(us, 1001, 2000, DSHOT_THROTTLE_MIN, DSHOT_THROTTLE_MAX); // Convert PWM signal in us to DShot value
            }
        }
        else // somDShot3D
        {
            if (us == 1500) { // stopped
                dshotVal = DSHOT_CMD_MOTOR_STOP;
            }
            else if (us > 1500) { // forward
                dshotVal = fmap(us, 1501, 2000, 1048, 2047);
            }
            else { // reverse
                dshotVal = fmap(us, 1499, 1000, 48, 1047);
            }
        }
        dshotInstances[ch]->send_dshot_value(dshotVal);
    }
    else
    {
        // getting an actual zero microsecond command means the failsafe mode is no-pulse
        dshotInstances[ch]->set_looping(false);
    }
#endif /* PLATFORM_ESP32 */
}

static void servoWrite(uint8_t ch, uint16_t us)
{
    const rx_config_pwm_t *chConfig = config.GetPwmChannel(ch);
    const eServoOutputMode chMode = (eServoOutputMode)chConfig->val.mode;
    if (chMode == somDShot || chMode == somDShot3D)
    {
        servoWriteDshot(chMode, ch, us);
    }
    else if (servoPins[ch] != UNDEF_PIN && pwmChannelValues[ch] != us)
    {
        pwmChannelValues[ch] = us;
        if (chMode == somOnOff)
        {
            digitalWrite(servoPins[ch], us > 1500);
        }
        else if (chMode == som10KHzDuty)
        {
            PWM.setDuty(pwmChannels[ch], constrain(us, 1000, 2000) - 1000);
        }
        else
        {
#if defined(M0139)
            us /= chConfig->val.narrow + 1;
#endif
            PWM.setMicroseconds(pwmChannels[ch], us);
        }
    }
}

#if defined(M0139)
uint16_t servoGetLastOutputUs(uint8_t ch)
{
    return ch < GPIO_PIN_PWM_OUTPUTS_COUNT ? pwmChannelValues[ch] : UINT16_MAX;
}
#endif

static void servoFailsafeChannel(uint8_t ch)
{
    const rx_config_pwm_t *chConfig = config.GetPwmChannel(ch);
    if (chConfig->val.failsafeMode == PWMFAILSAFE_SET_POSITION)
    {
        servoWrite(ch, chConfig->val.failsafe + US_CHANNEL_VALUE_MIN);
    }
    else if (chConfig->val.failsafeMode == PWMFAILSAFE_NO_PULSES)
    {
        servoWrite(ch, 0);
    }
}

static void servosFailsafe()
{
    for (int ch = 0 ; ch < GPIO_PIN_PWM_OUTPUTS_COUNT ; ++ch)
    {
        servoFailsafeChannel(ch);
    }
}

static void servosEnterFailsafe()
{
    newChannelsAvailable = false;
    lastUpdate = 0;

    // Outputs are only allocated after the RX has reached connected once.
    if (initialized)
    {
        servosFailsafe();
    }
}

static void servoCalcAllChannels(servoWrite_fn write)
{
    for (int ch = 0 ; ch < GPIO_PIN_PWM_OUTPUTS_COUNT ; ++ch)
    {
        const rx_config_pwm_t *chConfig = config.GetPwmChannel(ch);
#if defined(M0139)
        if (chConfig->val.requiresArm && !pwmIsArmed)
        {
            servoFailsafeChannel(ch);
            continue;
        }
#endif
        const unsigned crsfVal = ChannelData[chConfig->val.inputChannel];
        // crsfVal might be unset if this is a switch channel, and it has not been
        // received yet. Delay initializing the servo until the channel is valid
        if (crsfVal == CRSF_CHANNEL_VALUE_UNSET)
        {
            continue;
        }

        uint16_t us;
#if defined(M0139)
        us = mapChannelValue(chConfig, crsfVal);
#else
        if (chConfig->val.stretched)
        {
            if (OtaIsFullRes)
                us = fmap(crsfVal, CRSF_CHANNEL_VALUE_EXT_MIN, CRSF_CHANNEL_VALUE_EXT_MAX, US_CHANNEL_VALUE_MIN, US_CHANNEL_VALUE_MAX);
            else
                us = fmap(crsfVal, CRSF_CHANNEL_VALUE_STD_MIN, CRSF_CHANNEL_VALUE_STD_MAX, US_CHANNEL_VALUE_MIN, US_CHANNEL_VALUE_MAX);
        }
        else
        {
            us = CRSF_to_US(crsfVal);
        }
#endif
        // Flip the output around the mid-value if inverted
        // (1500 - usOutput) + 1500
        if (chConfig->val.inverted)
        {
            us = (2 * US_CHANNEL_VALUE_CENTER) - us;
        }
        write(ch, us);
    } /* for each servo */
}

static void servoUsToFailsafeConfig(uint8_t ch, uint16_t us)
{
    rx_config_pwm_t newPwmCh;
    newPwmCh = *config.GetPwmChannel(ch);
    newPwmCh.val.failsafe = constrain(us, US_CHANNEL_VALUE_MIN, US_CHANNEL_VALUE_MAX) - US_CHANNEL_VALUE_MIN;
    //DBGLN("FSCH(%u) us=%u", ch, us);
    config.SetPwmChannelRaw(ch, newPwmCh.raw);
}

void servoCurrentToFailsafeConfig()
{
    servoCalcAllChannels(&servoUsToFailsafeConfig);
}

static void servosUpdate(unsigned long now)
{
    PWM.feedWatchdog();
#if defined(M0139)
    const bool overrideActive = lastOverrideUpdate && now - lastOverrideUpdate < FAILSAFE_ABS_TIMEOUT_MS;
#else
    constexpr bool overrideActive = false;
#endif
    if (!overrideActive && (connectionState != connected || !connectionHasModelMatch || !teamraceHasModelMatch))
    {
        servosEnterFailsafe();
        return;
    }

    if (newChannelsAvailable)
    {
        newChannelsAvailable = false;
        lastUpdate = now;
        servoCalcAllChannels(&servoWrite);
    }     /* if newChannelsAvailable */

    // LQ goes to 0 (100 packets missed in a row)
    // OR last update older than FAILSAFE_ABS_TIMEOUT_MS
    // go to failsafe
    else if (lastUpdate && ((!overrideActive && getLq() == 0) || (now - lastUpdate > FAILSAFE_ABS_TIMEOUT_MS)))
    {
        servosFailsafe();
        lastUpdate = 0;
#if defined(M0139)
        lastOverrideUpdate = 0;
#endif
    }
}

static void releaseOutputs()
{
    for (int ch = 0; ch < GPIO_PIN_PWM_OUTPUTS_COUNT; ++ch)
    {
        if (pwmChannels[ch] != -1)
        {
            PWM.release(pwmChannels[ch]);
            pwmChannels[ch] = -1;
        }
#if defined(PLATFORM_ESP32)
        if (dshotInstances[ch] != nullptr)
        {
            delete dshotInstances[ch];
            dshotInstances[ch] = nullptr;
        }
#endif
        servoPins[ch] = UNDEF_PIN;
    }
    initialized = false;
}

static void configureOutputPins()
{
#if defined(PLATFORM_ESP32)
    uint8_t rmtCH = 0;
#endif
    for (int ch = 0; ch < GPIO_PIN_PWM_OUTPUTS_COUNT; ++ch)
    {
        pwmChannelValues[ch] = UINT16_MAX;
        pwmChannels[ch] = -1;
        int8_t pin = GPIO_PIN_PWM_OUTPUTS[ch];
#if defined(DEBUG_LOG) || defined(DEBUG_RCVR_LINKSTATS)
        // Disconnect the debug UART pins if DEBUG_LOG
        if (pin == U0RXD_GPIO_NUM || pin == U0TXD_GPIO_NUM)
        {
            pin = UNDEF_PIN;
        }
#endif
        // Mark servo pins that are being used for serial (or other purposes) as disconnected
        auto mode = (eServoOutputMode)config.GetPwmChannel(ch)->val.mode;
        if (mode >= somSerial)
        {
            pin = UNDEF_PIN;
        }
#if defined(PLATFORM_ESP32)
        else if (mode == somDShot || mode == somDShot3D)
        {
            if (rmtCH < RMT_MAX_CHANNELS)
            {
                auto gpio = (gpio_num_t)pin;
                auto rmtChannel = (rmt_channel_t)rmtCH;
                DBGLN("Initializing DShot: gpio: %u, ch: %d, rmtChannel: %u", gpio, ch, rmtChannel);
                pinMode(pin, OUTPUT);
                digitalWrite(pin, LOW);
                dshotInstances[ch] = new DShotRMT(gpio, rmtChannel); // Initialize the DShotRMT instance
                rmtCH++;
            }
            pin = UNDEF_PIN;
        }
#endif
        servoPins[ch] = pin;
        // Initialize all servos to low ASAP
        if (pin != UNDEF_PIN)
        {
            if (mode == somOnOff)
            {
                DBGLN("Initializing digital output: ch: %d, pin: %d", ch, pin);
            }
            else
            {
                DBGLN("Initializing PWM output: ch: %d, pin: %d", ch, pin);
            }

#if !defined(M0139)
            pinMode(pin, OUTPUT);
            digitalWrite(pin, LOW);
#endif
        }
    }
}

static void allocateOutputs()
{
    for (int ch = 0; ch < GPIO_PIN_PWM_OUTPUTS_COUNT; ++ch)
    {
        const rx_config_pwm_t *chConfig = config.GetPwmChannel(ch);
        const auto frequency = servoOutputModeToFrequency((eServoOutputMode)chConfig->val.mode);
        if (frequency && servoPins[ch] != UNDEF_PIN)
        {
            pwmChannels[ch] = PWM.allocate(servoPins[ch], frequency);
        }
#if defined(PLATFORM_ESP32)
        else if (((eServoOutputMode)chConfig->val.mode == somDShot || (eServoOutputMode)chConfig->val.mode == somDShot3D) && dshotInstances[ch])
        {
            dshotInstances[ch]->begin(DSHOT300, false);
        }
#endif
    }
    initialized = true;
}

#if defined(M0139)
static uint32_t computePwmConfigHash()
{
    uint32_t hash = 0;
    for (int ch = 0; ch < GPIO_PIN_PWM_OUTPUTS_COUNT; ++ch)
    {
        const rx_config_pwm_t *channel = config.GetPwmChannel(ch);
        for (const uint32_t value : channel->raw)
        {
            hash ^= value;
            hash = (hash << 5) | (hash >> 27);
        }
    }
    return hash;
}

static void reinitializeOutputs()
{
    releaseOutputs();
    configureOutputPins();
    allocateOutputs();
    servosFailsafe();
    pwmConfigHash = computePwmConfigHash();
}
#endif

static bool initialize()
{
    if (!OPT_HAS_SERVO_OUTPUT)
    {
        return false;
    }

#if defined(M0139)
    PWM.initialize();
#endif

    configureOutputPins();
    return true;
}

static int start()
{
#if defined(M0139)
    allocateOutputs();
    servosFailsafe();
    pwmConfigHash = computePwmConfigHash();
    return DISCONNECTED_UPDATE_MS;
#else
    return DURATION_NEVER;
#endif
}

static int event()
{
#if defined(M0139)
    if (computePwmConfigHash() != pwmConfigHash)
    {
        reinitializeOutputs();
    }
#endif

    if (connectionState == wifiUpdate)
    {
        servosEnterFailsafe();
        releaseOutputs();
        return DURATION_NEVER;
    }
    if (connectionState != connected)
    {
#if defined(M0139)
        if (lastOverrideUpdate && millis() - lastOverrideUpdate < FAILSAFE_ABS_TIMEOUT_MS)
        {
            return DURATION_IMMEDIATELY;
        }
#endif
        servosEnterFailsafe();
        // When disconnected, return a timeout to feed the ISR watchdog for the PWM signals
        return connectionState == disconnected ? DISCONNECTED_UPDATE_MS : DURATION_IMMEDIATELY;
    }
    if (!initialized && connectionState == connected)
    {
        configureOutputPins();
        allocateOutputs();
    }
    return DURATION_IMMEDIATELY;
}

#if defined(M0139)
bool servoApplyConfig(const modal_pwm_config_t &newConfig)
{
    if (newConfig.pinIndex >= GPIO_PIN_PWM_OUTPUTS_COUNT || newConfig.inputChannel >= CRSF_NUM_CHANNELS)
    {
        return false;
    }

    rx_config_pwm_t channel = *config.GetPwmChannel(newConfig.pinIndex);
    constexpr uint16_t legacyFailsafeBase = 800;
    constexpr uint16_t failsafeBaseDelta = legacyFailsafeBase - US_CHANNEL_VALUE_MIN;
    channel.val.failsafe = constrain((uint32_t)newConfig.failsafe + failsafeBaseDelta, 0U, 2047U);
    channel.val.inputChannel = newConfig.inputChannel;
    channel.val.inverted = newConfig.inverted;
    channel.val.mode = newConfig.mode;
    channel.val.narrow = newConfig.narrow;
    channel.val.failsafeMode = newConfig.failsafeMode;
    channel.val.mapMode = newConfig.mapMode;
    channel.val.mapInVal1 = newConfig.mapInVal1 >> 1;
    channel.val.mapInVal2 = newConfig.mapInVal2 >> 1;
    channel.val.mapInVal3 = newConfig.mapInVal3 >> 1;
    channel.val.mapOutVal1 = newConfig.mapOutVal1 >> 1;
    channel.val.mapOutVal2 = newConfig.mapOutVal2 >> 1;
    channel.val.mapOutVal3 = newConfig.mapOutVal3 >> 1;
    config.SetPwmChannelRaw(newConfig.pinIndex, channel.raw);
    return true;
}

bool servoOverrideChannel(const modal_pwm_override_t &overrideValue)
{
    if (overrideValue.command != SET_PWM_VAL || overrideValue.rcChannel == 0 || overrideValue.rcChannel > CRSF_NUM_CHANNELS)
    {
        return false;
    }

    const uint32_t now = millis();
    ChannelData[overrideValue.rcChannel - 1] = be16toh(overrideValue.crsfChannelValue);
    lastOverrideUpdate = now;
    newChannelsAvailable = true;
    if (!initialized)
    {
        reinitializeOutputs();
    }
    servosUpdate(now);
    return true;
}

void servoSetArmed(bool armed)
{
    if (pwmIsArmed == armed)
    {
        return;
    }

    pwmIsArmed = armed;
    newChannelsAvailable = true;
    servosUpdate(millis());
}

bool servoIsArmed()
{
    return pwmIsArmed;
}
#endif

static int timeout()
{
    servosUpdate(millis());
    // When disconnected, return a timeout to feed the ISR watchdog for the PWM signals
    return connectionState == disconnected ? DISCONNECTED_UPDATE_MS : DURATION_IMMEDIATELY;
}

#if defined(PLATFORM_ESP32)
// High priority constructor function to reset the hardware modules used for PWM/DShot output.
// Restarts can leave PWM/RMT peripherals running; so reset them early in the app start process.
__attribute__((constructor(101))) void resetEsp32PwmDevices()
{
    periph_module_reset(PERIPH_LEDC_MODULE);
    periph_module_reset(PERIPH_RMT_MODULE);
#if SOC_MCPWM_SUPPORTED
    periph_module_reset(PERIPH_PWM0_MODULE);
#if SOC_MCPWM_GROUPS > 1
    periph_module_reset(PERIPH_PWM1_MODULE);
#endif
#endif
}
#endif

device_t ServoOut_device = {
    .initialize = initialize,
    .start = start,
    .event = event,
    .timeout = timeout,
    .subscribe = EVENT_CONNECTION_CHANGED
#if defined(M0139)
        | EVENT_CONFIG_PWM_CHANGE
#endif
};

#endif
