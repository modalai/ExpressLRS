#include "targets.h"
#if !defined(UNIT_TEST)
#include "RXEndpoint.h"
#include "FHSS.h"
#include "POWERMGNT.h"
#include "config.h"
#include "deferred.h"
#include "devServoOutput.h"
#include "helpers.h"
#include "rxtx_intf.h"
#include "logging.h"

#define RX_HAS_SERIAL1 (GPIO_PIN_SERIAL1_TX != UNDEF_PIN || OPT_HAS_SERVO_OUTPUT)

extern void reconfigureSerial();
#if defined(PLATFORM_ESP32)
extern void reconfigureSerial1();
#endif
extern bool BindingModeRequest;

extern RXEndpoint crsfReceiver;

#if defined(Regulatory_Domain_EU_CE_2400)
#if defined(RADIO_LR1121)
char strPowerLevels[] = "10/10;25/25;25/50;25/100;25/250;25/500;25/1000;25/2000;MatchTX ";
#else
char strPowerLevels[] = "10;25;50;100;250;500;1000;2000;MatchTX ";
#endif
#else
char strPowerLevels[] = "10;25;50;100;250;500;1000;2000;MatchTX ";
#endif
static char modelString[] = "000";
static char pwmModes[] = "50Hz;60Hz;100Hz;160Hz;333Hz;400Hz;10kHzDuty;On/Off;DShot;DShot 3D;Serial RX;Serial TX;I2C SCL;I2C SDA;Serial2 RX;Serial2 TX";

static selectionParameter luaSerialProtocol = {
    {"Protocol", CRSF_TEXT_SELECTION},
    0, // value
#if defined(M0139)
    "CRSF;Inverted CRSF;SBUS;Inverted SBUS;SUMD;DJI RS Pro;HoTT Telemetry;MAVLink;DisplayPort;GPS;IBus",
#else
    "CRSF;Inverted CRSF;SBUS;Inverted SBUS;SUMD;DJI RS Pro;HoTT Telemetry;MAVLink;DisplayPort;GPS",
#endif
    STR_EMPTYSPACE
};

#if defined(PLATFORM_ESP32)
static selectionParameter luaSerial1Protocol = {
    {"Protocol2", CRSF_TEXT_SELECTION},
    0, // value
    "Off;CRSF;Inverted CRSF;SBUS;Inverted SBUS;SUMD;DJI RS Pro;HoTT Telemetry;Tramp;SmartAudio;DisplayPort;GPS",
    STR_EMPTYSPACE
};
#endif

static selectionParameter luaSBUSFailsafeMode = {
    {"SBUS failsafe", CRSF_TEXT_SELECTION},
    0, // value
    "No Pulses;Last Pos",
    STR_EMPTYSPACE
};

static int8Parameter luaTargetSysId = {
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
static int8Parameter luaSourceSysId = {
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

static selectionParameter luaTlmPower = {
    {"Tlm Power", CRSF_TEXT_SELECTION},
    0, // value
    strPowerLevels,
    "mW"
};

static selectionParameter luaAntennaMode = {
    {"Ant. Mode", CRSF_TEXT_SELECTION},
    0, // value
    "Antenna A;Antenna B;Diversity",
    STR_EMPTYSPACE
};

static selectionParameter luaAntennaGroup = {
    {"Ant. Group", CRSF_TEXT_SELECTION},
    0, // value
    "External;Builtin",
    STR_EMPTYSPACE
};

static folderParameter luaTeamraceFolder = {
    {"Team Race", CRSF_FOLDER},
};

static selectionParameter luaTeamraceChannel = {
    {"Channel", CRSF_TEXT_SELECTION},
    0, // value
    "AUX2;AUX3;AUX4;AUX5;AUX6;AUX7;AUX8;AUX9;AUX10;AUX11;AUX12",
    STR_EMPTYSPACE
};

static selectionParameter luaTeamracePosition = {
    {"Position", CRSF_TEXT_SELECTION},
    0, // value
    "Disabled;1/Low;2;3;Mid;4;5;6/High",
    STR_EMPTYSPACE
};

//----------------------------Info-----------------------------------

static stringParameter luaModelNumber = {
    {"Model Id", CRSF_INFO},
    modelString
};

static stringParameter luaELRSversion = {
    {version_domain, CRSF_INFO},
    commit
};

//----------------------------Info-----------------------------------

//---------------------------- WiFi -----------------------------


//---------------------------- WiFi -----------------------------

//---------------------------- Output Mapping -----------------------------

static folderParameter luaMappingFolder = {
    {"Output Mapping", CRSF_FOLDER},
};

static int8Parameter luaMappingChannelOut = {
  {"Output Ch", CRSF_UINT8},
  {
    {
      (uint8_t)5,       // value - start on AUX1, value is 1-16, not zero-based
      1,                // min
      PWM_MAX_CHANNELS, // max
    }
  },
  STR_EMPTYSPACE
};

static int8Parameter luaMappingChannelIn = {
  {"Input Ch", CRSF_UINT8},
  {
    {
      0,                 // value
      1,                 // min
      CRSF_NUM_CHANNELS, // max
    }
  },
  STR_EMPTYSPACE
};

static selectionParameter luaMappingOutputMode = {
    {"Output Mode", CRSF_TEXT_SELECTION},
    0, // value
    pwmModes,
    STR_EMPTYSPACE
};

static selectionParameter luaMappingInverted = {
    {"Invert", CRSF_TEXT_SELECTION},
    0, // value
    "Off;On",
    STR_EMPTYSPACE
};

static commandParameter luaSetFailsafe = {
    {"Set Failsafe Pos", CRSF_COMMAND},
    lcsIdle, // step
    STR_EMPTYSPACE
};

#if defined(M0139)
static selectionParameter luaPwmArmed = {
    {"PWM Armed", CRSF_TEXT_SELECTION},
    0,
    "Disarmed;Armed",
    STR_EMPTYSPACE
};

static selectionParameter luaMappingNarrow = {
    {"Narrow Pulse", CRSF_TEXT_SELECTION},
    0,
    "Off;On",
    STR_EMPTYSPACE
};

static selectionParameter luaMappingFailsafeMode = {
    {"Failsafe Mode", CRSF_TEXT_SELECTION},
    0,
    "Set Position;No Pulses;Last Position",
    STR_EMPTYSPACE
};

static int16Parameter luaMappingFailsafeValue = {
    {"Failsafe", CRSF_UINT16},
    {{htobe16(1500), htobe16(US_CHANNEL_VALUE_MIN), htobe16(US_CHANNEL_VALUE_MAX)}},
    "us"
};

static selectionParameter luaMappingMapMode = {
    {"Map Mode", CRSF_TEXT_SELECTION},
    0,
    "Off;Step;Interpolate",
    STR_EMPTYSPACE
};

#define PWM_MAP_PARAMETER(variable, label, maximum, unitsLabel) \
    static int16Parameter variable = { \
        {label, CRSF_UINT16}, \
        {{0, 0, htobe16(maximum)}}, \
        unitsLabel \
    }

PWM_MAP_PARAMETER(luaMappingInput1, "Map Input 1", 2046, "CRSF");
PWM_MAP_PARAMETER(luaMappingInput2, "Map Input 2", 2046, "CRSF");
PWM_MAP_PARAMETER(luaMappingInput3, "Map Input 3", 2046, "CRSF");
PWM_MAP_PARAMETER(luaMappingOutput1, "Map Output 1", 4094, "us");
PWM_MAP_PARAMETER(luaMappingOutput2, "Map Output 2", 4094, "us");
PWM_MAP_PARAMETER(luaMappingOutput3, "Map Output 3", 4094, "us");
#undef PWM_MAP_PARAMETER

static selectionParameter luaMappingRequiresArm = {
    {"Requires Arm", CRSF_TEXT_SELECTION},
    0,
    "Off;On",
    STR_EMPTYSPACE
};

static folderParameter luaUidFolder = {
    {"Receiver UID", CRSF_FOLDER},
};

#define UID_PARAMETER(index) \
    static int8Parameter luaUid##index = { \
        {"Byte " #index, CRSF_UINT8}, \
        {{0, 0, 255}}, \
        STR_EMPTYSPACE \
    }

UID_PARAMETER(0);
UID_PARAMETER(1);
UID_PARAMETER(2);
UID_PARAMETER(3);
UID_PARAMETER(4);
UID_PARAMETER(5);
#undef UID_PARAMETER

static commandParameter luaApplyUid = {
    {"Apply UID", CRSF_COMMAND},
    lcsIdle,
    STR_EMPTYSPACE
};

static commandParameter luaUnbind = {
    {"Unbind", CRSF_COMMAND},
    lcsIdle,
    STR_EMPTYSPACE
};

static uint8_t pendingUid[UID_LEN];
static bool pendingUidDirty;
#endif

#if defined(CUSTOM_DOMAIN_ENABLE)
static folderParameter luaCustomDomainFolder = {
    {"Custom Domain", CRSF_FOLDER},
};

static selectionParameter luaCustomDomainEnable = {
    {"Enable", CRSF_TEXT_SELECTION},
    0,
    "Off;On",
    STR_EMPTYSPACE
};

static int16Parameter luaCustomDomainStart = {
    {"Start MHz", CRSF_UINT16},
    {{htobe16(915), htobe16(410), htobe16(1019)}},
    "MHz"
};

static int16Parameter luaCustomDomainEnd = {
    {"End MHz", CRSF_UINT16},
    {{htobe16(928), htobe16(411), htobe16(1020)}},
    "MHz"
};

static int8Parameter luaCustomDomainChannels = {
    {"Channels", CRSF_UINT8},
    {{20, 2, 255}},
    "ch"
};
#endif

//---------------------------- Output Mapping -----------------------------

static selectionParameter luaBindStorage = {
    {"Bind Storage", CRSF_TEXT_SELECTION},
    0, // value
    "Persistent;Volatile;Returnable;Administered",
    STR_EMPTYSPACE
};

static commandParameter luaBindMode = {
    {STR_EMPTYSPACE, CRSF_COMMAND},
    lcsIdle, // step
    STR_EMPTYSPACE
};

static uint8_t sanitizePwmMode(uint8_t mode)
{
    return OPT_PWM_OUT_ONLY && mode >= somSerial ? som50Hz : mode;
}

#if defined(M0139)
static uint8_t selectedPwmOutput()
{
  return constrain(luaMappingChannelOut.properties.u.value, 1, GPIO_PIN_PWM_OUTPUTS_COUNT) - 1;
}

static rx_config_pwm_t getSelectedPwmConfig()
{
  return *config.GetPwmChannel(selectedPwmOutput());
}

static void saveSelectedPwmConfig(const rx_config_pwm_t &channel)
{
  config.SetPwmChannelRaw(selectedPwmOutput(), channel.raw);
}

static void setSelectedMapValue(propertiesCommon *item, int32_t arg)
{
  rx_config_pwm_t channel = getSelectedPwmConfig();
  const uint16_t storedValue = arg >> 1;
  if (item == &luaMappingInput1.common) channel.val.mapInVal1 = storedValue;
  else if (item == &luaMappingInput2.common) channel.val.mapInVal2 = storedValue;
  else if (item == &luaMappingInput3.common) channel.val.mapInVal3 = storedValue;
  else if (item == &luaMappingOutput1.common) channel.val.mapOutVal1 = storedValue;
  else if (item == &luaMappingOutput2.common) channel.val.mapOutVal2 = storedValue;
  else if (item == &luaMappingOutput3.common) channel.val.mapOutVal3 = storedValue;
  saveSelectedPwmConfig(channel);
}

static void setPendingUidByte(propertiesCommon *item, int32_t arg)
{
  const uint8_t index = item == &luaUid0.common ? 0 :
                        item == &luaUid1.common ? 1 :
                        item == &luaUid2.common ? 2 :
                        item == &luaUid3.common ? 3 :
                        item == &luaUid4.common ? 4 : 5;
  pendingUid[index] = arg;
  pendingUidDirty = true;
}

static void updateModalPwmParameters(const rx_config_pwm_t *channel)
{
  luaMappingNarrow.value = channel->val.narrow;
  luaMappingFailsafeMode.value = channel->val.failsafeMode;
  luaMappingFailsafeValue.properties.u.value = htobe16(channel->val.failsafe + US_CHANNEL_VALUE_MIN);
  luaMappingMapMode.value = channel->val.mapMode;
  luaMappingInput1.properties.u.value = htobe16(channel->val.mapInVal1 << 1);
  luaMappingInput2.properties.u.value = htobe16(channel->val.mapInVal2 << 1);
  luaMappingInput3.properties.u.value = htobe16(channel->val.mapInVal3 << 1);
  luaMappingOutput1.properties.u.value = htobe16(channel->val.mapOutVal1 << 1);
  luaMappingOutput2.properties.u.value = htobe16(channel->val.mapOutVal2 << 1);
  luaMappingOutput3.properties.u.value = htobe16(channel->val.mapOutVal3 << 1);
  luaMappingRequiresArm.value = channel->val.requiresArm;
  luaPwmArmed.value = servoIsArmed();
}
#endif

void RXEndpoint::luaparamMappingChannelOut(propertiesCommon *item, uint8_t arg)
{
    bool sclAssigned = false;
    bool sdaAssigned = false;
#if defined(PLATFORM_ESP32)
    bool serial1rxAssigned = false;
    bool serial1txAssigned = false;
#endif

    const char *no1Option    = ";";
    const char *no2Options   = ";;";
    const char *serial_RX    = ";Serial RX";
    const char *serial_TX    = ";Serial TX";
    const char *i2c_SCL      = ";I2C SCL;";
    const char *i2c_SDA      = ";;I2C SDA";
    const char *i2c_BOTH     = ";I2C SCL;I2C SDA";
#if defined(PLATFORM_ESP32)
    const char *serial1_RX   = ";Serial2 RX;";
    const char *serial1_TX   = ";;Serial2 TX";
    const char *serial1_BOTH = ";Serial2 RX;Serial2 TX";
    const char *dshot        = ";DShot;DShot 3D";
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

    setUint8Value(&luaMappingChannelOut, arg);

    // When the selected output channel changes, update the available PWM modes for that pin
    // Truncate the select options before the ; following On/Off
    pwmModes[50] = '\0';

#if defined(PLATFORM_ESP32)
    // DShot output (2 options)
    // ;DShot;DShot3D
    if (GPIO_PIN_PWM_OUTPUTS[arg-1] != 0)   // DShot doesn't work with GPIO0, exclude it
    {
        pModeString = dshot;
    }
    else
#endif
    {
        pModeString = no2Options;
    }
    strcat(pwmModes, pModeString);

    // SerialIO outputs (1 option)
    // ;[Serial RX] | [Serial TX]
    if (!OPT_PWM_OUT_ONLY && GPIO_PIN_PWM_OUTPUTS[arg-1] == U0RXD_GPIO_NUM)
    {
        pModeString = serial_RX;
    }
    else if (!OPT_PWM_OUT_ONLY && GPIO_PIN_PWM_OUTPUTS[arg-1] == U0TXD_GPIO_NUM)
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
    if (!OPT_PWM_OUT_ONLY && (GPIO_PIN_SCL != UNDEF_PIN || GPIO_PIN_SDA != UNDEF_PIN))
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
    else if (!OPT_PWM_OUT_ONLY)
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
    else
    {
        pModeString = no2Options;
    }
    strcat(pwmModes, pModeString);

    // nothing to do for unsupported somPwm mode
    strcat(pwmModes, no1Option);

#if defined(PLATFORM_ESP32)
    // secondary Serial pins (2 options)
    // ;[SERIAL2 RX] ;[SERIAL2_TX]
    if (!OPT_PWM_OUT_ONLY && (GPIO_PIN_SERIAL1_RX != UNDEF_PIN || GPIO_PIN_SERIAL1_TX != UNDEF_PIN))
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
    else if (!OPT_PWM_OUT_ONLY)
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
    else
    {
        pModeString = no2Options;
    }
    strcat(pwmModes, pModeString);
#endif

    // trim off trailing semicolons (assumes pwmModes has at least 1 non-semicolon)
    for (auto lastPos = strlen(pwmModes)-1; pwmModes[lastPos] == ';'; lastPos--)
    {
        pwmModes[lastPos] = '\0';
    }

    // update the related fields to represent the selected channel
    const rx_config_pwm_t *pwmCh = config.GetPwmChannel(luaMappingChannelOut.properties.u.value - 1);
    setUint8Value(&luaMappingChannelIn, pwmCh->val.inputChannel + 1);
    setTextSelectionValue(&luaMappingOutputMode, sanitizePwmMode(pwmCh->val.mode));
    setTextSelectionValue(&luaMappingInverted, pwmCh->val.inverted);
#if defined(M0139)
    updateModalPwmParameters(pwmCh);
#endif
}

static void luaparamMappingChannelIn(propertiesCommon *item, uint8_t arg)
{
  const uint8_t ch = luaMappingChannelOut.properties.u.value - 1;
  rx_config_pwm_t newPwmCh;
  newPwmCh = *config.GetPwmChannel(ch);
  newPwmCh.val.inputChannel = arg - 1; // convert 1-16 -> 0-15

  config.SetPwmChannelRaw(ch, newPwmCh.raw);
}

static void configureSerialPin(uint8_t sibling, uint8_t oldMode, uint8_t newMode)
{
  for (int ch=0 ; ch<GPIO_PIN_PWM_OUTPUTS_COUNT ; ch++)
  {
    if (GPIO_PIN_PWM_OUTPUTS[ch] == sibling)
    {
      // Retain as much of the sibling's current config as possible
      rx_config_pwm_t siblingPinConfig;
      siblingPinConfig = *config.GetPwmChannel(ch);

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

      config.SetPwmChannelRaw(ch, siblingPinConfig.raw);
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

static void luaparamMappingOutputMode(propertiesCommon *item, uint8_t arg)
{
  UNUSED(item);
  const uint8_t ch = luaMappingChannelOut.properties.u.value - 1;
  rx_config_pwm_t newPwmCh;
  newPwmCh = *config.GetPwmChannel(ch);
  uint8_t oldMode = newPwmCh.val.mode;
  newPwmCh.val.mode = sanitizePwmMode(arg);

  // Check if pin == 1/3 and do other pin adjustment accordingly
  if (GPIO_PIN_PWM_OUTPUTS[ch] == 1)
  {
    configureSerialPin(3, oldMode, newPwmCh.val.mode);
  }
  else if (GPIO_PIN_PWM_OUTPUTS[ch] == 3)
  {
    configureSerialPin(1, oldMode, newPwmCh.val.mode);
  }
  config.SetPwmChannelRaw(ch, newPwmCh.raw);
}

static void luaparamMappingInverted(propertiesCommon *item, uint8_t arg)
{
  UNUSED(item);
  const uint8_t ch = luaMappingChannelOut.properties.u.value - 1;
  rx_config_pwm_t newPwmCh;
  newPwmCh = *config.GetPwmChannel(ch);
  newPwmCh.val.inverted = arg;

  config.SetPwmChannelRaw(ch, newPwmCh.raw);
}

void RXEndpoint::luaparamSetFailsafe(propertiesCommon *item, uint8_t arg)
{
  commandStep_e newStep;
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
    servoCurrentToFailsafeConfig();
  }
  else
  {
    newStep = lcsIdle;
    msg = STR_EMPTYSPACE;
  }

  sendCommandResponse((commandParameter *)item, newStep, msg);
}

static void luaparamSetPower(propertiesCommon* item, uint8_t arg)
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

void RXEndpoint::registerParameters()
{
  registerParameter(&luaSerialProtocol, [](propertiesCommon* item, uint8_t arg){
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
    registerParameter(&luaSerial1Protocol, [](propertiesCommon* item, uint8_t arg){
      config.SetSerial1Protocol((eSerial1Protocol)arg);
      if (config.IsModified()) {
        deferExecutionMillis(100, [](){
          reconfigureSerial1();
        });
      }
    });
  }
#endif

  registerParameter(&luaSBUSFailsafeMode, [](propertiesCommon* item, uint8_t arg){
    config.SetFailsafeMode((eFailsafeMode)arg);
  });

  registerParameter(&luaTargetSysId, [](propertiesCommon* item, uint8_t arg){
    config.SetTargetSysId((uint8_t)arg);
  });
  registerParameter(&luaSourceSysId, [](propertiesCommon* item, uint8_t arg){
    config.SetSourceSysId((uint8_t)arg);
  });

  if (GPIO_PIN_ANT_CTRL != UNDEF_PIN)
  {
    registerParameter(&luaAntennaMode, [](propertiesCommon* item, uint8_t arg){
      config.SetAntennaMode(arg);
    });
  }

  if (GPIO_PIN_ANT_GROUP != UNDEF_PIN)
  {
    registerParameter(&luaAntennaGroup, [](propertiesCommon* item, uint8_t arg){
      config.SetAntennaGroup(arg);
    });
  }

  if (POWERMGNT::getMinPower() != POWERMGNT::getMaxPower())
  {
    filterOptions(&luaTlmPower, POWERMGNT::getMinPower(), POWERMGNT::getMaxPower(), strPowerLevels);
    strcat(strPowerLevels, ";MatchTX ");
    registerParameter(&luaTlmPower, &luaparamSetPower);
  }

  // Teamrace
  registerParameter(&luaTeamraceFolder);
  registerParameter(&luaTeamraceChannel, [](propertiesCommon* item, uint8_t arg) {
    config.SetTeamraceChannel(arg + AUX2);
  }, luaTeamraceFolder.common.id);
  registerParameter(&luaTeamracePosition, [](propertiesCommon* item, uint8_t arg) {
    config.SetTeamracePosition(arg);
  }, luaTeamraceFolder.common.id);

#if defined(CUSTOM_DOMAIN_ENABLE)
  registerParameter(&luaCustomDomainFolder);
  registerParameter(&luaCustomDomainEnable, [](propertiesCommon* item, int32_t arg) {
    config.SetCustomDomainEnabled(arg != 0);
  }, luaCustomDomainFolder.common.id);
  registerParameter(&luaCustomDomainStart, [](propertiesCommon* item, int32_t arg) {
    config.SetCustomDomainStartMHz(arg);
  }, luaCustomDomainFolder.common.id);
  registerParameter(&luaCustomDomainEnd, [](propertiesCommon* item, int32_t arg) {
    config.SetCustomDomainEndMHz(arg);
  }, luaCustomDomainFolder.common.id);
  registerParameter(&luaCustomDomainChannels, [](propertiesCommon* item, int32_t arg) {
    config.SetCustomDomainChannels(arg);
  }, luaCustomDomainFolder.common.id);
#endif

  if (OPT_HAS_SERVO_OUTPUT)
  {
#if defined(M0139)
    registerParameter(&luaPwmArmed, [](propertiesCommon* item, int32_t arg) {
      servoSetArmed(arg != 0);
    });
#endif
    luaparamMappingChannelOut(&luaMappingOutputMode.common, luaMappingChannelOut.properties.u.value);
    registerParameter(&luaMappingFolder);
    registerParameter(&luaMappingChannelOut, [&](propertiesCommon* item, uint8_t arg) {
        luaparamMappingChannelOut(item, arg);
    }, luaMappingFolder.common.id);
    registerParameter(&luaMappingChannelIn, &luaparamMappingChannelIn, luaMappingFolder.common.id);
    registerParameter(&luaMappingOutputMode, &luaparamMappingOutputMode, luaMappingFolder.common.id);
    registerParameter(&luaMappingInverted, &luaparamMappingInverted, luaMappingFolder.common.id);
#if defined(M0139)
    registerParameter(&luaMappingNarrow, [](propertiesCommon* item, int32_t arg) {
      rx_config_pwm_t channel = getSelectedPwmConfig();
      channel.val.narrow = arg;
      saveSelectedPwmConfig(channel);
    }, luaMappingFolder.common.id);
    registerParameter(&luaMappingFailsafeMode, [](propertiesCommon* item, int32_t arg) {
      rx_config_pwm_t channel = getSelectedPwmConfig();
      channel.val.failsafeMode = arg;
      saveSelectedPwmConfig(channel);
    }, luaMappingFolder.common.id);
    registerParameter(&luaMappingFailsafeValue, [](propertiesCommon* item, int32_t arg) {
      rx_config_pwm_t channel = getSelectedPwmConfig();
      channel.val.failsafe = constrain(arg, US_CHANNEL_VALUE_MIN, US_CHANNEL_VALUE_MAX) - US_CHANNEL_VALUE_MIN;
      saveSelectedPwmConfig(channel);
    }, luaMappingFolder.common.id);
    registerParameter(&luaMappingMapMode, [](propertiesCommon* item, int32_t arg) {
      rx_config_pwm_t channel = getSelectedPwmConfig();
      channel.val.mapMode = arg;
      saveSelectedPwmConfig(channel);
    }, luaMappingFolder.common.id);
    registerParameter(&luaMappingInput1, &setSelectedMapValue, luaMappingFolder.common.id);
    registerParameter(&luaMappingInput2, &setSelectedMapValue, luaMappingFolder.common.id);
    registerParameter(&luaMappingInput3, &setSelectedMapValue, luaMappingFolder.common.id);
    registerParameter(&luaMappingOutput1, &setSelectedMapValue, luaMappingFolder.common.id);
    registerParameter(&luaMappingOutput2, &setSelectedMapValue, luaMappingFolder.common.id);
    registerParameter(&luaMappingOutput3, &setSelectedMapValue, luaMappingFolder.common.id);
    registerParameter(&luaMappingRequiresArm, [](propertiesCommon* item, int32_t arg) {
      rx_config_pwm_t channel = getSelectedPwmConfig();
      channel.val.requiresArm = arg;
      saveSelectedPwmConfig(channel);
    }, luaMappingFolder.common.id);
#endif
    registerParameter(&luaSetFailsafe, [&](propertiesCommon* item, uint8_t arg) {
        luaparamSetFailsafe(item, arg);
    });
  }

#if defined(M0139)
  memcpy(pendingUid, config.GetUID(), UID_LEN);
  registerParameter(&luaUidFolder);
  registerParameter(&luaUid0, &setPendingUidByte, luaUidFolder.common.id);
  registerParameter(&luaUid1, &setPendingUidByte, luaUidFolder.common.id);
  registerParameter(&luaUid2, &setPendingUidByte, luaUidFolder.common.id);
  registerParameter(&luaUid3, &setPendingUidByte, luaUidFolder.common.id);
  registerParameter(&luaUid4, &setPendingUidByte, luaUidFolder.common.id);
  registerParameter(&luaUid5, &setPendingUidByte, luaUidFolder.common.id);
  registerParameter(&luaApplyUid, [this](propertiesCommon* item, int32_t arg) {
    if (arg == lcsClick)
    {
      sendCommandResponse(&luaApplyUid, lcsAskConfirm, "Apply receiver UID?");
    }
    else if (arg == lcsConfirmed)
    {
      sendCommandResponse(&luaApplyUid, lcsExecuting, "Applying...");
      deferExecutionMillis(200, []() {
        pendingUidDirty = false;
        UpdateUID(pendingUid);
      });
    }
    else
    {
      sendCommandResponse(&luaApplyUid, lcsIdle, STR_EMPTYSPACE);
    }
  }, luaUidFolder.common.id);
  registerParameter(&luaUnbind, [this](propertiesCommon* item, int32_t arg) {
    if (arg == lcsClick)
    {
      sendCommandResponse(&luaUnbind, lcsAskConfirm, "Unbind receiver?");
    }
    else if (arg == lcsConfirmed)
    {
      sendCommandResponse(&luaUnbind, lcsExecuting, "Unbinding...");
      deferExecutionMillis(200, EnterUnbindMode);
    }
    else
    {
      sendCommandResponse(&luaUnbind, lcsIdle, STR_EMPTYSPACE);
    }
  }, luaUidFolder.common.id);
#endif

  registerParameter(&luaBindStorage, [](propertiesCommon* item, uint8_t arg) {
    config.SetBindStorage((rx_config_bindstorage_t)arg);
  });
  registerParameter(&luaBindMode, [this](propertiesCommon* item, uint8_t arg){
    if (arg == lcsClick)
    {
      sendCommandResponse(&luaBindMode, lcsExecuting, "Entering...");
      deferExecutionMillis(200, EnterBindingModeSafely);
    }
    else
    {
      sendCommandResponse(&luaBindMode, lcsIdle, STR_EMPTYSPACE);
    }
  });

  registerParameter(&luaModelNumber);
  registerParameter(&luaELRSversion);
}

static void updateBindModeLabel()
{
  if (config.IsOnLoan())
    luaBindMode.common.name = "Return Model";
  else
    luaBindMode.common.name = "Enter Bind Mode";
}

void RXEndpoint::updateParameters()
{
  setTextSelectionValue(&luaSerialProtocol, config.GetSerialProtocol());
#if defined(PLATFORM_ESP32)
  if (RX_HAS_SERIAL1)
  {
    setTextSelectionValue(&luaSerial1Protocol, config.GetSerial1Protocol());
  }
#endif

  setTextSelectionValue(&luaSBUSFailsafeMode, config.GetFailsafeMode());

  if (GPIO_PIN_ANT_CTRL != UNDEF_PIN)
  {
    setTextSelectionValue(&luaAntennaMode, config.GetAntennaMode());
  }

  if (GPIO_PIN_ANT_GROUP != UNDEF_PIN)
  {
    setTextSelectionValue(&luaAntennaGroup, config.GetAntennaGroup());
  }

  if (MinPower != MaxPower)
  {
    // The last item (for MatchTX) will be MaxPower - MinPower + 1
    uint8_t luaPwrVal = (config.GetPower() == PWR_MATCH_TX) ? POWERMGNT::getMaxPower() + 1 : config.GetPower();
    setTextSelectionValue(&luaTlmPower, luaPwrVal - POWERMGNT::getMinPower());
  }

  // Teamrace
  setTextSelectionValue(&luaTeamraceChannel, config.GetTeamraceChannel() - AUX2);
  setTextSelectionValue(&luaTeamracePosition, config.GetTeamracePosition());

#if defined(CUSTOM_DOMAIN_ENABLE)
  setTextSelectionValue(&luaCustomDomainEnable, config.GetCustomDomainEnabled());
  setUint16Value(&luaCustomDomainStart, config.GetCustomDomainStartMHz());
  setUint16Value(&luaCustomDomainEnd, config.GetCustomDomainEndMHz());
  setUint8Value(&luaCustomDomainChannels, config.GetCustomDomainChannels());
#endif

  if (OPT_HAS_SERVO_OUTPUT)
  {
    const rx_config_pwm_t *pwmCh = config.GetPwmChannel(luaMappingChannelOut.properties.u.value - 1);
    setUint8Value(&luaMappingChannelIn, pwmCh->val.inputChannel + 1);
    setTextSelectionValue(&luaMappingOutputMode, sanitizePwmMode(pwmCh->val.mode));
    setTextSelectionValue(&luaMappingInverted, pwmCh->val.inverted);
#if defined(M0139)
    updateModalPwmParameters(pwmCh);
#endif
  }

#if defined(M0139)
  if (!pendingUidDirty)
  {
    memcpy(pendingUid, config.GetUID(), UID_LEN);
  }
  setUint8Value(&luaUid0, pendingUid[0]);
  setUint8Value(&luaUid1, pendingUid[1]);
  setUint8Value(&luaUid2, pendingUid[2]);
  setUint8Value(&luaUid3, pendingUid[3]);
  setUint8Value(&luaUid4, pendingUid[4]);
  setUint8Value(&luaUid5, pendingUid[5]);
#endif

  if (config.GetModelId() == 255)
  {
    setStringValue(&luaModelNumber, "Off");
  }
  else
  {
    itoa(config.GetModelId(), modelString, 10);
    setStringValue(&luaModelNumber, modelString);
  }
  setTextSelectionValue(&luaBindStorage, config.GetBindStorage());
  updateBindModeLabel();

  if (config.GetSerialProtocol() == PROTOCOL_MAVLINK)
  {
    setUint8Value(&luaSourceSysId, config.GetSourceSysId() == 0 ? 255 : config.GetSourceSysId());  //display Source sysID if 0 display 255 to mimic logic in SerialMavlink.cpp
    setUint8Value(&luaTargetSysId, config.GetTargetSysId() == 0 ? 1 : config.GetTargetSysId());  //display Target sysID if 0 display 1 to mimic logic in SerialMavlink.cpp
    LUA_FIELD_SHOW(luaSourceSysId)
    LUA_FIELD_SHOW(luaTargetSysId)
  }
  else
  {
    LUA_FIELD_HIDE(luaSourceSysId)
    LUA_FIELD_HIDE(luaTargetSysId)
  }
}
#endif
