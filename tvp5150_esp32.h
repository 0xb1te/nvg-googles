#ifndef TVP5150_ESP32_H
#define TVP5150_ESP32_H

#include <Arduino.h>
#include <Wire.h>
#include <stdint.h>
#include <stdbool.h>

// TVP5150 I2C Addresses (from Verilog code)
#define TVP5150_I2C_ADDR_PRIMARY   0x5D  // 0xBA >> 1 (7-bit address)
#define TVP5150_I2C_ADDR_SECONDARY 0x5C  // 0xB9 >> 1 (7-bit address)

// TVP5150 Register Addresses (from Verilog code)
// Video Standard Selection Registers
#define TVP5150_REG_VIDEO_STD_0A   0x0A
#define TVP5150_REG_VIDEO_STD_0B   0x0B
#define TVP5150_REG_VIDEO_STD_0C   0x0C
#define TVP5150_REG_VIDEO_STD_0D   0x0D  // PAL = 0x47
#define TVP5150_REG_VIDEO_STD_0E   0x0E
#define TVP5150_REG_VIDEO_STD_0F   0x0F
#define TVP5150_REG_VIDEO_STD_11   0x11
#define TVP5150_REG_VIDEO_STD_12   0x12
#define TVP5150_REG_VIDEO_STD_13   0x13
#define TVP5150_REG_VIDEO_STD_14   0x14
#define TVP5150_REG_VIDEO_STD_15   0x15
#define TVP5150_REG_VIDEO_STD_16   0x16
#define TVP5150_REG_VIDEO_STD_18   0x18
#define TVP5150_REG_VIDEO_STD_19   0x19
#define TVP5150_REG_VIDEO_STD_1A   0x1A
#define TVP5150_REG_VIDEO_STD_1B   0x1B
#define TVP5150_REG_VIDEO_STD_1C   0x1C
#define TVP5150_REG_VIDEO_STD_1D   0x1D
#define TVP5150_REG_VIDEO_STD_1E   0x1E
#define TVP5150_REG_VIDEO_STD_28   0x28

// Advanced Configuration Registers
#define TVP5150_REG_ADV_B1         0xB1
#define TVP5150_REG_ADV_B2         0xB2
#define TVP5150_REG_ADV_B3         0xB3
#define TVP5150_REG_ADV_B4         0xB4
#define TVP5150_REG_ADV_B5         0xB5
#define TVP5150_REG_ADV_B6         0xB6
#define TVP5150_REG_ADV_B7         0xB7
#define TVP5150_REG_ADV_B8         0xB8
#define TVP5150_REG_ADV_B9         0xB9
#define TVP5150_REG_ADV_BA         0xBA
#define TVP5150_REG_ADV_BB         0xBB

#define TVP5150_REG_ADV_C0         0xC0
#define TVP5150_REG_ADV_C1         0xC1
#define TVP5150_REG_ADV_C2         0xC2  // Value: 0x04
#define TVP5150_REG_ADV_C3         0xC3  // Value: 0xDC
#define TVP5150_REG_ADV_C4         0xC4  // Value: 0x0F
#define TVP5150_REG_ADV_C5         0xC5
#define TVP5150_REG_ADV_C8         0xC8
#define TVP5150_REG_ADV_C9         0xC9
#define TVP5150_REG_ADV_CA         0xCA
#define TVP5150_REG_ADV_CB         0xCB  // Value: 0x59
#define TVP5150_REG_ADV_CC         0xCC  // Value: 0x03
#define TVP5150_REG_ADV_CD         0xCD  // Value: 0x01
#define TVP5150_REG_ADV_CE         0xCE
#define TVP5150_REG_ADV_CF         0xCF

// Status Registers (for reading video status)
#define TVP5150_REG_STATUS_1       0x00
#define TVP5150_REG_STATUS_2       0x01
#define TVP5150_REG_STATUS_3       0x02
#define TVP5150_REG_STATUS_4       0x03
#define TVP5150_REG_STATUS_5       0x04
#define TVP5150_REG_STATUS_6       0x05
#define TVP5150_REG_STATUS_7       0x06
#define TVP5150_REG_STATUS_8       0x07
#define TVP5150_REG_STATUS_9       0x08
#define TVP5150_REG_STATUS_A       0x09
#define TVP5150_REG_STATUS_B       0x0A
#define TVP5150_REG_STATUS_C       0x0B
#define TVP5150_REG_STATUS_D       0x0C
#define TVP5150_REG_STATUS_E       0x0D
#define TVP5150_REG_STATUS_F       0x0E

// Video Processing Registers
#define TVP5150_REG_BRIGHTNESS     0x50
#define TVP5150_REG_CONTRAST       0x51
#define TVP5150_REG_SATURATION     0x52
#define TVP5150_REG_HUE            0x53

// Data structures
typedef struct {
    uint8_t y;
    uint8_t cb;
    uint8_t cr;
} yuv_pixel_t;

typedef struct {
    uint8_t status;
    uint16_t line;
    uint16_t frame_count;
    uint16_t buffer_count;
    bool vsync;
    bool hsync;
    bool field;
    bool video_present;
} tvp5150_status_t;

// Function prototypes
bool tvp5150_init(uint8_t sda_pin, uint8_t scl_pin);
void tvp5150_close(void);
yuv_pixel_t tvp5150_read_current_pixel(void);
tvp5150_status_t tvp5150_read_status(void);
bool tvp5150_read_frame_buffer(yuv_pixel_t* buffer, uint16_t max_pixels);
uint16_t tvp5150_get_available_pixels(void);
bool tvp5150_is_video_present(void);
void tvp5150_set_brightness(uint8_t brightness);
void tvp5150_set_contrast(uint8_t contrast);
void tvp5150_set_saturation(uint8_t saturation);

// Debug and verification functions
void tvp5150_print_critical_registers(void);
bool tvp5150_force_configure_verilog(void);
bool tvp5150_configure_pal(void);  // Configure for PAL video standard

// Register access functions
uint8_t tvp5150_read_register(uint8_t reg);
bool tvp5150_write_register(uint8_t reg, uint8_t data);

// Testing and debugging functions
bool tvp5150_test_input_selection(uint8_t input_sel);
bool tvp5150_reset_to_defaults(void);

// Video standard detection and configuration
bool tvp5150_auto_detect_video_standard(void);
bool tvp5150_configure_video_standard(bool is_pal);

// Camera connection and signal checking
void tvp5150_check_camera_connection(void);

#endif 