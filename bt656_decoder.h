#ifndef BT656_DECODER_H
#define BT656_DECODER_H

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// BT656 Decoder Configuration
// ============================================================================

// BT656 timing constants
#define BT656_CLOCK_FREQ_HZ        27000000  // 27 MHz
#define BT656_PAL_LINES            625       // PAL total lines
#define BT656_PAL_ACTIVE_LINES     576       // PAL active video lines
#define BT656_PAL_ACTIVE_PIXELS    720       // PAL active pixels per line
#define BT656_PAL_TOTAL_PIXELS     864       // PAL total pixels per line

// BT656 data stream markers
#define BT656_TR_MARKER_FF         0xFF      // Timing reference marker
#define BT656_TR_MARKER_00         0x00      // Timing reference marker
#define BT656_SAV_MARKER           0x80      // Start of Active Video
#define BT656_EAV_MARKER           0x9D      // End of Active Video

// BT656 sync signal bit positions
#define BT656_FIELD_BIT            6         // Field indicator bit
#define BT656_VSYNC_BIT            5         // Vertical sync bit
#define BT656_HSYNC_BIT            4         // Horizontal sync bit
#define BT656_SAV_BIT              3         // SAV indicator bit

// ============================================================================
// Data Structures
// ============================================================================

// BT656 decoder state machine
typedef enum {
    BT656_STATE_IDLE,              // Waiting for timing reference
    BT656_STATE_FF,                // Found first FF
    BT656_STATE_FF00,              // Found FF 00
    BT656_STATE_FF0000,            // Found FF 00 00
    BT656_STATE_CONTROL_BYTE,      // Processing control byte
    BT656_STATE_ACTIVE_VIDEO       // Processing active video data
} bt656_state_t;

// Data phase for 4:2:2 YCbCr format
typedef enum {
    BT656_PHASE_Y1,                // First Y sample
    BT656_PHASE_CB,                // Cb sample
    BT656_PHASE_Y2,                // Second Y sample
    BT656_PHASE_CR                 // Cr sample
} bt656_data_phase_t;

// BT656 sync signals
typedef struct {
    bool field;                    // Field indicator (odd/even)
    bool vsync;                    // Vertical sync
    bool hsync;                    // Horizontal sync
    bool sav;                      // Start of Active Video
    bool eav;                      // End of Active Video
} bt656_sync_t;

// YCbCr pixel data
typedef struct {
    uint8_t y;                     // Luminance
    uint8_t cb;                    // Chrominance Blue
    uint8_t cr;                    // Chrominance Red
} bt656_ycbcr_t;

// RGB pixel data
typedef struct {
    uint8_t r;                     // Red component
    uint8_t g;                     // Green component
    uint8_t b;                     // Blue component
} bt656_rgb_t;

// BT656 decoder statistics
typedef struct {
    uint32_t frames_received;      // Total frames received
    uint32_t lines_received;       // Total lines received
    uint32_t pixels_received;      // Total pixels received
    uint32_t timing_errors;        // Timing reference errors
    uint32_t sync_errors;          // Sync signal errors
    uint32_t data_errors;          // Data phase errors
    uint64_t last_frame_time;      // Timestamp of last frame
} bt656_stats_t;

// BT656 decoder configuration
typedef struct {
    uint16_t expected_width;       // Expected video width
    uint16_t expected_height;      // Expected video height
    bool enable_rgb_conversion;    // Enable YCbCr to RGB conversion
    bool enable_frame_buffer;      // Enable frame buffering
    uint8_t output_format;         // 0=YCbCr, 1=RGB, 2=Grayscale
} bt656_config_t;

// BT656 decoder instance
typedef struct {
    bt656_state_t state;           // Current state
    bt656_data_phase_t phase;      // Current data phase
    bt656_sync_t sync;             // Current sync signals
    bt656_ycbcr_t current_pixel;   // Current YCbCr pixel
    bt656_rgb_t current_rgb;       // Current RGB pixel
    
    uint16_t line_count;           // Current line number
    uint16_t pixel_count;          // Current pixel in line
    uint16_t frame_width;          // Detected frame width
    uint16_t frame_height;         // Detected frame height
    
    bool in_active_video;          // Currently in active video region
    bool frame_started;            // Frame has started
    bool line_started;             // Line has started
    
    bt656_stats_t stats;           // Decoder statistics
    bt656_config_t config;         // Decoder configuration
    
    // Callback functions
    void (*pixel_callback)(bt656_ycbcr_t* pixel, uint16_t x, uint16_t y);
    void (*rgb_callback)(bt656_rgb_t* pixel, uint16_t x, uint16_t y);
    void (*frame_callback)(void);
    void (*line_callback)(uint16_t line_number);
} bt656_decoder_t;

// ============================================================================
// Function Prototypes
// ============================================================================

// Core BT656 decoder functions
bool bt656_decoder_init(bt656_decoder_t* decoder, const bt656_config_t* config);
void bt656_decoder_deinit(bt656_decoder_t* decoder);
void bt656_decoder_reset(bt656_decoder_t* decoder);
void bt656_decoder_process_byte(bt656_decoder_t* decoder, uint8_t data);

// Configuration functions
void bt656_decoder_set_config(bt656_decoder_t* decoder, const bt656_config_t* config);
void bt656_decoder_set_pixel_callback(bt656_decoder_t* decoder, void (*callback)(bt656_ycbcr_t* pixel, uint16_t x, uint16_t y));
void bt656_decoder_set_rgb_callback(bt656_decoder_t* decoder, void (*callback)(bt656_rgb_t* pixel, uint16_t x, uint16_t y));
void bt656_decoder_set_frame_callback(bt656_decoder_t* decoder, void (*callback)(void));
void bt656_decoder_set_line_callback(bt656_decoder_t* decoder, void (*callback)(uint16_t line_number));

// Status and statistics functions
bt656_stats_t bt656_decoder_get_stats(bt656_decoder_t* decoder);
void bt656_decoder_reset_stats(bt656_decoder_t* decoder);
bool bt656_decoder_is_frame_active(bt656_decoder_t* decoder);
uint16_t bt656_decoder_get_current_line(bt656_decoder_t* decoder);
uint16_t bt656_decoder_get_current_pixel(bt656_decoder_t* decoder);

// Color space conversion functions
bt656_rgb_t bt656_ycbcr_to_rgb(bt656_ycbcr_t ycbcr);
uint8_t bt656_ycbcr_to_grayscale(bt656_ycbcr_t ycbcr);
uint16_t bt656_rgb_to_rgb565(bt656_rgb_t rgb);

// Utility functions
void bt656_decoder_print_stats(bt656_decoder_t* decoder);
const char* bt656_state_to_string(bt656_state_t state);
const char* bt656_phase_to_string(bt656_data_phase_t phase);

#endif // BT656_DECODER_H 