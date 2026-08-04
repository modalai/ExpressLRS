#pragma once

#define ELRS4_DATA_DL_SHIFT 1
#define ELRS4_DATA_DL_BYTES_PER_CALL 5
#define ELRS4_DATA_DL_MAX_PACKAGES (255 >> ELRS4_DATA_DL_SHIFT)
#define ELRS8_DATA_DL_BYTES_PER_CALL 10 // some of this is consumed by LinkStats buuuut....
#define ELRS8_DATA_DL_SHIFT 3
#define ELRS8_DATA_DL_MAX_PACKAGES (255 >> ELRS8_DATA_DL_SHIFT)

#define ELRS4_DATA_UL_BYTES_PER_CALL 5
#define ELRS8_DATA_UL_BYTES_PER_CALL 10
#define ELRS_DATA_UL_BUFFER 65
#define ELRS_MSP_MAX_PACKAGES ((ELRS_DATA_UL_BUFFER / ELRS4_DATA_UL_BYTES_PER_CALL)+1)

#define AP_MAX_BUF_LEN  64

#if defined(M0139) && defined(TARGET_RX)
enum ModalPwmCommand : uint8_t
{
    SET_PWM_CH = 0xF3,
    SET_PWM_VAL = 0xF4,
    SET_PWM_DEFAULT = 0xFF,
};

struct __attribute__((packed)) modal_pwm_config_t
{
    uint8_t pinIndex;
    uint16_t failsafe;
    uint8_t inputChannel;
    uint8_t inverted;
    uint8_t mode;
    uint8_t narrow;
    uint8_t failsafeMode;
    uint8_t mapMode;
    uint8_t unused;
    uint16_t mapInVal1;
    uint16_t mapInVal2;
    uint16_t mapInVal3;
    uint16_t mapOutVal1;
    uint16_t mapOutVal2;
    uint16_t mapOutVal3;
};

struct __attribute__((packed)) modal_pwm_override_t
{
    uint8_t command;
    uint8_t rcChannel;
    uint16_t crsfChannelValue;
};

static_assert(sizeof(modal_pwm_config_t) == 22, "ModalAI PWM configuration wire layout changed");
static_assert(sizeof(modal_pwm_override_t) == 4, "ModalAI PWM override wire layout changed");
#endif
