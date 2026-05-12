#include "config.h"
#include "config_legacy.h"
#include "common.h"
#include "POWERMGNT.h"
#include "OTA.h"
#include "helpers.h"
#include "logging.h"

#if defined(TARGET_TX)

#define MODEL_CHANGED       bit(1)
#define VTX_CHANGED         bit(2)
#define MAIN_CHANGED        bit(3) // catch-all for global config item
#define FAN_CHANGED         bit(4)
#define MOTION_CHANGED      bit(5)
#define BUTTON_CHANGED      bit(6)
#define UID_CHANGED         bit(7)
#define ALL_CHANGED         (MODEL_CHANGED | VTX_CHANGED | MAIN_CHANGED | FAN_CHANGED | MOTION_CHANGED | BUTTON_CHANGED | UID_CHANGED)

// Really awful but safe(?) type punning of model_config_t/v6_model_config_t to and from uint32_t
template<class T> static const void U32_to_Model(uint32_t const u32, T * const model)
{
    union {
        union {
            T model;
            uint8_t padding[sizeof(uint32_t)-sizeof(T)];
        } val;
        uint32_t u32;
    } converter = { .u32 = u32 };

    *model = converter.val.model;
}

template<class T> static const uint32_t Model_to_U32(T const * const model)
{
    // clear the entire union because the assignment will only fill sizeof(T)
    union {
        union {
            T model;
            uint8_t padding[sizeof(uint32_t)-sizeof(T)];
        } val;
        uint32_t u32;
    } converter = { 0 };

    converter.val.model = *model;
    return converter.u32;
}

static uint8_t RateV6toV7(uint8_t rateV6)
{
#if defined(RADIO_SX127X) || defined(RADIO_LR1121)
    if (rateV6 == 0)
    {
        // 200Hz stays same
        return 0;
    }

    // 100Hz, 50Hz, 25Hz all move up one
    // to make room for 100Hz Full
    return rateV6 + 1;
#else // RADIO_2400
    switch (rateV6)
    {
        case 0: return 4; // 500Hz
        case 1: return 6; // 250Hz
        case 2: return 7; // 150Hz
        case 3: return 9; // 50Hz
        default: return 4; // 500Hz
    }
#endif // RADIO_2400
}

static uint8_t RatioV6toV7(uint8_t ratioV6)
{
    // All shifted up for Std telem
    return ratioV6 + 1;
}

static uint8_t SwitchesV6toV7(uint8_t switchesV6)
{
    // 0 was removed, Wide(2) became 0, Hybrid(1) became 1
    switch (switchesV6)
    {
        case 1: return (uint8_t)smHybridOr16ch;
        case 2:
        default:
            return (uint8_t)smWideOr8ch;
    }
}

static void ModelV6toV7(v6_model_config_t const * const v6, model_config_t * const v7)
{
    v7->rate = RateV6toV7(v6->rate);
    v7->tlm = RatioV6toV7(v6->tlm);
    v7->power = v6->power;
    v7->switchMode = SwitchesV6toV7(v6->switchMode);
    v7->modelMatch = v6->modelMatch;
    v7->dynamicPower = v6->dynamicPower;
    v7->boostChannel = v6->boostChannel;
}

#if defined(CUSTOM_DOMAIN_ENABLE)
namespace {
constexpr uint8_t CUSTOM_DOMAIN_HIGH_BAND = 0;
constexpr uint8_t CUSTOM_DOMAIN_LOW_BAND = 1;
constexpr uint16_t CUSTOM_DOMAIN_HIGH_BAND_START_MHZ = 862;
constexpr uint16_t CUSTOM_DOMAIN_HIGH_BAND_STOP_MHZ = 1020;
constexpr uint16_t CUSTOM_DOMAIN_LOW_BAND_START_MHZ = 410;
constexpr uint16_t CUSTOM_DOMAIN_LOW_BAND_STOP_MHZ = 525;
constexpr uint8_t CUSTOM_DOMAIN_MIN_CHANNELS = 2;
constexpr uint8_t CUSTOM_DOMAIN_DEFAULT_CHANNELS = 20;

static uint16_t clampCustomDomainMHz(uint16_t value, uint16_t minValue, uint16_t maxValue)
{
    if (value < minValue)
    {
        return minValue;
    }
    if (value > maxValue)
    {
        return maxValue;
    }
    return value;
}

static uint8_t clampCustomDomainChannels(uint8_t channels)
{
    return channels < CUSTOM_DOMAIN_MIN_CHANNELS ? CUSTOM_DOMAIN_DEFAULT_CHANNELS : channels;
}

static uint8_t customDomainBandFromMHz(uint16_t freqMHz)
{
    return freqMHz < CUSTOM_DOMAIN_HIGH_BAND_START_MHZ ? CUSTOM_DOMAIN_LOW_BAND : CUSTOM_DOMAIN_HIGH_BAND;
}

static uint16_t customDomainBandStartMHz(uint8_t band)
{
    return band == CUSTOM_DOMAIN_LOW_BAND ? CUSTOM_DOMAIN_LOW_BAND_START_MHZ : CUSTOM_DOMAIN_HIGH_BAND_START_MHZ;
}

static uint16_t customDomainBandStopMHz(uint8_t band)
{
    return band == CUSTOM_DOMAIN_LOW_BAND ? CUSTOM_DOMAIN_LOW_BAND_STOP_MHZ : CUSTOM_DOMAIN_HIGH_BAND_STOP_MHZ;
}

template <typename TConfig>
static void normalizeCustomDomain(TConfig &cfg)
{
    cfg.custom_domain_band = cfg.custom_domain_band == CUSTOM_DOMAIN_LOW_BAND ? CUSTOM_DOMAIN_LOW_BAND : CUSTOM_DOMAIN_HIGH_BAND;

    const uint16_t bandStartMHz = customDomainBandStartMHz(cfg.custom_domain_band);
    const uint16_t bandStopMHz = customDomainBandStopMHz(cfg.custom_domain_band);
    uint16_t startMHz = bandStartMHz + cfg.custom_domain_start;
    uint16_t endMHz = bandStartMHz + cfg.custom_domain_end;

    cfg.custom_domain_n_channels = clampCustomDomainChannels(cfg.custom_domain_n_channels);
    startMHz = clampCustomDomainMHz(startMHz, bandStartMHz, bandStopMHz - 1);
    endMHz = clampCustomDomainMHz(endMHz, bandStartMHz + 1, bandStopMHz);

    if (endMHz <= startMHz)
    {
        if (startMHz >= bandStopMHz)
        {
            startMHz = bandStopMHz - 1;
        }
        endMHz = startMHz + 1;
    }

    cfg.custom_domain_start = startMHz - bandStartMHz;
    cfg.custom_domain_end = endMHz - bandStartMHz;
}

template <typename TConfig>
static uint16_t customDomainStartMHz(TConfig cfg)
{
    normalizeCustomDomain(cfg);
    return customDomainBandStartMHz(cfg.custom_domain_band) + cfg.custom_domain_start;
}

template <typename TConfig>
static uint16_t customDomainEndMHz(TConfig cfg)
{
    normalizeCustomDomain(cfg);
    return customDomainBandStartMHz(cfg.custom_domain_band) + cfg.custom_domain_end;
}

template <typename TConfig>
static uint8_t customDomainChannels(TConfig cfg)
{
    normalizeCustomDomain(cfg);
    return cfg.custom_domain_n_channels;
}

template <typename TConfig>
static fhss_config_t buildCustomDomain(TConfig cfg)
{
    normalizeCustomDomain(cfg);

    const uint16_t startMHz = customDomainBandStartMHz(cfg.custom_domain_band) + cfg.custom_domain_start;
    const uint16_t endMHz = customDomainBandStartMHz(cfg.custom_domain_band) + cfg.custom_domain_end;
    const uint32_t centerHz = ((uint32_t)startMHz + (uint32_t)endMHz) * 500000U;

    return {
        "CUSTOM",
        FREQ_HZ_TO_REG_VAL((uint32_t)startMHz * 1000000U),
        FREQ_HZ_TO_REG_VAL((uint32_t)endMHz * 1000000U),
        cfg.custom_domain_n_channels,
        centerHz
    };
}

#if defined(RADIO_LR1121)
static uint16_t customDomainRegToMHz(uint32_t regFreq)
{
    return regFreq / 1000000U;
}
#else
static uint16_t customDomainRegToMHz(uint32_t regFreq)
{
    return (uint16_t)(((double)regFreq * FREQ_STEP / 1000000.0) + 0.5);
}
#endif

template <typename TConfig>
static void setCustomDomainFromConfig(TConfig &cfg, const fhss_config_t &domain)
{
    const uint16_t startMHz = customDomainRegToMHz(domain.freq_start);
    const uint16_t endMHz = customDomainRegToMHz(domain.freq_stop);
    const uint8_t band = customDomainBandFromMHz(startMHz);
    const uint16_t bandStartMHz = customDomainBandStartMHz(band);

    cfg.custom_domain_band = band;
    cfg.custom_domain_start = startMHz - bandStartMHz;
    cfg.custom_domain_end = endMHz - bandStartMHz;
    cfg.custom_domain_n_channels = domain.freq_count;
    cfg.custom_domain_enable = false;
    normalizeCustomDomain(cfg);
}

template <typename TConfig>
static void setCustomDomainStartMHz(TConfig &cfg, uint16_t startMHz)
{
    const uint8_t band = customDomainBandFromMHz(startMHz);
    const uint16_t bandStartMHz = customDomainBandStartMHz(band);
    const uint16_t bandStopMHz = customDomainBandStopMHz(band);

    startMHz = clampCustomDomainMHz(startMHz, bandStartMHz, bandStopMHz - 1);

    uint16_t endMHz = customDomainEndMHz(cfg);
    if (customDomainBandFromMHz(endMHz) != band)
    {
        endMHz = bandStopMHz;
    }
    endMHz = clampCustomDomainMHz(endMHz, startMHz + 1, bandStopMHz);

    cfg.custom_domain_band = band;
    cfg.custom_domain_start = startMHz - bandStartMHz;
    cfg.custom_domain_end = endMHz - bandStartMHz;
    normalizeCustomDomain(cfg);
}

template <typename TConfig>
static void setCustomDomainEndMHz(TConfig &cfg, uint16_t endMHz)
{
    const uint8_t band = customDomainBandFromMHz(endMHz);
    const uint16_t bandStartMHz = customDomainBandStartMHz(band);
    const uint16_t bandStopMHz = customDomainBandStopMHz(band);

    endMHz = clampCustomDomainMHz(endMHz, bandStartMHz + 1, bandStopMHz);

    uint16_t startMHz = customDomainStartMHz(cfg);
    if (customDomainBandFromMHz(startMHz) != band)
    {
        startMHz = bandStartMHz;
    }
    startMHz = clampCustomDomainMHz(startMHz, bandStartMHz, endMHz - 1);

    cfg.custom_domain_band = band;
    cfg.custom_domain_start = startMHz - bandStartMHz;
    cfg.custom_domain_end = endMHz - bandStartMHz;
    normalizeCustomDomain(cfg);
}

template <typename TConfig>
static void setCustomDomainChannels(TConfig &cfg, uint8_t channels)
{
    cfg.custom_domain_n_channels = clampCustomDomainChannels(channels);
    normalizeCustomDomain(cfg);
}

template <typename TConfig>
static void setCustomDomainEnabled(TConfig &cfg, bool enable)
{
    if (enable)
    {
        normalizeCustomDomain(cfg);
    }
    cfg.custom_domain_enable = enable;
}
} // namespace

bool FHSSuseConfiguredCustomDomain()
{
    return config.GetCustomDomainEnabled();
}

fhss_config_t FHSSgetConfiguredCustomDomain()
{
    return config.GetCustomDomain();
}
#endif

TxConfig::TxConfig() :
    m_model(m_config.model_config)
{
}

#if defined(PLATFORM_ESP32)
void TxConfig::Load()
{
    m_modified = 0;

    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    ESP_ERROR_CHECK(nvs_open("ELRS", NVS_READWRITE, &handle));

    // Try to load the version and make sure it is a TX config
    uint32_t version = 0;
    if (nvs_get_u32(handle, "tx_version", &version) == ESP_OK && ((version & CONFIG_MAGIC_MASK) == TX_CONFIG_MAGIC))
        version = version & ~CONFIG_MAGIC_MASK;
    DBGLN("Config version %u", version);

    // Can't upgrade from version <5, or when flashing a previous version, just use defaults.
    if (version < 5 || version > TX_CONFIG_VERSION)
    {
        SetDefaults(true);
        return;
    }

    SetDefaults(false);

    uint32_t value;
    uint8_t value8;
    // vtx (v5)
    if (nvs_get_u32(handle, "vtx", &value) == ESP_OK)
    {
        m_config.vtxBand = value >> 24;
        m_config.vtxChannel = value >> 16;
        m_config.vtxPower = value >> 8;
        m_config.vtxPitmode = value;
    }

    // fanthresh (v5)
    if (nvs_get_u8(handle, "fanthresh", &value8) == ESP_OK)
        m_config.powerFanThreshold = value8;

    // Both of these were added to config v5 without incrementing the version
    if (nvs_get_u32(handle, "fan", &value) == ESP_OK)
        m_config.fanMode = value;
    if (nvs_get_u32(handle, "motion", &value) == ESP_OK)
        m_config.motionMode = value;

    if (version >= 6)
    {
        // dvr (v6)
        if (nvs_get_u8(handle, "dvraux", &value8) == ESP_OK)
            m_config.dvrAux = value8;
        if (nvs_get_u8(handle, "dvrstartdelay", &value8) == ESP_OK)
            m_config.dvrStartDelay = value8;
        if (nvs_get_u8(handle, "dvrstopdelay", &value8) == ESP_OK)
            m_config.dvrStopDelay = value8;
    }
    else
    {
        // Need to write the dvr defaults
        m_modified |= MAIN_CHANGED;
    }

    if (version >= 7) {
        // load button actions
        if (nvs_get_u32(handle, "button1", &value) == ESP_OK)
            m_config.buttonColors[0].raw = value;
        if (nvs_get_u32(handle, "button2", &value) == ESP_OK)
            m_config.buttonColors[1].raw = value;
        // backpackdisable was actually added after 7, but if not found will default to 0 (enabled)
        if (nvs_get_u8(handle, "backpackdisable", &value8) == ESP_OK)
            m_config.backpackDisable = value8;
        if (nvs_get_u8(handle, "backpacktlmen", &value8) == ESP_OK)
            m_config.backpackTlmMode = value8;
    }

    if (version >= 8) {
        size_t uid_len = UID_LEN;
        nvs_get_blob(handle, "uid", m_config.uid, &uid_len);
    }

#if defined(CUSTOM_DOMAIN_ENABLE)
    if (version >= 9)
    {
        if (nvs_get_u8(handle, "cdstart", &value8) == ESP_OK)
            m_config.custom_domain_start = value8;
        if (nvs_get_u8(handle, "cdend", &value8) == ESP_OK)
            m_config.custom_domain_end = value8;
        if (nvs_get_u8(handle, "cdnchan", &value8) == ESP_OK)
            m_config.custom_domain_n_channels = value8;
        if (nvs_get_u8(handle, "cdband", &value8) == ESP_OK)
            m_config.custom_domain_band = value8;
        if (nvs_get_u8(handle, "cden", &value8) == ESP_OK)
            m_config.custom_domain_enable = value8;
    }
    else
    {
        m_modified |= MAIN_CHANGED;
    }
#endif

    for(unsigned i=0; i<CONFIG_TX_MODEL_CNT; i++)
    {
        char model[10] = "model";
        itoa(i, model+5, 10);
        if (nvs_get_u32(handle, model, &value) == ESP_OK)
        {
            if (version >= 7)
            {
                U32_to_Model(value, &m_config.model_config[i]);
            }
            else
            {
                // Upgrade v6 to v7 directly writing to nvs instead of calling Commit() over and over
                v6_model_config_t v6model;
                U32_to_Model(value, &v6model);
                model_config_t * const newModel = &m_config.model_config[i];
                ModelV6toV7(&v6model, newModel);
                nvs_set_u32(handle, model, Model_to_U32(newModel));
            }
        }
    } // for each model

    if (version != TX_CONFIG_VERSION)
    {
        Commit();
    }
}
#else  // ESP8266
void TxConfig::Load()
{
    m_modified = 0;
    m_eeprom->Get(0, m_config);

    uint32_t version = 0;
    if ((m_config.version & CONFIG_MAGIC_MASK) == TX_CONFIG_MAGIC)
        version = m_config.version & ~CONFIG_MAGIC_MASK;
    DBGLN("Config version %u", version);

    // If version is current, all done
    if (version == TX_CONFIG_VERSION)
        return;

    // Can't upgrade from version <5, or when flashing a previous version, just use defaults.
    if (version < 5 || version > TX_CONFIG_VERSION)
    {
        SetDefaults(true);
        return;
    }

    // Upgrade EEPROM, starting with defaults
    SetDefaults(false);

    if (version == 5)
    {
        UpgradeEepromV5ToV6();
        version = 6;
    }

    if (version == 6)
    {
        UpgradeEepromV6ToV7();
        version = 7;
    }

    if (version == 7)
    {
        UpgradeEepromV7ToV8();
        version = 8;
    }

    if (version == 8)
    {
        UpgradeEepromV8ToV9();
        version = 9;
    }

    if (version == 9)
    {
        // V9→V10: CONFIG_TX_MODEL_CNT changed on M0139, invalidating the raw struct layout.
        // No migration — reset all params to defaults.
        SetDefaults(true);
        return;
    }
}

void TxConfig::UpgradeEepromV5ToV6()
{
    v5_tx_config_t v5Config;
    v6_tx_config_t v6Config = { 0 }; // default the new fields to 0

    // Populate the prev version struct from eeprom
    m_eeprom->Get(0, v5Config);

    // Copy prev values to current config struct
    // This only workse because v5 and v6 are the same up to the new fields
    // which have already been set to 0
    memcpy(&v6Config, &v5Config, sizeof(v5Config));
    v6Config.version = 6U | TX_CONFIG_MAGIC;
    m_eeprom->Put(0, v6Config);
    m_eeprom->Commit();
}

void TxConfig::UpgradeEepromV6ToV7()
{
    v6_tx_config_t v6Config;

    // Populate the prev version struct from eeprom
    m_eeprom->Get(0, v6Config);

    // Manual field copying as some fields have moved
    #define LAZY(member) m_config.member = v6Config.member
    LAZY(vtxBand);
    LAZY(vtxChannel);
    LAZY(vtxPower);
    LAZY(vtxPitmode);
    LAZY(powerFanThreshold);
    LAZY(fanMode);
    LAZY(motionMode);
    LAZY(dvrAux);
    LAZY(dvrStartDelay);
    LAZY(dvrStopDelay);
    #undef LAZY

    for (unsigned i=0; i<CONFIG_TX_MODEL_CNT; i++)
    {
        ModelV6toV7(&v6Config.model_config[i], &m_config.model_config[i]);
    }

    m_modified = ALL_CHANGED;

    // Full Commit now
    m_config.version = 7U | TX_CONFIG_MAGIC;
    Commit();
}

void TxConfig::UpgradeEepromV7ToV8()
{
    // Zero the uid field — setupBindingFromConfig() will use hardware-derived UID
    memset(m_config.uid, 0, UID_LEN);
    m_config.version = 8U | TX_CONFIG_MAGIC;
    m_modified = ALL_CHANGED;
    Commit();
}

void TxConfig::UpgradeEepromV8ToV9()
{
    v8_tx_config_t v8Config;
    m_eeprom->Get(0, v8Config);

    SetDefaults(false);
    memcpy(&m_config, &v8Config, sizeof(v8Config));
    m_config.version = 9U | TX_CONFIG_MAGIC;
    m_modified = ALL_CHANGED;
    Commit();
}
#endif

void
TxConfig::Commit()
{
    if (!m_modified)
    {
        // No changes
        return;
    }
#if defined(PLATFORM_ESP32)
    // Write parts to NVS
    if (m_modified & MODEL_CHANGED)
    {
        uint32_t value = Model_to_U32(m_model);
        char model[10] = "model";
        itoa(m_modelId, model+5, 10);
        nvs_set_u32(handle, model, value);
    }
    if (m_modified & VTX_CHANGED)
    {
        uint32_t value =
            m_config.vtxBand << 24 |
            m_config.vtxChannel << 16 |
            m_config.vtxPower << 8 |
            m_config.vtxPitmode;
        nvs_set_u32(handle, "vtx", value);
    }
    if (m_modified & FAN_CHANGED)
    {
        uint32_t value = m_config.fanMode;
        nvs_set_u32(handle, "fan", value);
    }
    if (m_modified & MOTION_CHANGED)
    {
        uint32_t value = m_config.motionMode;
        nvs_set_u32(handle, "motion", value);
    }
    if (m_modified & MAIN_CHANGED)
    {
        nvs_set_u8(handle, "fanthresh", m_config.powerFanThreshold);

        nvs_set_u8(handle, "backpackdisable", m_config.backpackDisable);
        nvs_set_u8(handle, "backpacktlmen", m_config.backpackTlmMode);
        nvs_set_u8(handle, "dvraux", m_config.dvrAux);
        nvs_set_u8(handle, "dvrstartdelay", m_config.dvrStartDelay);
        nvs_set_u8(handle, "dvrstopdelay", m_config.dvrStopDelay);
#if defined(CUSTOM_DOMAIN_ENABLE)
        nvs_set_u8(handle, "cdstart", m_config.custom_domain_start);
        nvs_set_u8(handle, "cdend", m_config.custom_domain_end);
        nvs_set_u8(handle, "cdnchan", m_config.custom_domain_n_channels);
        nvs_set_u8(handle, "cdband", m_config.custom_domain_band);
        nvs_set_u8(handle, "cden", m_config.custom_domain_enable);
#endif
    }
    if (m_modified & BUTTON_CHANGED)
    {
        nvs_set_u32(handle, "button1", m_config.buttonColors[0].raw);
        nvs_set_u32(handle, "button2", m_config.buttonColors[1].raw);
    }
    if (m_modified & UID_CHANGED)
    {
        nvs_set_blob(handle, "uid", m_config.uid, UID_LEN);
    }
    nvs_set_u32(handle, "tx_version", m_config.version);
    nvs_commit(handle);
#else
    // Write the struct to eeprom
    m_eeprom->Put(0, m_config);
    m_eeprom->Commit();
#endif
    m_modified = 0;
}

// Setters
void
TxConfig::SetRate(uint8_t rate)
{
    if (GetRate() != rate)
    {
        m_model->rate = rate;
        m_modified |= MODEL_CHANGED;
    }
}

void
TxConfig::SetTlm(uint8_t tlm)
{
    if (GetTlm() != tlm)
    {
        m_model->tlm = tlm;
        m_modified |= MODEL_CHANGED;
    }
}

void
TxConfig::SetPower(uint8_t power)
{
    if (GetPower() != power)
    {
        m_model->power = power;
        m_modified |= MODEL_CHANGED;
    }
}

void
TxConfig::SetDynamicPower(bool dynamicPower)
{
    if (GetDynamicPower() != dynamicPower)
    {
        m_model->dynamicPower = dynamicPower;
        m_modified |= MODEL_CHANGED;
    }
}

void
TxConfig::SetBoostChannel(uint8_t boostChannel)
{
    if (GetBoostChannel() != boostChannel)
    {
        m_model->boostChannel = boostChannel;
        m_modified |= MODEL_CHANGED;
    }
}

void
TxConfig::SetSwitchMode(uint8_t switchMode)
{
    if (GetSwitchMode() != switchMode)
    {
        m_model->switchMode = switchMode;
        m_modified |= MODEL_CHANGED;
    }
}

void
TxConfig::SetAntennaMode(uint8_t txAntenna)
{
    if (GetAntennaMode() != txAntenna)
    {
        m_model->txAntenna = txAntenna;
        m_modified |= MODEL_CHANGED;
    }
}

void
TxConfig::SetLinkMode(uint8_t linkMode)
{
    if (GetLinkMode() != linkMode)
    {
        m_model->linkMode = linkMode;

        if (linkMode == TX_MAVLINK_MODE)
        {
            m_model->tlm = TLM_RATIO_1_2;
            m_model->switchMode = smHybridOr16ch; // Force Hybrid / 16ch/2 switch modes for mavlink
        }
        m_modified |= MODEL_CHANGED | MAIN_CHANGED;
    }
}

void
TxConfig::SetModelMatch(bool modelMatch)
{
    if (GetModelMatch() != modelMatch)
    {
        m_model->modelMatch = modelMatch;
        m_modified |= MODEL_CHANGED;
    }
}

void
TxConfig::SetVtxBand(uint8_t vtxBand)
{
    if (m_config.vtxBand != vtxBand)
    {
        m_config.vtxBand = vtxBand;
        m_modified |= VTX_CHANGED;
    }
}

void
TxConfig::SetVtxChannel(uint8_t vtxChannel)
{
    if (m_config.vtxChannel != vtxChannel)
    {
        m_config.vtxChannel = vtxChannel;
        m_modified |= VTX_CHANGED;
    }
}

void
TxConfig::SetVtxPower(uint8_t vtxPower)
{
    if (m_config.vtxPower != vtxPower)
    {
        m_config.vtxPower = vtxPower;
        m_modified |= VTX_CHANGED;
    }
}

void
TxConfig::SetVtxPitmode(uint8_t vtxPitmode)
{
    if (m_config.vtxPitmode != vtxPitmode)
    {
        m_config.vtxPitmode = vtxPitmode;
        m_modified |= VTX_CHANGED;
    }
}

void
TxConfig::SetPowerFanThreshold(uint8_t powerFanThreshold)
{
    if (m_config.powerFanThreshold != powerFanThreshold)
    {
        m_config.powerFanThreshold = powerFanThreshold;
        m_modified |= MAIN_CHANGED;
    }
}

void
TxConfig::SetStorageProvider(ELRS_EEPROM *eeprom)
{
    if (eeprom)
    {
        m_eeprom = eeprom;
    }
}

void
TxConfig::SetFanMode(uint8_t fanMode)
{
    if (m_config.fanMode != fanMode)
    {
        m_config.fanMode = fanMode;
        m_modified |= FAN_CHANGED;
    }
}

void
TxConfig::SetMotionMode(uint8_t motionMode)
{
    if (m_config.motionMode != motionMode)
    {
        m_config.motionMode = motionMode;
        m_modified |= MOTION_CHANGED;
    }
}

void
TxConfig::SetDvrAux(uint8_t dvrAux)
{
    if (GetDvrAux() != dvrAux)
    {
        m_config.dvrAux = dvrAux;
        m_modified |= MAIN_CHANGED;
    }
}

void
TxConfig::SetDvrStartDelay(uint8_t dvrStartDelay)
{
    if (GetDvrStartDelay() != dvrStartDelay)
    {
        m_config.dvrStartDelay = dvrStartDelay;
        m_modified |= MAIN_CHANGED;
    }
}

void
TxConfig::SetDvrStopDelay(uint8_t dvrStopDelay)
{
    if (GetDvrStopDelay() != dvrStopDelay)
    {
        m_config.dvrStopDelay = dvrStopDelay;
        m_modified |= MAIN_CHANGED;
    }
}

void
TxConfig::SetBackpackDisable(bool backpackDisable)
{
    if (m_config.backpackDisable != backpackDisable)
    {
        m_config.backpackDisable = backpackDisable;
        m_modified |= MAIN_CHANGED;
    }
}

void
TxConfig::SetBackpackTlmMode(uint8_t mode)
{
    if (m_config.backpackTlmMode != mode)
    {
        m_config.backpackTlmMode = mode;
        m_modified |= MAIN_CHANGED;
    }
}

void
TxConfig::SetButtonActions(uint8_t button, tx_button_color_t *action)
{
    if (m_config.buttonColors[button].raw != action->raw) {
        m_config.buttonColors[button].raw = action->raw;
        m_modified |= BUTTON_CHANGED;
    }
}

void
TxConfig::SetPTRStartChannel(uint8_t ptrStartChannel)
{
    if (ptrStartChannel != m_model->ptrStartChannel) {
        m_model->ptrStartChannel = ptrStartChannel;
        m_modified |= MODEL_CHANGED;
    }
}

void
TxConfig::SetPTREnableChannel(uint8_t ptrEnableChannel)
{
    if (ptrEnableChannel != m_model->ptrEnableChannel) {
        m_model->ptrEnableChannel = ptrEnableChannel;
        m_modified |= MODEL_CHANGED;
    }
}

void
TxConfig::SetUID(const uint8_t* uid)
{
    if (memcmp(m_config.uid, uid, UID_LEN) != 0)
    {
        memcpy(m_config.uid, uid, UID_LEN);
        m_modified |= UID_CHANGED;
    }
}

#if defined(CUSTOM_DOMAIN_ENABLE)
fhss_config_t
TxConfig::GetCustomDomain() const
{
    return buildCustomDomain(m_config);
}

uint16_t
TxConfig::GetCustomDomainStartMHz() const
{
    return customDomainStartMHz(m_config);
}

uint16_t
TxConfig::GetCustomDomainEndMHz() const
{
    return customDomainEndMHz(m_config);
}

uint8_t
TxConfig::GetCustomDomainChannels() const
{
    return customDomainChannels(m_config);
}

bool
TxConfig::GetCustomDomainEnabled() const
{
    return m_config.custom_domain_enable;
}

void
TxConfig::SetCustomDomain(const fhss_config_t *new_custom_domain, bool enable)
{
    if (new_custom_domain != nullptr)
    {
        SetCustomDomainStartMHz(customDomainRegToMHz(new_custom_domain->freq_start));
        SetCustomDomainEndMHz(customDomainRegToMHz(new_custom_domain->freq_stop));
        SetCustomDomainChannels(new_custom_domain->freq_count);
    }
    SetCustomDomainEnabled(enable);
}

void
TxConfig::SetCustomDomainStartMHz(uint16_t startMHz)
{
    tx_config_t next = m_config;
    setCustomDomainStartMHz(next, startMHz);
    if (m_config.custom_domain_start != next.custom_domain_start
        || m_config.custom_domain_end != next.custom_domain_end
        || m_config.custom_domain_n_channels != next.custom_domain_n_channels
        || m_config.custom_domain_band != next.custom_domain_band)
    {
        m_config.custom_domain_start = next.custom_domain_start;
        m_config.custom_domain_end = next.custom_domain_end;
        m_config.custom_domain_n_channels = next.custom_domain_n_channels;
        m_config.custom_domain_band = next.custom_domain_band;
        m_modified |= MAIN_CHANGED;
    }
}

void
TxConfig::SetCustomDomainEndMHz(uint16_t endMHz)
{
    tx_config_t next = m_config;
    setCustomDomainEndMHz(next, endMHz);
    if (m_config.custom_domain_start != next.custom_domain_start
        || m_config.custom_domain_end != next.custom_domain_end
        || m_config.custom_domain_n_channels != next.custom_domain_n_channels
        || m_config.custom_domain_band != next.custom_domain_band)
    {
        m_config.custom_domain_start = next.custom_domain_start;
        m_config.custom_domain_end = next.custom_domain_end;
        m_config.custom_domain_n_channels = next.custom_domain_n_channels;
        m_config.custom_domain_band = next.custom_domain_band;
        m_modified |= MAIN_CHANGED;
    }
}

void
TxConfig::SetCustomDomainChannels(uint8_t channels)
{
    tx_config_t next = m_config;
    setCustomDomainChannels(next, channels);
    if (m_config.custom_domain_start != next.custom_domain_start
        || m_config.custom_domain_end != next.custom_domain_end
        || m_config.custom_domain_n_channels != next.custom_domain_n_channels
        || m_config.custom_domain_band != next.custom_domain_band)
    {
        m_config.custom_domain_start = next.custom_domain_start;
        m_config.custom_domain_end = next.custom_domain_end;
        m_config.custom_domain_n_channels = next.custom_domain_n_channels;
        m_config.custom_domain_band = next.custom_domain_band;
        m_modified |= MAIN_CHANGED;
    }
}

void
TxConfig::SetCustomDomainEnabled(bool enable)
{
    tx_config_t next = m_config;
    setCustomDomainEnabled(next, enable);
    if (m_config.custom_domain_start != next.custom_domain_start
        || m_config.custom_domain_end != next.custom_domain_end
        || m_config.custom_domain_n_channels != next.custom_domain_n_channels
        || m_config.custom_domain_band != next.custom_domain_band
        || m_config.custom_domain_enable != next.custom_domain_enable)
    {
        m_config.custom_domain_start = next.custom_domain_start;
        m_config.custom_domain_end = next.custom_domain_end;
        m_config.custom_domain_n_channels = next.custom_domain_n_channels;
        m_config.custom_domain_band = next.custom_domain_band;
        m_config.custom_domain_enable = next.custom_domain_enable;
        m_modified |= MAIN_CHANGED;
    }
}
#endif

void
TxConfig::SetDefaults(bool commit)
{
    // Reset everything to 0/false and then just set anything that zero is not appropriate
    memset(&m_config, 0, sizeof(m_config));
#if defined(CUSTOM_DOMAIN_ENABLE)
    setCustomDomainFromConfig(m_config, FHSSgetInitialDomain());
#endif

    m_config.version = TX_CONFIG_VERSION | TX_CONFIG_MAGIC;
    m_config.powerFanThreshold = PWR_250mW;
    m_modified = ALL_CHANGED;

    if (commit)
    {
        m_modified = ALL_CHANGED;
    }

    // Set defaults for button 1
    tx_button_color_t default_actions1 = {
        .val = {
            .color = 226,   // R:255 G:0 B:182
            .actions = {
                {false, 2, ACTION_BIND},
                {true, 0, ACTION_INCREASE_POWER}
            }
        }
    };
    m_config.buttonColors[0].raw = default_actions1.raw;

    // Set defaults for button 2
    tx_button_color_t default_actions2 = {
        .val = {
            .color = 3,     // R:0 G:0 B:255
            .actions = {
                {false, 1, ACTION_GOTO_VTX_CHANNEL},
                {true, 0, ACTION_SEND_VTX}
            }
        }
    };
    m_config.buttonColors[1].raw = default_actions2.raw;

    for (unsigned i=0; i<CONFIG_TX_MODEL_CNT; i++)
    {
        SetModelId(i);
        #if defined(DEFAULT_RATE)
            SetRate(enumRatetoIndex(DEFAULT_RATE));
        #elif defined(RADIO_SX127X)
            SetRate(enumRatetoIndex(RATE_LORA_200HZ));
        #elif defined(RADIO_LR1121)
            SetRate(enumRatetoIndex(POWER_OUTPUT_VALUES_COUNT == 0 ? RATE_LORA_250HZ : RATE_LORA_200HZ));
        #elif defined(RADIO_SX128X)
            SetRate(enumRatetoIndex(RATE_LORA_250HZ));
        #endif
        SetPower(POWERMGNT::getDefaultPower());
#if defined(PLATFORM_ESP32)
        // ESP32 nvs needs to commit every model
        if (commit)
        {
            m_modified |= MODEL_CHANGED;
            Commit();
        }
#endif
    }

#if !defined(PLATFORM_ESP32)
    // ESP8266 just needs one commit
    if (commit)
    {
        Commit();
    }
#endif

    SetModelId(0);
    m_modified = 0;
}

/**
 * Sets ModelId used for subsequent per-model config gets
 * Returns: true if the model has changed
 **/
bool
TxConfig::SetModelId(uint8_t modelId)
{
    // Clamp to valid range — on M0139, CONFIG_TX_MODEL_CNT=1 so all models share slot 0
    model_config_t *newModel = &m_config.model_config[modelId % CONFIG_TX_MODEL_CNT];
    if (newModel != m_model)
    {
        m_model = newModel;
        m_modelId = modelId;
        return true;
    }

    return false;
}
#endif

/////////////////////////////////////////////////////

#if defined(TARGET_RX)

#if defined(PLATFORM_ESP8266)
#include "flash_hal.h"
#endif

#if defined(CUSTOM_DOMAIN_ENABLE)
namespace {
constexpr uint8_t CUSTOM_DOMAIN_HIGH_BAND = 0;
constexpr uint8_t CUSTOM_DOMAIN_LOW_BAND = 1;
constexpr uint16_t CUSTOM_DOMAIN_HIGH_BAND_START_MHZ = 862;
constexpr uint16_t CUSTOM_DOMAIN_HIGH_BAND_STOP_MHZ = 1020;
constexpr uint16_t CUSTOM_DOMAIN_LOW_BAND_START_MHZ = 410;
constexpr uint16_t CUSTOM_DOMAIN_LOW_BAND_STOP_MHZ = 525;
constexpr uint8_t CUSTOM_DOMAIN_MIN_CHANNELS = 2;
constexpr uint8_t CUSTOM_DOMAIN_DEFAULT_CHANNELS = 20;

static uint16_t clampCustomDomainMHz(uint16_t value, uint16_t minValue, uint16_t maxValue)
{
    if (value < minValue)
    {
        return minValue;
    }
    if (value > maxValue)
    {
        return maxValue;
    }
    return value;
}

static uint8_t clampCustomDomainChannels(uint8_t channels)
{
    return channels < CUSTOM_DOMAIN_MIN_CHANNELS ? CUSTOM_DOMAIN_DEFAULT_CHANNELS : channels;
}

static uint8_t customDomainBandFromMHz(uint16_t freqMHz)
{
    return freqMHz < CUSTOM_DOMAIN_HIGH_BAND_START_MHZ ? CUSTOM_DOMAIN_LOW_BAND : CUSTOM_DOMAIN_HIGH_BAND;
}

static uint16_t customDomainBandStartMHz(uint8_t band)
{
    return band == CUSTOM_DOMAIN_LOW_BAND ? CUSTOM_DOMAIN_LOW_BAND_START_MHZ : CUSTOM_DOMAIN_HIGH_BAND_START_MHZ;
}

static uint16_t customDomainBandStopMHz(uint8_t band)
{
    return band == CUSTOM_DOMAIN_LOW_BAND ? CUSTOM_DOMAIN_LOW_BAND_STOP_MHZ : CUSTOM_DOMAIN_HIGH_BAND_STOP_MHZ;
}

template <typename TConfig>
static void normalizeCustomDomain(TConfig &cfg)
{
    cfg.custom_domain_band = cfg.custom_domain_band == CUSTOM_DOMAIN_LOW_BAND ? CUSTOM_DOMAIN_LOW_BAND : CUSTOM_DOMAIN_HIGH_BAND;

    const uint16_t bandStartMHz = customDomainBandStartMHz(cfg.custom_domain_band);
    const uint16_t bandStopMHz = customDomainBandStopMHz(cfg.custom_domain_band);
    uint16_t startMHz = bandStartMHz + cfg.custom_domain_start;
    uint16_t endMHz = bandStartMHz + cfg.custom_domain_end;

    cfg.custom_domain_n_channels = clampCustomDomainChannels(cfg.custom_domain_n_channels);
    startMHz = clampCustomDomainMHz(startMHz, bandStartMHz, bandStopMHz - 1);
    endMHz = clampCustomDomainMHz(endMHz, bandStartMHz + 1, bandStopMHz);

    if (endMHz <= startMHz)
    {
        if (startMHz >= bandStopMHz)
        {
            startMHz = bandStopMHz - 1;
        }
        endMHz = startMHz + 1;
    }

    cfg.custom_domain_start = startMHz - bandStartMHz;
    cfg.custom_domain_end = endMHz - bandStartMHz;
}

template <typename TConfig>
static uint16_t customDomainStartMHz(TConfig cfg)
{
    normalizeCustomDomain(cfg);
    return customDomainBandStartMHz(cfg.custom_domain_band) + cfg.custom_domain_start;
}

template <typename TConfig>
static uint16_t customDomainEndMHz(TConfig cfg)
{
    normalizeCustomDomain(cfg);
    return customDomainBandStartMHz(cfg.custom_domain_band) + cfg.custom_domain_end;
}

template <typename TConfig>
static uint8_t customDomainChannels(TConfig cfg)
{
    normalizeCustomDomain(cfg);
    return cfg.custom_domain_n_channels;
}

template <typename TConfig>
static fhss_config_t buildCustomDomain(TConfig cfg)
{
    normalizeCustomDomain(cfg);

    const uint16_t startMHz = customDomainBandStartMHz(cfg.custom_domain_band) + cfg.custom_domain_start;
    const uint16_t endMHz = customDomainBandStartMHz(cfg.custom_domain_band) + cfg.custom_domain_end;
    const uint32_t centerHz = ((uint32_t)startMHz + (uint32_t)endMHz) * 500000U;

    return {
        "CUSTOM",
        FREQ_HZ_TO_REG_VAL((uint32_t)startMHz * 1000000U),
        FREQ_HZ_TO_REG_VAL((uint32_t)endMHz * 1000000U),
        cfg.custom_domain_n_channels,
        centerHz
    };
}

#if defined(RADIO_LR1121)
static uint16_t customDomainRegToMHz(uint32_t regFreq)
{
    return regFreq / 1000000U;
}
#else
static uint16_t customDomainRegToMHz(uint32_t regFreq)
{
    return (uint16_t)(((double)regFreq * FREQ_STEP / 1000000.0) + 0.5);
}
#endif

template <typename TConfig>
static void setCustomDomainFromConfig(TConfig &cfg, const fhss_config_t &domain)
{
    const uint16_t startMHz = customDomainRegToMHz(domain.freq_start);
    const uint16_t endMHz = customDomainRegToMHz(domain.freq_stop);
    const uint8_t band = customDomainBandFromMHz(startMHz);
    const uint16_t bandStartMHz = customDomainBandStartMHz(band);

    cfg.custom_domain_band = band;
    cfg.custom_domain_start = startMHz - bandStartMHz;
    cfg.custom_domain_end = endMHz - bandStartMHz;
    cfg.custom_domain_n_channels = domain.freq_count;
    cfg.custom_domain_enable = false;
    normalizeCustomDomain(cfg);
}

template <typename TConfig>
static void setCustomDomainStartMHz(TConfig &cfg, uint16_t startMHz)
{
    const uint8_t band = customDomainBandFromMHz(startMHz);
    const uint16_t bandStartMHz = customDomainBandStartMHz(band);
    const uint16_t bandStopMHz = customDomainBandStopMHz(band);

    startMHz = clampCustomDomainMHz(startMHz, bandStartMHz, bandStopMHz - 1);

    uint16_t endMHz = customDomainEndMHz(cfg);
    if (customDomainBandFromMHz(endMHz) != band)
    {
        endMHz = bandStopMHz;
    }
    endMHz = clampCustomDomainMHz(endMHz, startMHz + 1, bandStopMHz);

    cfg.custom_domain_band = band;
    cfg.custom_domain_start = startMHz - bandStartMHz;
    cfg.custom_domain_end = endMHz - bandStartMHz;
    normalizeCustomDomain(cfg);
}

template <typename TConfig>
static void setCustomDomainEndMHz(TConfig &cfg, uint16_t endMHz)
{
    const uint8_t band = customDomainBandFromMHz(endMHz);
    const uint16_t bandStartMHz = customDomainBandStartMHz(band);
    const uint16_t bandStopMHz = customDomainBandStopMHz(band);

    endMHz = clampCustomDomainMHz(endMHz, bandStartMHz + 1, bandStopMHz);

    uint16_t startMHz = customDomainStartMHz(cfg);
    if (customDomainBandFromMHz(startMHz) != band)
    {
        startMHz = bandStartMHz;
    }
    startMHz = clampCustomDomainMHz(startMHz, bandStartMHz, endMHz - 1);

    cfg.custom_domain_band = band;
    cfg.custom_domain_start = startMHz - bandStartMHz;
    cfg.custom_domain_end = endMHz - bandStartMHz;
    normalizeCustomDomain(cfg);
}

template <typename TConfig>
static void setCustomDomainChannels(TConfig &cfg, uint8_t channels)
{
    cfg.custom_domain_n_channels = clampCustomDomainChannels(channels);
    normalizeCustomDomain(cfg);
}

template <typename TConfig>
static void setCustomDomainEnabled(TConfig &cfg, bool enable)
{
    if (enable)
    {
        normalizeCustomDomain(cfg);
    }
    cfg.custom_domain_enable = enable;
}
} // namespace

bool FHSSuseConfiguredCustomDomain()
{
    return config.GetCustomDomainEnabled();
}

fhss_config_t FHSSgetConfiguredCustomDomain()
{
    return config.GetCustomDomain();
}
#endif

RxConfig::RxConfig()
{
}

void RxConfig::Load()
{
    m_modified = false;
    m_eeprom->Get(0, m_config);

    uint32_t version = 0;
    if ((m_config.version & CONFIG_MAGIC_MASK) == RX_CONFIG_MAGIC)
       version = m_config.version & ~CONFIG_MAGIC_MASK;
    DBGLN("Config version %u", version);

#ifdef NO_TEAMRACE
    // Turn off teamrace
    m_config.teamraceChannel = AUX7; // CH11
    m_config.teamracePosition = 0;

    m_modified = true;
    Commit();
#endif

    // If version is current, all done
    if (version == RX_CONFIG_VERSION)
    {
        CheckUpdateFlashedUid(false);
        return;
    }

    // V11→V12: teamrace fields moved before pwmChannels to fit within 128-byte EEPROM.
    // Preserve uid and other header fields, reset everything else to defaults.
    if (version == 11)
    {
        UpgradeEepromV11();
        CheckUpdateFlashedUid(false);
        return;
    }

    if (version == 13)
    {
        UpgradeEepromV13ToV14();
        CheckUpdateFlashedUid(false);
        return;
    }

    // Can't upgrade from version <4, or when flashing a previous version, just use defaults.
    if (version < 4 || version > RX_CONFIG_VERSION)
    {
        SetDefaults(true);
        CheckUpdateFlashedUid(true);
        return;
    }

    // Upgrade EEPROM, starting with defaults
    SetDefaults(false);
    UpgradeEepromV4();
    UpgradeEepromV5();
    UpgradeEepromV6();
    UpgradeEepromV7V8();
    // TODO: implement an upgrade function instead of resetting to defaults
    SetDefaults(false);
    m_config.version = RX_CONFIG_VERSION | RX_CONFIG_MAGIC;
    m_modified = true;
    Commit();
}

void RxConfig::CheckUpdateFlashedUid(bool skipDescrimCheck)
{
    // No binding phrase flashed, nothing to do
    if (!firmwareOptions.hasUID)
        return;
    // If already copied binding info, do not replace
    if (!skipDescrimCheck && m_config.flash_discriminator == firmwareOptions.flash_discriminator)
        return;

    // Save the new UID along with this discriminator to prevent resetting every boot
    SetUID(firmwareOptions.uid);
    m_config.flash_discriminator = firmwareOptions.flash_discriminator;
    // Reset the power on counter because this is following a flash, may have taken a few boots to flash
    m_config.powerOnCounter = 0;
    // SetUID should set this but just in case that gets removed, flash_discriminator needs to be saved
    m_modified = true;

    Commit();
}

// ========================================================
// V4 Upgrade

static void PwmConfigV4(v4_rx_config_pwm_t const * const v4, rx_config_pwm_t * const current)
{
    current->val.failsafe = v4->val.failsafe;
    current->val.inputChannel = v4->val.inputChannel;
    current->val.inverted = v4->val.inverted;
}

void RxConfig::UpgradeEepromV4()
{
    v4_rx_config_t v4Config;
    m_eeprom->Get(0, v4Config);

    if ((v4Config.version & ~CONFIG_MAGIC_MASK) == 4)
    {
        UpgradeUid(nullptr, v4Config.isBound ? v4Config.uid : nullptr);
        m_config.modelId = v4Config.modelId;
        #if defined(GPIO_PIN_PWM_OUTPUTS)
        // OG PWMP had only 8 channels
        for (unsigned ch=0; ch<8; ++ch)
        {
            PwmConfigV4(&v4Config.pwmChannels[ch], &m_config.pwmChannels[ch]);
        }
        #endif
    }
}

// ========================================================
// V5 Upgrade

static void PwmConfigV5(v5_rx_config_pwm_t const * const v5, rx_config_pwm_t * const current)
{
    current->val.failsafe = v5->val.failsafe;
    current->val.inputChannel = v5->val.inputChannel;
    current->val.inverted = v5->val.inverted;
    current->val.narrow = v5->val.narrow;
    current->val.mode = v5->val.mode;
    if (v5->val.mode > som400Hz)
    {
        current->val.mode += 1;
    }
}

void RxConfig::UpgradeEepromV5()
{
    v5_rx_config_t v5Config;
    m_eeprom->Get(0, v5Config);

    if ((v5Config.version & ~CONFIG_MAGIC_MASK) == 5)
    {
        UpgradeUid(v5Config.onLoan ? v5Config.loanUID : nullptr, v5Config.isBound ? v5Config.uid : nullptr);
        m_config.vbat.scale = v5Config.vbatScale;
        m_config.power = v5Config.power;
        m_config.antennaMode = v5Config.antennaMode;
        m_config.forceTlmOff = v5Config.forceTlmOff;
        m_config.rateInitialIdx = v5Config.rateInitialIdx;
        m_config.modelId = v5Config.modelId;

        #if defined(GPIO_PIN_PWM_OUTPUTS)
        for (unsigned ch=0; ch<16; ++ch)
        {
            PwmConfigV5(&v5Config.pwmChannels[ch], &m_config.pwmChannels[ch]);
        }
        #endif
    }
}

// ========================================================
// V6 Upgrade

static void PwmConfigV6(v6_rx_config_pwm_t const * const v6, rx_config_pwm_t * const current)
{
    current->val.failsafe = v6->val.failsafe;
    current->val.inputChannel = v6->val.inputChannel;
    current->val.inverted = v6->val.inverted;
    current->val.narrow = v6->val.narrow;
    current->val.mode = v6->val.mode;
}

void RxConfig::UpgradeEepromV6()
{
    v6_rx_config_t v6Config;
    m_eeprom->Get(0, v6Config);

    if ((v6Config.version & ~CONFIG_MAGIC_MASK) == 6)
    {
        UpgradeUid(v6Config.onLoan ? v6Config.loanUID : nullptr, v6Config.isBound ? v6Config.uid : nullptr);
        m_config.vbat.scale = v6Config.vbatScale;
        m_config.power = v6Config.power;
        m_config.antennaMode = v6Config.antennaMode;
        m_config.forceTlmOff = v6Config.forceTlmOff;
        m_config.rateInitialIdx = v6Config.rateInitialIdx;
        m_config.modelId = v6Config.modelId;

        #if defined(GPIO_PIN_PWM_OUTPUTS)
        for (unsigned ch=0; ch<16; ++ch)
        {
            PwmConfigV6(&v6Config.pwmChannels[ch], &m_config.pwmChannels[ch]);
        }
        #endif
    }
}

// ========================================================
// V7/V8 Upgrade

void RxConfig::UpgradeEepromV7V8()
{
    v7_rx_config_t v7Config;
    m_eeprom->Get(0, v7Config);

    bool isV8 = (v7Config.version & ~CONFIG_MAGIC_MASK) == 8;
    if (isV8 || (v7Config.version & ~CONFIG_MAGIC_MASK) == 7)
    {
        UpgradeUid(v7Config.onLoan ? v7Config.loanUID : nullptr, v7Config.isBound ? v7Config.uid : nullptr);

        m_config.vbat.scale = v7Config.vbatScale;
        m_config.power = v7Config.power;
        m_config.antennaMode = v7Config.antennaMode;
        m_config.forceTlmOff = v7Config.forceTlmOff;
        m_config.rateInitialIdx = v7Config.rateInitialIdx;
        m_config.modelId = v7Config.modelId;
        m_config.serialProtocol = v7Config.serialProtocol;
        m_config.failsafeMode = v7Config.failsafeMode;
    }
}

void RxConfig::UpgradeUid(uint8_t *onLoanUid, uint8_t *boundUid)
{
    // Convert to traditional binding
    // On loan? Now you own
    if (onLoanUid)
    {
        memcpy(m_config.uid, onLoanUid, UID_LEN);
    }
    // Compiled in UID? Bind to that
    else if (firmwareOptions.hasUID)
    {
        memcpy(m_config.uid, firmwareOptions.uid, UID_LEN);
        m_config.flash_discriminator = firmwareOptions.flash_discriminator;
    }
    else if (boundUid)
    {
        // keep binding
        memcpy(m_config.uid, boundUid, UID_LEN);
    }
    else
    {
        // No bind
        memset(m_config.uid, 0, UID_LEN);
    }
}

void RxConfig::UpgradeEepromV9()
{
    SetDefaults(true);
}

void RxConfig::UpgradeEepromV11()
{
    // V11→V12: teamrace/targetSysId/sourceSysId moved before pwmChannels so they
    // land within the first 128 bytes of EEPROM (physical chip capacity on M0184).
    // The V11 struct had these fields at offset 216 which is beyond the physical chip,
    // so they were never actually stored.  Preserve uid and other header fields
    // (offsets 0-23 are identical in V11 and V12) then reset everything else.
    uint8_t savedUid[UID_LEN];
    memcpy(savedUid, m_config.uid, UID_LEN);
    uint32_t savedFlashDiscriminator = m_config.flash_discriminator;

    SetDefaults(false);

    memcpy(m_config.uid, savedUid, UID_LEN);
    m_config.flash_discriminator = savedFlashDiscriminator;
    m_modified = true;
    Commit();
}

void RxConfig::UpgradeEepromV13ToV14()
{
    v13_rx_config_t v13Config;
    m_eeprom->Get(0, v13Config);

    SetDefaults(false);

    memcpy(m_config.uid, v13Config.uid, UID_LEN);
    #define LAZY(member) m_config.member = v13Config.member
    LAZY(unused_padding);
    LAZY(serial1Protocol);
    LAZY(serial1Protocol_unused);
    LAZY(flash_discriminator);
    LAZY(bindStorage);
    LAZY(power);
    LAZY(antennaMode);
    LAZY(powerOnCounter);
    LAZY(forceTlmOff);
    LAZY(rateInitialIdx);
    LAZY(modelId);
    LAZY(serialProtocol);
    LAZY(failsafeMode);
    LAZY(unused);
    LAZY(teamraceChannel);
    LAZY(teamracePosition);
    LAZY(teamracePitMode);
    LAZY(targetSysId);
    LAZY(sourceSysId);
    LAZY(reserved1);
    #undef LAZY

    m_config.vbat.scale = v13Config.vbat.scale;
    m_config.vbat.offset = v13Config.vbat.offset;

    for (uint8_t i = 0; i < PWM_MAX_CHANNELS; ++i)
    {
        m_config.pwmChannels[i] = v13Config.pwmChannels[i];
    }

    m_config.version = RX_CONFIG_VERSION | RX_CONFIG_MAGIC;
    m_modified = true;
    Commit();
}

bool RxConfig::GetIsBound() const
{
    if (m_config.bindStorage == BINDSTORAGE_VOLATILE)
        return false;
    return UID_IS_BOUND(m_config.uid);
}

bool RxConfig::IsOnLoan() const
{
    if (m_config.bindStorage != BINDSTORAGE_RETURNABLE)
        return false;
    if (!firmwareOptions.hasUID)
        return false;
    return GetIsBound() && memcmp(m_config.uid, firmwareOptions.uid, UID_LEN) != 0;
}

#if defined(PLATFORM_ESP8266)
#define EMPTY_SECTOR ((FS_start - 0x1000 - 0x40200000) / SPI_FLASH_SEC_SIZE) // empty sector before FS area start
static bool erase_power_on_count = false;
static int realPowerOnCounter = -1;
uint8_t
RxConfig::GetPowerOnCounter() const
{
    if (realPowerOnCounter == -1) {
        byte zeros[16];
        ESP.flashRead(EMPTY_SECTOR * SPI_FLASH_SEC_SIZE, zeros, sizeof(zeros));
        realPowerOnCounter = sizeof(zeros);
        for (int i=0 ; i<sizeof(zeros) ; i++) {
            if (zeros[i] != 0) {
                realPowerOnCounter = i;
                break;
            }
        }
    }
    return realPowerOnCounter;
}
#endif

void
RxConfig::Commit()
{
#if defined(PLATFORM_ESP8266)
    if (erase_power_on_count)
    {
        ESP.flashEraseSector(EMPTY_SECTOR);
        erase_power_on_count = false;
    }
#endif
    if (!m_modified)
    {
        // No changes
        return;
    }

    // Write the struct to eeprom
    m_eeprom->Put(0, m_config);
    m_eeprom->Commit();


    m_modified = false;
}

// Setters
void
RxConfig::SetUID(uint8_t* uid)
{
    for (uint8_t i = 0; i < UID_LEN; ++i)
    {
        m_config.uid[i] = uid[i];
    }
    m_modified = true;
}

void
RxConfig::SetPowerOnCounter(uint8_t powerOnCounter)
{
#if defined(PLATFORM_ESP8266)
    realPowerOnCounter = powerOnCounter;
    if (powerOnCounter == 0)
    {
        erase_power_on_count = true;
        m_modified = true;
    }
    else
    {
        byte zeros[16] = {0};
        ESP.flashWrite(EMPTY_SECTOR * SPI_FLASH_SEC_SIZE, zeros, std::min((size_t)powerOnCounter, sizeof(zeros)));
    }
#else
    if (m_config.powerOnCounter != powerOnCounter)
    {
        m_config.powerOnCounter = powerOnCounter;
        m_modified = true;
    }
#endif
}

void
RxConfig::SetModelId(uint8_t modelId)
{
    if (m_config.modelId != modelId)
    {
        m_config.modelId = modelId;
        m_modified = true;
    }
}

void
RxConfig::SetPower(uint8_t power)
{
    if (m_config.power != power)
    {
        m_config.power = power;
        m_modified = true;
    }
}


void
RxConfig::SetAntennaMode(uint8_t antennaMode)
{
    //0 and 1 is use for gpio_antenna_select
    // 2 is diversity
    if (m_config.antennaMode != antennaMode)
    {
        m_config.antennaMode = antennaMode;
        m_modified = true;
    }
}

void
RxConfig::SetDefaults(bool commit)
{
    // Reset everything to 0/false and then just set anything that zero is not appropriate
    memset(&m_config, 0, sizeof(m_config));
#if defined(CUSTOM_DOMAIN_ENABLE)
    setCustomDomainFromConfig(m_config, FHSSgetInitialDomain());
#endif
    // All-0xFF uid is treated as bound but matches no TX, keeping RX inert until explicitly bound
    // warning this will reset bind keys on eeprom updates on FW updates that increment the eeprom version.
    memset(m_config.uid, 0xFF, UID_LEN);

    m_config.version = RX_CONFIG_VERSION | RX_CONFIG_MAGIC;
    m_config.modelId = 0xff;
    m_config.power = POWERMGNT::getDefaultPower();
    if (GPIO_PIN_ANT_CTRL != UNDEF_PIN)
        m_config.antennaMode = 2; // 2 is diversity
    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
        m_config.antennaMode = 0; // 0 is diversity for dual radio

#if defined(GPIO_PIN_PWM_OUTPUTS)
    for (int ch=0; ch<PWM_MAX_CHANNELS; ++ch)
    {
        uint8_t mode = som50Hz;
        // setup defaults for hardware defined I2C pins that are also IO pins
        if (ch < GPIO_PIN_PWM_OUTPUTS_COUNT)
        {
            if (GPIO_PIN_PWM_OUTPUTS[ch] == GPIO_PIN_SCL)
            {
                mode = somSCL;
            }
            else if (GPIO_PIN_PWM_OUTPUTS[ch] == GPIO_PIN_SDA)
            {
                mode = somSDA;
            }
        }
        SetPwmChannel(ch, 512, ch, false, mode, false);
    }
    SetPwmChannel(2, 0, 2, false, 0, false); // ch2 is throttle, failsafe it to 988
#endif

    m_config.teamraceChannel = AUX7; // CH11
    m_config.teamracePosition = 0;

#if defined(RCVR_INVERT_TX)
    m_config.serialProtocol = PROTOCOL_INVERTED_CRSF;
#endif

    if (commit)
    {
        // Prevent rebinding to the flashed UID on first boot
        m_config.flash_discriminator = firmwareOptions.flash_discriminator;
        m_modified = true;
        Commit();
    }
}

void
RxConfig::SetStorageProvider(ELRS_EEPROM *eeprom)
{
    if (eeprom)
    {
        m_eeprom = eeprom;
    }
}

#if defined(GPIO_PIN_PWM_OUTPUTS)
void
RxConfig::SetPwmChannel(uint8_t ch, uint16_t failsafe, uint8_t inputCh, bool inverted, uint8_t mode, bool narrow)
{
    if (ch > PWM_MAX_CHANNELS)
        return;

    rx_config_pwm_t *pwm = &m_config.pwmChannels[ch];
    rx_config_pwm_t newConfig;
    newConfig.val.failsafe = failsafe;
    newConfig.val.inputChannel = inputCh;
    newConfig.val.inverted = inverted;
    newConfig.val.mode = mode;
    newConfig.val.narrow = narrow;
    if (pwm->raw.raw[0] == newConfig.raw.raw[0] && pwm->raw.raw[1] == newConfig.raw.raw[1] && pwm->raw.raw[2] == newConfig.raw.raw[2])
        return;

    pwm->raw.raw[0] = newConfig.raw.raw[0];
    pwm->raw.raw[1] = newConfig.raw.raw[1];
    pwm->raw.raw[2] = newConfig.raw.raw[2];
    m_modified = true;
}

void
RxConfig::SetPwmChannelRaw(uint8_t ch, uint32_t *raw)
{
    if (ch > PWM_MAX_CHANNELS)
        return;

    rx_config_pwm_t *pwm = &m_config.pwmChannels[ch];
    // if (pwm->raw.raw[0] == raw[0] && pwm->raw.raw[1] == raw[1] && pwm->raw.raw[2] == raw[2])
        // return;

    pwm->raw.raw[0] = raw[0];
    pwm->raw.raw[1] = raw[1];
    pwm->raw.raw[2] = raw[2];
    m_modified = true;
}
#endif

void
RxConfig::SetForceTlmOff(bool forceTlmOff)
{
    if (m_config.forceTlmOff != forceTlmOff)
    {
        m_config.forceTlmOff = forceTlmOff;
        m_modified = true;
    }
}

void
RxConfig::SetRateInitialIdx(uint8_t rateInitialIdx)
{
    if (m_config.rateInitialIdx != rateInitialIdx)
    {
        m_config.rateInitialIdx = rateInitialIdx;
        m_modified = true;
    }
}

void RxConfig::SetSerialProtocol(eSerialProtocol serialProtocol)
{
    if (m_config.serialProtocol != serialProtocol)
    {
        m_config.serialProtocol = serialProtocol;
        m_modified = true;
    }
}

#if defined(PLATFORM_ESP32)
void RxConfig::SetSerial1Protocol(eSerial1Protocol serialProtocol)
{
    if (m_config.serial1Protocol != serialProtocol)
    {
        m_config.serial1Protocol = serialProtocol;
        m_modified = true;
    }
}
#endif

void RxConfig::SetTeamraceChannel(uint8_t teamraceChannel)
{
    if (m_config.teamraceChannel != teamraceChannel)
    {
        m_config.teamraceChannel = teamraceChannel;
        m_modified = true;
    }
}

void RxConfig::SetTeamracePosition(uint8_t teamracePosition)
{
    if (m_config.teamracePosition != teamracePosition)
    {
        m_config.teamracePosition = teamracePosition;
        m_modified = true;
    }
}

void RxConfig::SetFailsafeMode(eFailsafeMode failsafeMode)
{
    if (m_config.failsafeMode != failsafeMode)
    {
        m_config.failsafeMode = failsafeMode;
        m_modified = true;
    }
}

void RxConfig::SetBindStorage(rx_config_bindstorage_t value)
{
    if (m_config.bindStorage != value)
    {
        // If switching away from returnable, revert
        ReturnLoan();
        m_config.bindStorage = value;
        m_modified = true;
    }
}

void RxConfig::SetTargetSysId(uint8_t value)
{
    if (m_config.targetSysId != value)
    {
        m_config.targetSysId = value;
        m_modified = true;
    }
}
void RxConfig::SetSourceSysId(uint8_t value)
{
    if (m_config.sourceSysId != value)
    {
        m_config.sourceSysId = value;
        m_modified = true;
    }
}

#if defined(CUSTOM_DOMAIN_ENABLE)
fhss_config_t RxConfig::GetCustomDomain() const
{
    return buildCustomDomain(m_config);
}

uint16_t RxConfig::GetCustomDomainStartMHz() const
{
    return customDomainStartMHz(m_config);
}

uint16_t RxConfig::GetCustomDomainEndMHz() const
{
    return customDomainEndMHz(m_config);
}

uint8_t RxConfig::GetCustomDomainChannels() const
{
    return customDomainChannels(m_config);
}

bool RxConfig::GetCustomDomainEnabled() const
{
    return m_config.custom_domain_enable;
}

void RxConfig::SetCustomDomain(const fhss_config_t *new_custom_domain, bool enable)
{
    if (new_custom_domain != nullptr)
    {
        SetCustomDomainStartMHz(customDomainRegToMHz(new_custom_domain->freq_start));
        SetCustomDomainEndMHz(customDomainRegToMHz(new_custom_domain->freq_stop));
        SetCustomDomainChannels(new_custom_domain->freq_count);
    }
    SetCustomDomainEnabled(enable);
}

void RxConfig::SetCustomDomainStartMHz(uint16_t startMHz)
{
    rx_config_t next = m_config;
    setCustomDomainStartMHz(next, startMHz);
    if (m_config.custom_domain_start != next.custom_domain_start
        || m_config.custom_domain_end != next.custom_domain_end
        || m_config.custom_domain_n_channels != next.custom_domain_n_channels
        || m_config.custom_domain_band != next.custom_domain_band)
    {
        m_config.custom_domain_start = next.custom_domain_start;
        m_config.custom_domain_end = next.custom_domain_end;
        m_config.custom_domain_n_channels = next.custom_domain_n_channels;
        m_config.custom_domain_band = next.custom_domain_band;

void RxConfig::SetCustomDomainEndMHz(uint16_t endMHz)
{
    rx_config_t next = m_config;
    setCustomDomainEndMHz(next, endMHz);
    if (m_config.custom_domain_start != next.custom_domain_start
        || m_config.custom_domain_end != next.custom_domain_end
        || m_config.custom_domain_n_channels != next.custom_domain_n_channels
        || m_config.custom_domain_band != next.custom_domain_band)
    {
        m_config.custom_domain_start = next.custom_domain_start;
        m_config.custom_domain_end = next.custom_domain_end;
        m_config.custom_domain_n_channels = next.custom_domain_n_channels;
        m_config.custom_domain_band = next.custom_domain_band;
        m_modified = true;
    }
}

void RxConfig::SetCustomDomainChannels(uint8_t channels)
{
    rx_config_t next = m_config;
    setCustomDomainChannels(next, channels);
    if (m_config.custom_domain_start != next.custom_domain_start
        || m_config.custom_domain_end != next.custom_domain_end
        || m_config.custom_domain_n_channels != next.custom_domain_n_channels
        || m_config.custom_domain_band != next.custom_domain_band)
    {
        m_config.custom_domain_start = next.custom_domain_start;
        m_config.custom_domain_end = next.custom_domain_end;
        m_config.custom_domain_n_channels = next.custom_domain_n_channels;
        m_config.custom_domain_band = next.custom_domain_band;
        m_modified = true;
    }
}

void RxConfig::SetCustomDomainEnabled(bool enable)
{
    rx_config_t next = m_config;
    setCustomDomainEnabled(next, enable);
    if (m_config.custom_domain_start != next.custom_domain_start
        || m_config.custom_domain_end != next.custom_domain_end
        || m_config.custom_domain_n_channels != next.custom_domain_n_channels
        || m_config.custom_domain_band != next.custom_domain_band
        || m_config.custom_domain_enable != next.custom_domain_enable)
    {
        m_config.custom_domain_start = next.custom_domain_start;
        m_config.custom_domain_end = next.custom_domain_end;
        m_config.custom_domain_n_channels = next.custom_domain_n_channels;
        m_config.custom_domain_band = next.custom_domain_band;
        m_config.custom_domain_enable = next.custom_domain_enable;
        m_modified = true;
    }
}
#endif

void RxConfig::SetSyntheticPWM(bool value)
{
    if (m_config.syntheticPWM != value)
    {
        m_config.syntheticPWM = value;
        m_modified = true;
    }
}

void RxConfig::ReturnLoan()
{
    if (IsOnLoan())
    {
        // go back to flashed UID if there is one
        // or unbind if there is not
        if (firmwareOptions.hasUID)
            memcpy(m_config.uid, firmwareOptions.uid, UID_LEN);
        else
            memset(m_config.uid, 0, UID_LEN);

        m_modified = true;
    }
}

#endif
