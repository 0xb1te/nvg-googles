#ifndef BT656_EXAMPLE_H
#define BT656_EXAMPLE_H

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>
#include "bt656_decoder.h"
#include "bt656_interface.h"

// ============================================================================
// Frame Buffer Configuration
// ============================================================================

// Frame buffer dimensions (PAL)
#define FRAME_WIDTH    720
#define FRAME_HEIGHT   576
#define FRAME_SIZE     (FRAME_WIDTH * FRAME_HEIGHT)

// Frame buffer formats
#define FRAME_FORMAT_YCBCR    0
#define FRAME_FORMAT_RGB      1
#define FRAME_FORMAT_RGB565   2
#define FRAME_FORMAT_GRAY     3

// Frame buffer structure
typedef struct {
    uint8_t* ycbcr_buffer;        // YCbCr frame buffer
    uint8_t* rgb_buffer;          // RGB frame buffer
    uint16_t* rgb565_buffer;      // RGB565 frame buffer
    uint8_t* gray_buffer;         // Grayscale frame buffer
    
    uint16_t width;               // Frame width
    uint16_t height;              // Frame height
    uint8_t format;               // Current format
    uint32_t frame_number;        // Frame number
    uint64_t timestamp;           // Frame timestamp
    bool frame_complete;          // Frame complete flag
    bool frame_ready;             // Frame ready for processing
    
    // Statistics
    uint32_t pixels_received;     // Pixels received in current frame
    uint32_t lines_received;      // Lines received in current frame
    uint32_t frame_errors;        // Frame errors
} frame_buffer_t;

// ============================================================================
// Video Processing Configuration
// ============================================================================

// Processing modes
#define PROCESS_MODE_NONE      0
#define PROCESS_MODE_DISPLAY   1
#define PROCESS_MODE_SAVE      2
#define PROCESS_MODE_STREAM    3

// Video processing configuration
typedef struct {
    uint8_t process_mode;         // Processing mode
    uint8_t output_format;        // Output format
    bool enable_processing;       // Enable video processing
    bool enable_statistics;       // Enable statistics
    bool enable_debug;            // Enable debug output
    
    // Processing parameters
    uint8_t brightness;           // Brightness adjustment
    uint8_t contrast;             // Contrast adjustment
    uint8_t saturation;           // Saturation adjustment
    
    // Output parameters
    uint16_t output_width;        // Output width
    uint16_t output_height;       // Output height
    uint8_t output_fps;           // Output frame rate
} video_processing_config_t;

// ============================================================================
// Function Prototypes
// ============================================================================

// Frame buffer functions
bool frame_buffer_init(frame_buffer_t* buffer, uint16_t width, uint16_t height);
void frame_buffer_deinit(frame_buffer_t* buffer);
void frame_buffer_reset(frame_buffer_t* buffer);
bool frame_buffer_is_ready(frame_buffer_t* buffer);

// Frame buffer access functions
uint8_t* frame_buffer_get_ycbcr(frame_buffer_t* buffer);
uint8_t* frame_buffer_get_rgb(frame_buffer_t* buffer);
uint16_t* frame_buffer_get_rgb565(frame_buffer_t* buffer);
uint8_t* frame_buffer_get_gray(frame_buffer_t* buffer);

// Video processing functions
bool video_processing_init(const video_processing_config_t* config);
void video_processing_deinit(void);
void video_processing_process_frame(frame_buffer_t* buffer);
void video_processing_set_config(const video_processing_config_t* config);

// Callback functions for BT656 decoder
void example_ycbcr_callback(bt656_ycbcr_t* pixel, uint16_t x, uint16_t y);
void example_rgb_callback(bt656_rgb_t* pixel, uint16_t x, uint16_t y);
void example_frame_callback(void);
void example_line_callback(uint16_t line_number);

// Utility functions
void example_print_frame_info(frame_buffer_t* buffer);
void example_save_frame_to_file(frame_buffer_t* buffer, const char* filename);
void example_display_frame_statistics(frame_buffer_t* buffer);

// ============================================================================
// Default Configurations
// ============================================================================

// Default frame buffer configuration
static const video_processing_config_t DEFAULT_PROCESSING_CONFIG = {
    .process_mode = PROCESS_MODE_DISPLAY,
    .output_format = FRAME_FORMAT_RGB565,
    .enable_processing = true,
    .enable_statistics = true,
    .enable_debug = false,
    .brightness = 128,
    .contrast = 128,
    .saturation = 128,
    .output_width = FRAME_WIDTH,
    .output_height = FRAME_HEIGHT,
    .output_fps = 25
};

#endif // BT656_EXAMPLE_H 