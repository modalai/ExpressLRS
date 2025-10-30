#ifdef TARGET_RX

#include "rxtx_devLua.h"
#include "helpers.h"
#include "devServoOutput.h"
#include "deferred.h"
#include "logging.h"

#define RX_HAS_SERIAL1 (GPIO_PIN_SERIAL1_TX != UNDEF_PIN || OPT_HAS_SERVO_OUTPUT)

extern void reconfigureSerial();
#if defined(PLATFORM_ESP32)
extern void reconfigureSerial1();
#endif
extern bool BindingModeRequest;

static char modelString[] = "000";

static struct luaItem_selection luaSerialProtocol = {
    {"Protocol", CRSF_TEXT_SELECTION},
    0, // value
    "CRSF;Inverted CRSF;SBUS;Inverted SBUS;SUMD;DJI RS Pro;HoTT Telemetry;MAVLink;IBus",
    STR_EMPTYSPACE
};

#if defined(PLATFORM_ESP32)
static struct luaItem_selection luaSerial1Protocol = {
    {"Protocol2", CRSF_TEXT_SELECTION},
    0, // value
    "Off;CRSF;Inverted CRSF;SBUS;Inverted SBUS;SUMD;DJI RS Pro;HoTT Telemetry;Tramp;SmartAudio;DisplayPort",
    STR_EMPTYSPACE
};
#endif

static struct luaItem_selection luaSBUSFailsafeMode = {
    {"SBUS failsafe", CRSF_TEXT_SELECTION},
    0, // value
    "No Pulses;Last Pos",
    STR_EMPTYSPACE
};

static struct luaItem_int8 luaTargetSysId = {
  {"Target SysID", CRSF_UINT8},
  {
    {
      (uint8_t)1,       // value - default to 1
      (uint8_t)1,       // min
      (uint8_t)255,     // max
    }
  },
  STR_EMPTYSPACE
};
static struct luaItem_int8 luaSourceSysId = {
  {"Source SysID", CRSF_UINT8},
  {
    {
      (uint8_t)255,       // value - default to 255
      (uint8_t)1,         // min
      (uint8_t)255,       // max
    }
  },
  STR_EMPTYSPACE
};

#if defined(POWER_OUTPUT_VALUES)
static struct luaItem_selection luaTlmPower = {
    {"Tlm Power", CRSF_TEXT_SELECTION},
    0, // value
    strPowerLevels,
    "mW"
};
#endif

#if defined(GPIO_PIN_ANT_CTRL)
static struct luaItem_selection luaAntennaMode = {
    {"Ant. Mode", CRSF_TEXT_SELECTION},
    0, // value
    "Antenna A;Antenna B;Diversity",
    STR_EMPTYSPACE
};
#endif

// Gemini Mode
#if defined(GPIO_PIN_NSS_2)
static struct luaItem_selection luaDiversityMode = {
    {"Rx Mode", CRSF_TEXT_SELECTION},
    0, // value
    "Diversity;Gemini",
    STR_EMPTYSPACE
};
#endif

static struct luaItem_folder luaTeamraceFolder = {
    {"Team Race", CRSF_FOLDER},
};

static struct luaItem_selection luaTeamraceChannel = {
    {"Channel", CRSF_TEXT_SELECTION},
    0, // value
    "AUX2;AUX3;AUX4;AUX5;AUX6;AUX7;AUX8;AUX9;AUX10;AUX11;AUX12",
    STR_EMPTYSPACE
};

static struct luaItem_selection luaTeamracePosition = {
    {"Position", CRSF_TEXT_SELECTION},
    0, // value
    "Disabled;1/Low;2;3;Mid;4;5;6/High",
    STR_EMPTYSPACE
};

//----------------------------Info-----------------------------------

static struct luaItem_string luaModelNumber = {
    {"Model Id", CRSF_INFO},
    modelString
};

static struct luaItem_string luaELRSversion = {
    {version, CRSF_INFO},
    commit
};

//----------------------------Info-----------------------------------

//---------------------------- WiFi -----------------------------


//---------------------------- WiFi -----------------------------

//---------------------------- Output Mapping -----------------------------

#if defined(GPIO_PIN_PWM_OUTPUTS)

// Zero-overhead per-pin folder design
// Shared string literals (compiler automatically deduplicates)
static const char STR_INPUT_CH[] = "Input Channel";
static const char STR_OUTPUT_MODE[] = "Output Mode";
static const char STR_INVERT[] = "Invert Output";
static const char STR_NARROW[] = "Narrow Pulse";
static const char STR_FAILSAFE_MODE[] = "Failsafe Mode";
static const char STR_FAILSAFE_VAL[] = "Failsafe (us)";
static const char STR_MAP_MODE[] = "Map Mode";
static const char STR_REQUIRES_ARM[] = "Requires Arm";
static const char STR_PIN_1[] = "Pin 1";
static const char STR_PIN_2[] = "Pin 2";
static const char STR_PIN_3[] = "Pin 3";
static const char STR_PIN_4[] = "Pin 4";

// Shared option strings
static const char STR_PWM_MODES[] = "50Hz;60Hz;100Hz;160Hz;333Hz;400Hz;10kHz Duty;On/Off";
static const char STR_ON_OFF[] = "Off;On";
static const char STR_FAILSAFE_MODES[] = "No Pulses;Last Pos;Custom";
static const char STR_MAP_MODES[] = "Off;3-Point";

// Main folder
static struct luaItem_folder luaMappingFolder = {
    {"Output Mapping", CRSF_FOLDER},
};

// Pin folders (static - no runtime allocation)
static struct luaItem_folder luaPinFolder0 = {{STR_PIN_1, CRSF_FOLDER}};
static struct luaItem_folder luaPinFolder1 = {{STR_PIN_2, CRSF_FOLDER}};
static struct luaItem_folder luaPinFolder2 = {{STR_PIN_3, CRSF_FOLDER}};
static struct luaItem_folder luaPinFolder3 = {{STR_PIN_4, CRSF_FOLDER}};

// Macros to define parameters for one pin
#define PWM_INPUT_CH_PARAM(n) \
    static struct luaItem_int8 luaPwmInputCh##n = { \
        {STR_INPUT_CH, CRSF_UINT8}, \
        {{1, 1, CRSF_NUM_CHANNELS}}, \
        STR_EMPTYSPACE \
    }

#define PWM_OUTPUT_MODE_PARAM(n) \
    static struct luaItem_selection luaPwmMode##n = { \
        {STR_OUTPUT_MODE, CRSF_TEXT_SELECTION}, \
        0, \
        STR_PWM_MODES, \
        STR_EMPTYSPACE \
    }

#define PWM_INVERT_PARAM(n) \
    static struct luaItem_selection luaPwmInvert##n = { \
        {STR_INVERT, CRSF_TEXT_SELECTION}, \
        0, \
        STR_ON_OFF, \
        STR_EMPTYSPACE \
    }

#define PWM_NARROW_PARAM(n) \
    static struct luaItem_selection luaPwmNarrow##n = { \
        {STR_NARROW, CRSF_TEXT_SELECTION}, \
        0, \
        STR_ON_OFF, \
        STR_EMPTYSPACE \
    }

#define PWM_FAILSAFE_MODE_PARAM(n) \
    static struct luaItem_selection luaPwmFailsafeMode##n = { \
        {STR_FAILSAFE_MODE, CRSF_TEXT_SELECTION}, \
        0, \
        STR_FAILSAFE_MODES, \
        STR_EMPTYSPACE \
    }

#define PWM_FAILSAFE_VAL_PARAM(n) \
    static struct luaItem_int16 luaPwmFailsafeVal##n = { \
        {STR_FAILSAFE_VAL, CRSF_UINT16}, \
        {{1500, 988, 2012}}, \
        "us" \
    }

#define PWM_MAP_MODE_PARAM(n) \
    static struct luaItem_selection luaPwmMapMode##n = { \
        {STR_MAP_MODE, CRSF_TEXT_SELECTION}, \
        0, \
        STR_MAP_MODES, \
        STR_EMPTYSPACE \
    }

#define PWM_MAP_VALUES_PARAM(n) \
    static char luaPwmMapValuesStr##n[20]; \
    static struct luaItem_string luaPwmMapValues##n = { \
        {"Map Values", CRSF_INFO}, \
        luaPwmMapValuesStr##n \
    }

#define PWM_REQUIRES_ARM_PARAM(n) \
    static struct luaItem_selection luaPwmRequiresArm##n = { \
        {STR_REQUIRES_ARM, CRSF_TEXT_SELECTION}, \
        0, \
        STR_ON_OFF, \
        STR_EMPTYSPACE \
    }

// Expand for all pins
PWM_INPUT_CH_PARAM(0);
PWM_INPUT_CH_PARAM(1);
PWM_INPUT_CH_PARAM(2);
PWM_INPUT_CH_PARAM(3);

PWM_OUTPUT_MODE_PARAM(0);
PWM_OUTPUT_MODE_PARAM(1);
PWM_OUTPUT_MODE_PARAM(2);
PWM_OUTPUT_MODE_PARAM(3);

PWM_INVERT_PARAM(0);
PWM_INVERT_PARAM(1);
PWM_INVERT_PARAM(2);
PWM_INVERT_PARAM(3);

PWM_NARROW_PARAM(0);
PWM_NARROW_PARAM(1);
PWM_NARROW_PARAM(2);
PWM_NARROW_PARAM(3);

PWM_FAILSAFE_MODE_PARAM(0);
PWM_FAILSAFE_MODE_PARAM(1);
PWM_FAILSAFE_MODE_PARAM(2);
PWM_FAILSAFE_MODE_PARAM(3);

PWM_FAILSAFE_VAL_PARAM(0);
PWM_FAILSAFE_VAL_PARAM(1);
PWM_FAILSAFE_VAL_PARAM(2);
PWM_FAILSAFE_VAL_PARAM(3);

PWM_MAP_MODE_PARAM(0);
PWM_MAP_MODE_PARAM(1);
PWM_MAP_MODE_PARAM(2);
PWM_MAP_MODE_PARAM(3);

PWM_MAP_VALUES_PARAM(0);
PWM_MAP_VALUES_PARAM(1);
PWM_MAP_VALUES_PARAM(2);
PWM_MAP_VALUES_PARAM(3);

PWM_REQUIRES_ARM_PARAM(0);
PWM_REQUIRES_ARM_PARAM(1);
PWM_REQUIRES_ARM_PARAM(2);
PWM_REQUIRES_ARM_PARAM(3);

// Generic callbacks - use pointer comparison to determine pin

static void pwmInputChCallback(struct luaPropertiesCommon *item, uint8_t arg)
{
    uint8_t pin = (item == &luaPwmInputCh0.common) ? 0 :
                  (item == &luaPwmInputCh1.common) ? 1 :
                  (item == &luaPwmInputCh2.common) ? 2 : 3;

    if (arg == 0 || arg > CRSF_NUM_CHANNELS) return;

    rx_config_pwm_t cfg;
    cfg.raw = config.GetPwmChannel(pin)->raw;
    cfg.val.inputChannel = arg - 1;
    config.SetPwmChannelRaw(pin, cfg.raw.raw);
    devicesTriggerEvent();
}

static void pwmModeCallback(struct luaPropertiesCommon *item, uint8_t arg)
{
    uint8_t pin = (item == &luaPwmMode0.common) ? 0 :
                  (item == &luaPwmMode1.common) ? 1 :
                  (item == &luaPwmMode2.common) ? 2 : 3;

    rx_config_pwm_t cfg;
    cfg.raw = config.GetPwmChannel(pin)->raw;
    cfg.val.mode = arg;
    config.SetPwmChannelRaw(pin, cfg.raw.raw);
    devicesTriggerEvent();
}

static void pwmInvertCallback(struct luaPropertiesCommon *item, uint8_t arg)
{
    uint8_t pin = (item == &luaPwmInvert0.common) ? 0 :
                  (item == &luaPwmInvert1.common) ? 1 :
                  (item == &luaPwmInvert2.common) ? 2 : 3;

    rx_config_pwm_t cfg;
    cfg.raw = config.GetPwmChannel(pin)->raw;
    cfg.val.inverted = arg;
    config.SetPwmChannelRaw(pin, cfg.raw.raw);
    devicesTriggerEvent();
}

static void pwmNarrowCallback(struct luaPropertiesCommon *item, uint8_t arg)
{
    uint8_t pin = (item == &luaPwmNarrow0.common) ? 0 :
                  (item == &luaPwmNarrow1.common) ? 1 :
                  (item == &luaPwmNarrow2.common) ? 2 : 3;

    rx_config_pwm_t cfg;
    cfg.raw = config.GetPwmChannel(pin)->raw;
    cfg.val.narrow = arg;
    config.SetPwmChannelRaw(pin, cfg.raw.raw);
    devicesTriggerEvent();
}

static void pwmFailsafeModeCallback(struct luaPropertiesCommon *item, uint8_t arg)
{
    uint8_t pin = (item == &luaPwmFailsafeMode0.common) ? 0 :
                  (item == &luaPwmFailsafeMode1.common) ? 1 :
                  (item == &luaPwmFailsafeMode2.common) ? 2 : 3;

    rx_config_pwm_t cfg;
    cfg.raw = config.GetPwmChannel(pin)->raw;
    cfg.val.failsafeMode = arg;
    config.SetPwmChannelRaw(pin, cfg.raw.raw);
    devicesTriggerEvent();
}

static void pwmFailsafeValCallback(struct luaPropertiesCommon *item, uint8_t arg)
{
    uint8_t pin = (item == &luaPwmFailsafeVal0.common) ? 0 :
                  (item == &luaPwmFailsafeVal1.common) ? 1 :
                  (item == &luaPwmFailsafeVal2.common) ? 2 : 3;

    if (arg < 988 || arg > 2012) return;

    rx_config_pwm_t cfg;
    cfg.raw = config.GetPwmChannel(pin)->raw;
    cfg.val.failsafe = arg - 988;  // Stored as offset from 988us
    config.SetPwmChannelRaw(pin, cfg.raw.raw);
    devicesTriggerEvent();
}

static void pwmMapModeCallback(struct luaPropertiesCommon *item, uint8_t arg)
{
    uint8_t pin = (item == &luaPwmMapMode0.common) ? 0 :
                  (item == &luaPwmMapMode1.common) ? 1 :
                  (item == &luaPwmMapMode2.common) ? 2 : 3;

    rx_config_pwm_t cfg;
    cfg.raw = config.GetPwmChannel(pin)->raw;
    cfg.val.mapMode = arg;
    config.SetPwmChannelRaw(pin, cfg.raw.raw);
    devicesTriggerEvent();
}


static void pwmRequiresArmCallback(struct luaPropertiesCommon *item, uint8_t arg)
{
    uint8_t pin = (item == &luaPwmRequiresArm0.common) ? 0 :
                  (item == &luaPwmRequiresArm1.common) ? 1 :
                  (item == &luaPwmRequiresArm2.common) ? 2 : 3;

    rx_config_pwm_t cfg;
    cfg.raw = config.GetPwmChannel(pin)->raw;
    cfg.val.requiresArm = arg;
    config.SetPwmChannelRaw(pin, cfg.raw.raw);
    devicesTriggerEvent();
}

// Format map values as hex string (63 bits packed into 8 bytes = 16 hex chars)
static void formatMapValues(const rx_config_pwm_t *cfg, char *out)
{
    // Pack all map values into 64 bits (8 bytes)
    uint64_t packed = 0;
    packed |= ((uint64_t)cfg->val.mapInVal1 & 0x3FF) << 0;   // bits 0-9
    packed |= ((uint64_t)cfg->val.mapInVal2 & 0x3FF) << 10;  // bits 10-19
    packed |= ((uint64_t)cfg->val.mapInVal3 & 0x3FF) << 20;  // bits 20-29
    packed |= ((uint64_t)cfg->val.mapOutVal1 & 0x7FF) << 30; // bits 30-40
    packed |= ((uint64_t)cfg->val.mapOutVal2 & 0x7FF) << 41; // bits 41-51
    packed |= ((uint64_t)cfg->val.mapOutVal3 & 0x7FF) << 52; // bits 52-62

    // Format as 16-character hex string
    snprintf(out, 20, "%016llX", (unsigned long long)packed);
}

#endif // GPIO_PIN_PWM_OUTPUTS

//---------------------------- Output Mapping -----------------------------

static struct luaItem_selection luaBindStorage = {
    {"Bind Storage", CRSF_TEXT_SELECTION},
    0, // value
    "Persistent;Volatile;Returnable",
    STR_EMPTYSPACE
};

static struct luaItem_command luaBindMode = {
    {STR_EMPTYSPACE, CRSF_COMMAND},
    lcsIdle, // step
    STR_EMPTYSPACE
};

// Old PWM callbacks removed - replaced with minimal per-pin callbacks above

#if defined(GPIO_PIN_PWM_OUTPUTS)
// Old callback code removed
#if 0
static void luaparamMappingChannelOut(struct luaPropertiesCommon *item, uint8_t arg)
{
    bool sclAssigned = false;
    bool sdaAssigned = false;
#if defined(PLATFORM_ESP32)
    bool serial1rxAssigned = false;
    bool serial1txAssigned = false;
#endif

    const char *no1Option    = ";";
    const char *no2Options   = ";;";
    const char *dshot        = ";DShot";
    const char *serial_RX    = ";Serial RX";
    const char *serial_TX    = ";Serial TX";
    const char *i2c_SCL      = ";I2C SCL;";
    const char *i2c_SDA      = ";;I2C SDA";
    const char *i2c_BOTH     = ";I2C SCL;I2C SDA";
#if defined(PLATFORM_ESP32)
    const char *serial1_RX   = ";Serial2 RX;";
    const char *serial1_TX   = ";;Serial2 TX";
    const char *serial1_BOTH = ";Serial2 RX;Serial2 TX";
#endif

    const char *pModeString;


    // find out if use once only modes have already been assigned
    for (uint8_t ch = 0; ch < GPIO_PIN_PWM_OUTPUTS_COUNT; ch++)
    {
      if (ch == (arg -1))
        continue;

      eServoOutputMode mode = (eServoOutputMode)config.GetPwmChannel(ch)->val.mode;

      if (mode == somSCL)
        sclAssigned = true;

      if (mode == somSDA)
        sdaAssigned = true;

#if defined(PLATFORM_ESP32)
      if (mode == somSerial1RX)
        serial1rxAssigned = true;

      if (mode == somSerial1TX)
        serial1txAssigned = true;
#endif
    }

    setLuaUint8Value(&luaMappingChannelOut, arg);

    // When the selected output channel changes, update the available PWM modes for that pin
    // Truncate the select options before the ; following On/Off
    pwmModes[50] = '\0';

#if defined(PLATFORM_ESP32)
    // DShot output (1 option)
    // ;DShot
    // ESP8266 enum skips this, so it is never present
    if (GPIO_PIN_PWM_OUTPUTS[arg-1] != 0)   // DShot doesn't work with GPIO0, exclude it
    {
        pModeString = dshot;
    }
    else
#endif
    {
        pModeString = no1Option;
    }
    strcat(pwmModes, pModeString);

    // SerialIO outputs (1 option)
    // ;[Serial RX] | [Serial TX]
    if (GPIO_PIN_PWM_OUTPUTS[arg-1] == U0RXD_GPIO_NUM)
    {
        pModeString = serial_RX;
    }
    else if (GPIO_PIN_PWM_OUTPUTS[arg-1] == U0TXD_GPIO_NUM)
    {
        pModeString = serial_TX;
    }
    else
    {
        pModeString = no1Option;
    }
    strcat(pwmModes, pModeString);

    // I2C pins (2 options)
    // ;[I2C SCL] ;[I2C SDA]
    if (GPIO_PIN_SCL != UNDEF_PIN || GPIO_PIN_SDA != UNDEF_PIN)
    {
        // If the target defines SCL/SDA then those pins MUST be used
        if (GPIO_PIN_PWM_OUTPUTS[arg-1] == GPIO_PIN_SCL)
        {
            pModeString = i2c_SCL;
        }
        else if (GPIO_PIN_PWM_OUTPUTS[arg-1] == GPIO_PIN_SDA)
        {
            pModeString = i2c_SDA;
        }
        else
        {
            pModeString = no2Options;
        }
    }
    else
    {
        // otherwise allow any pin to be either SCL or SDA but only once
        if (sclAssigned && !sdaAssigned)
        {
            pModeString = i2c_SDA;
        }
        else if (sdaAssigned && !sclAssigned)
        {
            pModeString = i2c_SCL;
        }
        else if (!sclAssigned && !sdaAssigned)
        {
            pModeString = i2c_BOTH;
        }
        else
        {
            pModeString = no2Options;
        }
    }
    strcat(pwmModes, pModeString);

    // nothing to do for unsupported somPwm mode
    strcat(pwmModes, no1Option);

#if defined(PLATFORM_ESP32)
    // secondary Serial pins (2 options)
    // ;[SERIAL2 RX] ;[SERIAL2_TX]
    if (GPIO_PIN_SERIAL1_RX != UNDEF_PIN || GPIO_PIN_SERIAL1_TX != UNDEF_PIN)
    {
        // If the target defines Serial2 RX/TX then those pins MUST be used
        if (GPIO_PIN_PWM_OUTPUTS[arg-1] == GPIO_PIN_SERIAL1_RX)
        {
            pModeString = serial1_RX;
        }
        else if (GPIO_PIN_PWM_OUTPUTS[arg-1] == GPIO_PIN_SERIAL1_TX)
        {
            pModeString = serial1_TX;
        }
        else
        { 
            pModeString = no2Options;
        }
    } 
    else
    {   // otherwise allow any pin to be either RX or TX but only once
        if (serial1txAssigned && !serial1rxAssigned)
        {
            pModeString = serial1_RX;
        }        
        else if (serial1rxAssigned && !serial1txAssigned)
        {
            pModeString = serial1_TX;
        }

        else if (!serial1rxAssigned && !serial1txAssigned)
        {
            pModeString = serial1_BOTH;
        } 
        else
        {
            pModeString = no2Options;
        }
    }
    strcat(pwmModes, pModeString);
#endif

    // trim off trailing semicolons (assumes pwmModes has at least 1 non-semicolon)
    for (auto lastPos = strlen(pwmModes)-1; pwmModes[lastPos] == ';'; lastPos--)
    {
        pwmModes[lastPos] = '\0';
    }

    // Trigger an event to update the related fields to represent the selected channel
    devicesTriggerEvent();
}

static void luaparamMappingChannelIn(struct luaPropertiesCommon *item, uint8_t arg)
{
  const uint8_t ch = luaMappingChannelOut.properties.u.value - 1;
  rx_config_pwm_t newPwmCh;
  newPwmCh.raw = config.GetPwmChannel(ch)->raw;
  newPwmCh.val.inputChannel = arg - 1; // convert 1-16 -> 0-15

  config.SetPwmChannelRaw(ch, newPwmCh.raw.raw);
}

static void configureSerialPin(uint8_t sibling, uint8_t oldMode, uint8_t newMode)
{
  for (int ch=0 ; ch<GPIO_PIN_PWM_OUTPUTS_COUNT ; ch++)
  {
    if (GPIO_PIN_PWM_OUTPUTS[ch] == sibling)
    {
      // Retain as much of the sibling's current config as possible
      rx_config_pwm_t siblingPinConfig;
      siblingPinConfig.raw = config.GetPwmChannel(ch)->raw;

      // If the new mode is serial, the sibling is also forced to serial
      if (newMode == somSerial)
      {
        siblingPinConfig.val.mode = somSerial;
      }
      // If the new mode is not serial, and the sibling is serial, set the sibling to PWM (50Hz)
      else if (siblingPinConfig.val.mode == somSerial)
      {
        siblingPinConfig.val.mode = som50Hz;
      }

      config.SetPwmChannelRaw(ch, siblingPinConfig.raw.raw);
      break;
    }
  }

  if (oldMode != newMode)
  {
    deferExecutionMillis(100, [](){
      reconfigureSerial();
    });
  }
}

static void luaparamMappingOutputMode(struct luaPropertiesCommon *item, uint8_t arg)
{
  UNUSED(item);
  const uint8_t ch = luaMappingChannelOut.properties.u.value - 1;
  rx_config_pwm_t newPwmCh;
  newPwmCh.raw = config.GetPwmChannel(ch)->raw;
  uint8_t oldMode = newPwmCh.val.mode;
  newPwmCh.val.mode = arg;

  // Check if pin == 1/3 and do other pin adjustment accordingly
  if (GPIO_PIN_PWM_OUTPUTS[ch] == 1)
  {
    configureSerialPin(3, oldMode, newPwmCh.val.mode);
  }
  else if (GPIO_PIN_PWM_OUTPUTS[ch] == 3)
  {
    configureSerialPin(1, oldMode, newPwmCh.val.mode);
  }
  config.SetPwmChannelRaw(ch, newPwmCh.raw.raw);
}

static void luaparamMappingInverted(struct luaPropertiesCommon *item, uint8_t arg)
{
  UNUSED(item);
  const uint8_t ch = luaMappingChannelOut.properties.u.value - 1;
  rx_config_pwm_t newPwmCh;
  newPwmCh.raw = config.GetPwmChannel(ch)->raw;
  newPwmCh.val.inverted = arg;

  config.SetPwmChannelRaw(ch, newPwmCh.raw.raw);
}

static void luaparamSetFailsafe(struct luaPropertiesCommon *item, uint8_t arg)
{
  luaCmdStep_e newStep;
  const char *msg;
  if (arg == lcsClick)
  {
    newStep = lcsAskConfirm;
    msg = "Set failsafe to curr?";
  }
  else if (arg == lcsConfirmed)
  {
    // This is generally not seen by the user, since we'll disconnect to commit config
    // and the handset will send another lcdQuery that will overwrite it with idle
    newStep = lcsExecuting;
    msg = "Setting failsafe";

    for (int ch=0; ch<GPIO_PIN_PWM_OUTPUTS_COUNT; ++ch)
    {
      rx_config_pwm_t newPwmCh;
      // The value must fit into the 10 bit range of the failsafe
      newPwmCh.raw = config.GetPwmChannel(ch)->raw;
      newPwmCh.val.failsafe = CRSF_to_UINT10(constrain(ChannelData[config.GetPwmChannel(ch)->val.inputChannel], CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX));
      //DBGLN("FSCH(%u) crsf=%u us=%u", ch, ChannelData[ch], newPwmCh.val.failsafe+988U);
      config.SetPwmChannelRaw(ch, newPwmCh.raw.raw);
    }
  }
  else
  {
    newStep = lcsIdle;
    msg = STR_EMPTYSPACE;
  }

  sendLuaCommandResponse((struct luaItem_command *)item, newStep, msg);
}
#endif // #if 0 - old callback code

#endif // GPIO_PIN_PWM_OUTPUTS

#if defined(POWER_OUTPUT_VALUES)

static void luaparamSetPower(struct luaPropertiesCommon* item, uint8_t arg)
{
  UNUSED(item);
  uint8_t newPower = arg + POWERMGNT::getMinPower();
  if (newPower > POWERMGNT::getMaxPower())
  {
    newPower = PWR_MATCH_TX;
  }

  config.SetPower(newPower);
  // POWERMGNT::setPower() will be called in updatePower() in the main loop
}

#endif // POWER_OUTPUT_VALUES

static void registerLuaParameters()
{
  registerLUAParameter(&luaSerialProtocol, [](struct luaPropertiesCommon* item, uint8_t arg){
    config.SetSerialProtocol((eSerialProtocol)arg);
    if (config.IsModified()) {
      deferExecutionMillis(100, [](){
        reconfigureSerial();
      });
    }
  });

#if defined(PLATFORM_ESP32)
  if (RX_HAS_SERIAL1)
  {
    registerLUAParameter(&luaSerial1Protocol, [](struct luaPropertiesCommon* item, uint8_t arg){
      config.SetSerial1Protocol((eSerial1Protocol)arg);
      if (config.IsModified()) {
        deferExecutionMillis(100, [](){
          reconfigureSerial1();
        });
      }
    });
  }
#endif

  registerLUAParameter(&luaSBUSFailsafeMode, [](struct luaPropertiesCommon* item, uint8_t arg){
    config.SetFailsafeMode((eFailsafeMode)arg);
  });

  registerLUAParameter(&luaTargetSysId, [](struct luaPropertiesCommon* item, uint8_t arg){
    config.SetTargetSysId((uint8_t)arg);
  });
  registerLUAParameter(&luaSourceSysId, [](struct luaPropertiesCommon* item, uint8_t arg){
    config.SetSourceSysId((uint8_t)arg);
  });

  if (GPIO_PIN_ANT_CTRL != UNDEF_PIN)
  {
    registerLUAParameter(&luaAntennaMode, [](struct luaPropertiesCommon* item, uint8_t arg){
      config.SetAntennaMode(arg);
    });
  }

  // Gemini Mode
  if (isDualRadio())
  {
    registerLUAParameter(&luaDiversityMode, [](struct luaPropertiesCommon* item, uint8_t arg){
      config.SetAntennaMode(arg); // Reusing SetAntennaMode since both GPIO_PIN_ANTENNA_SELECT and GPIO_PIN_NSS_2 will not be defined together.
    });
  }

#if defined(POWER_OUTPUT_VALUES)
  luadevGeneratePowerOpts(&luaTlmPower);
  registerLUAParameter(&luaTlmPower, &luaparamSetPower);
#endif

  // Teamrace
  registerLUAParameter(&luaTeamraceFolder);
  registerLUAParameter(&luaTeamraceChannel, [](struct luaPropertiesCommon* item, uint8_t arg) {
    config.SetTeamraceChannel(arg + AUX2);
  }, luaTeamraceFolder.common.id);
  registerLUAParameter(&luaTeamracePosition, [](struct luaPropertiesCommon* item, uint8_t arg) {
    config.SetTeamracePosition(arg);
  }, luaTeamraceFolder.common.id);

#if defined(GPIO_PIN_PWM_OUTPUTS)
  if (OPT_HAS_SERVO_OUTPUT)
  {
    // Register main Output Mapping folder
    registerLUAParameter(&luaMappingFolder);

    // Register per-pin folders and all parameters for each pin
    registerLUAParameter(&luaPinFolder0, nullptr, luaMappingFolder.common.id);
    registerLUAParameter(&luaPwmInputCh0, &pwmInputChCallback, luaPinFolder0.common.id);
    registerLUAParameter(&luaPwmMode0, &pwmModeCallback, luaPinFolder0.common.id);
    registerLUAParameter(&luaPwmInvert0, &pwmInvertCallback, luaPinFolder0.common.id);
    registerLUAParameter(&luaPwmNarrow0, &pwmNarrowCallback, luaPinFolder0.common.id);
    registerLUAParameter(&luaPwmFailsafeMode0, &pwmFailsafeModeCallback, luaPinFolder0.common.id);
    registerLUAParameter(&luaPwmFailsafeVal0, &pwmFailsafeValCallback, luaPinFolder0.common.id);
    registerLUAParameter(&luaPwmMapMode0, &pwmMapModeCallback, luaPinFolder0.common.id);
    registerLUAParameter(&luaPwmMapValues0, nullptr, luaPinFolder0.common.id);
    registerLUAParameter(&luaPwmRequiresArm0, &pwmRequiresArmCallback, luaPinFolder0.common.id);

    registerLUAParameter(&luaPinFolder1, nullptr, luaMappingFolder.common.id);
    registerLUAParameter(&luaPwmInputCh1, &pwmInputChCallback, luaPinFolder1.common.id);
    registerLUAParameter(&luaPwmMode1, &pwmModeCallback, luaPinFolder1.common.id);
    registerLUAParameter(&luaPwmInvert1, &pwmInvertCallback, luaPinFolder1.common.id);
    registerLUAParameter(&luaPwmNarrow1, &pwmNarrowCallback, luaPinFolder1.common.id);
    registerLUAParameter(&luaPwmFailsafeMode1, &pwmFailsafeModeCallback, luaPinFolder1.common.id);
    registerLUAParameter(&luaPwmFailsafeVal1, &pwmFailsafeValCallback, luaPinFolder1.common.id);
    registerLUAParameter(&luaPwmMapMode1, &pwmMapModeCallback, luaPinFolder1.common.id);
    registerLUAParameter(&luaPwmMapValues1, nullptr, luaPinFolder1.common.id);
    registerLUAParameter(&luaPwmRequiresArm1, &pwmRequiresArmCallback, luaPinFolder1.common.id);

    registerLUAParameter(&luaPinFolder2, nullptr, luaMappingFolder.common.id);
    registerLUAParameter(&luaPwmInputCh2, &pwmInputChCallback, luaPinFolder2.common.id);
    registerLUAParameter(&luaPwmMode2, &pwmModeCallback, luaPinFolder2.common.id);
    registerLUAParameter(&luaPwmInvert2, &pwmInvertCallback, luaPinFolder2.common.id);
    registerLUAParameter(&luaPwmNarrow2, &pwmNarrowCallback, luaPinFolder2.common.id);
    registerLUAParameter(&luaPwmFailsafeMode2, &pwmFailsafeModeCallback, luaPinFolder2.common.id);
    registerLUAParameter(&luaPwmFailsafeVal2, &pwmFailsafeValCallback, luaPinFolder2.common.id);
    registerLUAParameter(&luaPwmMapMode2, &pwmMapModeCallback, luaPinFolder2.common.id);
    registerLUAParameter(&luaPwmMapValues2, nullptr, luaPinFolder2.common.id);
    registerLUAParameter(&luaPwmRequiresArm2, &pwmRequiresArmCallback, luaPinFolder2.common.id);

    registerLUAParameter(&luaPinFolder3, nullptr, luaMappingFolder.common.id);
    registerLUAParameter(&luaPwmInputCh3, &pwmInputChCallback, luaPinFolder3.common.id);
    registerLUAParameter(&luaPwmMode3, &pwmModeCallback, luaPinFolder3.common.id);
    registerLUAParameter(&luaPwmInvert3, &pwmInvertCallback, luaPinFolder3.common.id);
    registerLUAParameter(&luaPwmNarrow3, &pwmNarrowCallback, luaPinFolder3.common.id);
    registerLUAParameter(&luaPwmFailsafeMode3, &pwmFailsafeModeCallback, luaPinFolder3.common.id);
    registerLUAParameter(&luaPwmFailsafeVal3, &pwmFailsafeValCallback, luaPinFolder3.common.id);
    registerLUAParameter(&luaPwmMapMode3, &pwmMapModeCallback, luaPinFolder3.common.id);
    registerLUAParameter(&luaPwmMapValues3, nullptr, luaPinFolder3.common.id);
    registerLUAParameter(&luaPwmRequiresArm3, &pwmRequiresArmCallback, luaPinFolder3.common.id);
  }
#endif

  registerLUAParameter(&luaBindStorage, [](struct luaPropertiesCommon* item, uint8_t arg) {
    config.SetBindStorage((rx_config_bindstorage_t)arg);
  });
  registerLUAParameter(&luaBindMode, [](struct luaPropertiesCommon* item, uint8_t arg){
    // Complete when TX polls for status i.e. going back to idle, because we're going to lose connection
    if (arg == lcsQuery) {
      deferExecutionMillis(200, EnterBindingModeSafely);
    }
    sendLuaCommandResponse(&luaBindMode, arg < 5 ? lcsExecuting : lcsIdle, arg < 5 ? "Entering..." : "");
  });

  registerLUAParameter(&luaModelNumber);
  registerLUAParameter(&luaELRSversion);
  registerLUAParameter(nullptr);
}

static void updateBindModeLabel()
{
  if (config.IsOnLoan())
    luaBindMode.common.name = "Return Model";
  else
    luaBindMode.common.name = "Enter Bind Mode";
}

static int event()
{
  setLuaTextSelectionValue(&luaSerialProtocol, config.GetSerialProtocol());
#if defined(PLATFORM_ESP32)
  if (RX_HAS_SERIAL1)
  {
    setLuaTextSelectionValue(&luaSerial1Protocol, config.GetSerial1Protocol());
  }
#endif
  
  setLuaTextSelectionValue(&luaSBUSFailsafeMode, config.GetFailsafeMode());

  if (GPIO_PIN_ANT_CTRL != UNDEF_PIN)
  {
    setLuaTextSelectionValue(&luaAntennaMode, config.GetAntennaMode());
  }

  // Gemini Mode
  if (isDualRadio())
  {
    setLuaTextSelectionValue(&luaDiversityMode, config.GetAntennaMode()); // Reusing SetAntennaMode since both GPIO_PIN_ANTENNA_SELECT and GPIO_PIN_NSS_2 will not be defined together.
  }

#if defined(POWER_OUTPUT_VALUES)
  // The last item (for MatchTX) will be MaxPower - MinPower + 1
  uint8_t luaPwrVal = (config.GetPower() == PWR_MATCH_TX) ? POWERMGNT::getMaxPower() + 1 : config.GetPower();
  setLuaTextSelectionValue(&luaTlmPower, luaPwrVal - POWERMGNT::getMinPower());
#endif

  // Teamrace
  setLuaTextSelectionValue(&luaTeamraceChannel, config.GetTeamraceChannel() - AUX2);
  setLuaTextSelectionValue(&luaTeamracePosition, config.GetTeamracePosition());

#if defined(GPIO_PIN_PWM_OUTPUTS)
  if (OPT_HAS_SERVO_OUTPUT)
  {
    // Load current values from config for all pins
    const rx_config_pwm_t *cfg0 = config.GetPwmChannel(0);
    setLuaUint8Value(&luaPwmInputCh0, cfg0->val.inputChannel + 1);
    setLuaTextSelectionValue(&luaPwmMode0, cfg0->val.mode);
    setLuaTextSelectionValue(&luaPwmInvert0, cfg0->val.inverted);
    setLuaTextSelectionValue(&luaPwmNarrow0, cfg0->val.narrow);
    setLuaTextSelectionValue(&luaPwmFailsafeMode0, cfg0->val.failsafeMode);
    setLuaUint16Value(&luaPwmFailsafeVal0, cfg0->val.failsafe + 988);
    setLuaTextSelectionValue(&luaPwmMapMode0, cfg0->val.mapMode);
    formatMapValues(cfg0, luaPwmMapValuesStr0);
    setLuaTextSelectionValue(&luaPwmRequiresArm0, cfg0->val.requiresArm);

    const rx_config_pwm_t *cfg1 = config.GetPwmChannel(1);
    setLuaUint8Value(&luaPwmInputCh1, cfg1->val.inputChannel + 1);
    setLuaTextSelectionValue(&luaPwmMode1, cfg1->val.mode);
    setLuaTextSelectionValue(&luaPwmInvert1, cfg1->val.inverted);
    setLuaTextSelectionValue(&luaPwmNarrow1, cfg1->val.narrow);
    setLuaTextSelectionValue(&luaPwmFailsafeMode1, cfg1->val.failsafeMode);
    setLuaUint16Value(&luaPwmFailsafeVal1, cfg1->val.failsafe + 988);
    setLuaTextSelectionValue(&luaPwmMapMode1, cfg1->val.mapMode);
    formatMapValues(cfg1, luaPwmMapValuesStr1);
    setLuaTextSelectionValue(&luaPwmRequiresArm1, cfg1->val.requiresArm);

    const rx_config_pwm_t *cfg2 = config.GetPwmChannel(2);
    setLuaUint8Value(&luaPwmInputCh2, cfg2->val.inputChannel + 1);
    setLuaTextSelectionValue(&luaPwmMode2, cfg2->val.mode);
    setLuaTextSelectionValue(&luaPwmInvert2, cfg2->val.inverted);
    setLuaTextSelectionValue(&luaPwmNarrow2, cfg2->val.narrow);
    setLuaTextSelectionValue(&luaPwmFailsafeMode2, cfg2->val.failsafeMode);
    setLuaUint16Value(&luaPwmFailsafeVal2, cfg2->val.failsafe + 988);
    setLuaTextSelectionValue(&luaPwmMapMode2, cfg2->val.mapMode);
    formatMapValues(cfg2, luaPwmMapValuesStr2);
    setLuaTextSelectionValue(&luaPwmRequiresArm2, cfg2->val.requiresArm);

    const rx_config_pwm_t *cfg3 = config.GetPwmChannel(3);
    setLuaUint8Value(&luaPwmInputCh3, cfg3->val.inputChannel + 1);
    setLuaTextSelectionValue(&luaPwmMode3, cfg3->val.mode);
    setLuaTextSelectionValue(&luaPwmInvert3, cfg3->val.inverted);
    setLuaTextSelectionValue(&luaPwmNarrow3, cfg3->val.narrow);
    setLuaTextSelectionValue(&luaPwmFailsafeMode3, cfg3->val.failsafeMode);
    setLuaUint16Value(&luaPwmFailsafeVal3, cfg3->val.failsafe + 988);
    setLuaTextSelectionValue(&luaPwmMapMode3, cfg3->val.mapMode);
    formatMapValues(cfg3, luaPwmMapValuesStr3);
    setLuaTextSelectionValue(&luaPwmRequiresArm3, cfg3->val.requiresArm);
  }
#endif

  if (config.GetModelId() == 255)
  {
    setLuaStringValue(&luaModelNumber, "Off");
  }
  else
  {
    itoa(config.GetModelId(), modelString, 10);
    setLuaStringValue(&luaModelNumber, modelString);
  }
  setLuaTextSelectionValue(&luaBindStorage, config.GetBindStorage());
  updateBindModeLabel();

  if (config.GetSerialProtocol() == PROTOCOL_MAVLINK)
  {
    setLuaUint8Value(&luaSourceSysId, config.GetSourceSysId() == 0 ? 255 : config.GetSourceSysId());  //display Source sysID if 0 display 255 to mimic logic in SerialMavlink.cpp
    setLuaUint8Value(&luaTargetSysId, config.GetTargetSysId() == 0 ? 1 : config.GetTargetSysId());  //display Target sysID if 0 display 1 to mimic logic in SerialMavlink.cpp
    LUA_FIELD_SHOW(luaSourceSysId)
    LUA_FIELD_SHOW(luaTargetSysId)
  }
  else
  {
    LUA_FIELD_HIDE(luaSourceSysId)
    LUA_FIELD_HIDE(luaTargetSysId)
  }

  return DURATION_IMMEDIATELY;
}

static int timeout()
{
  DBGVLN("LUA_device timeout() called");
  luaHandleUpdateParameter();
  // Receivers can only `UpdateParamReq == true` every 4th packet due to the transmitter cadence in 1:2
  // Channels, Downlink Telemetry Slot, Uplink Telemetry (the write command), Downlink Telemetry Slot...
  // (interval * 4 / 1000) or 1 second if not connected
  return (connectionState == connected) ? ExpressLRS_currAirRate_Modparams->interval / 250 : 1000;
}

static int start()
{
  registerLuaParameters();
  event();
  return DURATION_IMMEDIATELY;
}

device_t LUA_device = {
  .initialize = nullptr,
  .start = start,
  .event = event,
  .timeout = timeout
};

#endif
