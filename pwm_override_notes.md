# ELRS CRSF Packet structure

CRSF EXT packet format used by ELRS:

```c
typedef struct crsf_ext_header_s
{
    uint8_t device_addr; // CRSF_ADDRESS_CRSF_RECEIVER = 0xEC
    uint8_t frame_size;
    crsf_frame_type_e type; // CRSF_FRAMETYPE_COMMAND = 0x32 
    // Extended fields
    crsf_addr_e dest_addr; // CRSF_ADDRESS_CRSF_RECEIVER = 0xEC
    crsf_addr_e orig_addr; // CRSF_ADDRESS_FLIGHT_CONTROLLER = 0xC8
    uint8_t payload[0];
} PACKED crsf_ext_header_t;
```

ELRS also checks the 16-bit CRSF CRC at the end of the frame.

The payload field for the custom CRSF PWM Override packet is described below.

# Payload

```c
typedef struct pwm_val_override_t {
    uint8_t command; // SET_PWM_VAL = 0xF4
    uint8_t rc_channel;
    uint16_t crsf_channel_value;
} __attribute__((packed)) pwm_val_override_t;
```

- rc_channel: The *input* channel to override. If the edgetx controller uses channel 7 to control the LED bars, then that is the channel number which should be passed here. (This number is 1-indexed)
- crsf_channel_value: The usual CRSF RC channels packed fits each channel into 11 bits so the values get translated to pwm values using the macros below. So to send a PWM microsecond value of 1000, crsf_channel_value should be 191.

```c
// from crsf_protocol.h
#define CRSF_CHANNEL_VALUE_MIN  172 // 987us - actual CRSF min is 0 with E.Limits on
#define CRSF_CHANNEL_VALUE_1000 191
#define CRSF_CHANNEL_VALUE_MID  992
#define CRSF_CHANNEL_VALUE_2000 1792
#define CRSF_CHANNEL_VALUE_MAX  1811 // 2012us - actual CRSF max is 1984 with E.Limits on
#define CRSF_MAX_PACKET_LEN 64
```

Note, https://github.com/tbs-fpv/tbs-crsf-spec/blob/main/crsf.md#0x32-direct-commands specifies an additional 8-bit CRC for command payloads, but ELRS ignores this so it isn't necessary to implement.
