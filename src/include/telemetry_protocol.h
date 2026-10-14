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
    SET_PIN_OUTPUT = 0xF5,
    SET_PWM_DEFAULT = 0xFF,
};

// Result of a SET_PIN_OUTPUT command, returned to the FC in the acknowledgement
enum ModalPinOutputStatus : uint8_t
{
    PIN_OUTPUT_OK = 0,          // the pin outputs the commanded value
    PIN_OUTPUT_RELEASED = 1,    // the pin follows its input channel or failsafe again
    PIN_OUTPUT_NEEDS_ARM = 2,   // accepted, but the pin holds its failsafe until the outputs are armed
    PIN_OUTPUT_BAD_PIN = 3,     // no such pin
    PIN_OUTPUT_BAD_MODE = 4,    // the pin's output mode is not servo PWM, on/off or duty
    PIN_OUTPUT_BAD_VALUE = 5,   // the value is outside MODAL_PIN_OUTPUT_MIN_US..MODAL_PIN_OUTPUT_MAX_US
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

// Drives one pin with a raw pulse width, bypassing its input channel and map.
// The pin returns to its input channel or failsafe 1 s after the last command,
// or at once when usOutput is 0.
struct __attribute__((packed)) modal_pin_output_t
{
    uint8_t command;    // SET_PIN_OUTPUT
    uint8_t pinIndex;   // 0-based
    uint16_t usOutput;  // big-endian
};

// The RX answers every SET_PIN_OUTPUT with this frame, sent to the FC
struct __attribute__((packed)) modal_pin_output_ack_t
{
    uint8_t command;    // SET_PIN_OUTPUT
    uint8_t pinIndex;
    uint8_t status;     // ModalPinOutputStatus
    uint16_t usOutput;  // big-endian, what the pin outputs now (0 for no pulses or unknown)
};

constexpr uint16_t MODAL_PIN_OUTPUT_MIN_US = 800U;
constexpr uint16_t MODAL_PIN_OUTPUT_MAX_US = 2200U;

static_assert(sizeof(modal_pwm_config_t) == 22, "ModalAI PWM configuration wire layout changed");
static_assert(sizeof(modal_pwm_override_t) == 4, "ModalAI PWM override wire layout changed");
static_assert(sizeof(modal_pin_output_t) == 4, "ModalAI pin output wire layout changed");
static_assert(sizeof(modal_pin_output_ack_t) == 5, "ModalAI pin output acknowledgement wire layout changed");
#endif
