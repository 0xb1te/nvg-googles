#ifndef TVP5150_PARALLEL_ESP32_H
#define TVP5150_PARALLEL_ESP32_H

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

// Parallel interface pin definitions
typedef struct {
    uint8_t d0_pin;
    uint8_t d1_pin;
    uint8_t d2_pin;
    uint8_t d3_pin;
    uint8_t d4_pin;
    uint8_t d5_pin;
    uint8_t d6_pin;
    uint8_t d7_pin;
    uint8_t vsync_pin;
    uint8_t href_pin;
    uint8_t pclk_pin;
} tvp5150_pins_t;

// Video frame buffer
typedef struct {
    uint8_t* buffer;
    size_t size;
    uint16_t width;
    uint16_t height;
    uint32_t frame_number;
    uint64_t timestamp;
} video_frame_t;

// Video capture configuration
typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t format; // 0=YUV422, 1=RGB565, 2=Grayscale
    uint8_t fps;
} video_config_t;

// Function prototypes
bool tvp5150_parallel_init(const tvp5150_pins_t* pins);
void tvp5150_parallel_deinit(void);
bool tvp5150_capture_frame(video_frame_t* frame);
bool tvp5150_start_capture(const video_config_t* config);
void tvp5150_stop_capture(void);
bool tvp5150_is_capturing(void);
uint32_t tvp5150_get_frame_count(void);
void tvp5150_set_callback(void (*callback)(video_frame_t* frame));

// Utility functions
void tvp5150_yuv422_to_rgb565(uint8_t* yuv_data, uint16_t* rgb_data, size_t pixel_count);
void tvp5150_yuv422_to_grayscale(uint8_t* yuv_data, uint8_t* gray_data, size_t pixel_count);

#endif 