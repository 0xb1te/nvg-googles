#include "bt656_decoder.h"
#include <Arduino.h>

// ============================================================================
// Internal Helper Functions
// ============================================================================

// Detect timing reference pattern (FF 00 00)
static bool detect_timing_reference(bt656_decoder_t* decoder, uint8_t data) {
    switch(decoder->state) {
        case BT656_STATE_IDLE:
            if (data == BT656_TR_MARKER_FF) {
                decoder->state = BT656_STATE_FF;
            }
            break;
            
        case BT656_STATE_FF:
            if (data == BT656_TR_MARKER_00) {
                decoder->state = BT656_STATE_FF00;
            } else {
                decoder->state = BT656_STATE_IDLE;
            }
            break;
            
        case BT656_STATE_FF00:
            if (data == BT656_TR_MARKER_00) {
                decoder->state = BT656_STATE_FF0000;
                return true; // Timing reference found
            } else {
                decoder->state = BT656_STATE_IDLE;
            }
            break;
            
        case BT656_STATE_FF0000:
            decoder->state = BT656_STATE_CONTROL_BYTE;
            break;
            
        default:
            decoder->state = BT656_STATE_IDLE;
            break;
    }
    return false;
}

// Extract sync signals from control byte
static bt656_sync_t extract_sync_signals(uint8_t control_byte) {
    bt656_sync_t sync;
    sync.field = (control_byte & (1 << BT656_FIELD_BIT)) != 0;
    sync.vsync = (control_byte & (1 << BT656_VSYNC_BIT)) != 0;
    sync.hsync = (control_byte & (1 << BT656_HSYNC_BIT)) != 0;
    sync.sav = (control_byte & (1 << BT656_SAV_BIT)) != 0;
    sync.eav = !sync.sav; // EAV is the opposite of SAV
    return sync;
}

// Process video data in 4:2:2 YCbCr format
static void process_video_data(bt656_decoder_t* decoder, uint8_t data) {
    if (!decoder->in_active_video) return;
    
    switch(decoder->phase) {
        case BT656_PHASE_Y1:
            decoder->current_pixel.y = data;
            decoder->phase = BT656_PHASE_CB;
            break;
            
        case BT656_PHASE_CB:
            decoder->current_pixel.cb = data;
            decoder->phase = BT656_PHASE_Y2;
            break;
            
        case BT656_PHASE_Y2:
            decoder->current_pixel.y = data; // Second Y sample
            decoder->phase = BT656_PHASE_CR;
            break;
            
        case BT656_PHASE_CR:
            decoder->current_pixel.cr = data;
            
            // Complete pixel pair available - call callbacks
            if (decoder->pixel_callback) {
                decoder->pixel_callback(&decoder->current_pixel, 
                                      decoder->pixel_count, decoder->line_count);
            }
            
            if (decoder->config.enable_rgb_conversion && decoder->rgb_callback) {
                decoder->current_rgb = bt656_ycbcr_to_rgb(decoder->current_pixel);
                decoder->rgb_callback(&decoder->current_rgb, 
                                    decoder->pixel_count, decoder->line_count);
            }
            
            decoder->stats.pixels_received++;
            decoder->pixel_count++;
            decoder->phase = BT656_PHASE_Y1;
            break;
    }
}

// Handle sync signal changes
static void handle_sync_signals(bt656_decoder_t* decoder, bt656_sync_t sync) {
    // Handle vertical sync (new frame)
    if (sync.vsync && !decoder->sync.vsync) {
        decoder->frame_started = true;
        decoder->line_count = 0;
        decoder->pixel_count = 0;
        decoder->stats.frames_received++;
        decoder->stats.last_frame_time = micros();
        
        if (decoder->frame_callback) {
            decoder->frame_callback();
        }
    }
    
    // Handle horizontal sync (new line)
    if (sync.hsync && !decoder->sync.hsync) {
        decoder->line_started = true;
        decoder->pixel_count = 0;
        decoder->stats.lines_received++;
        
        if (decoder->line_callback) {
            decoder->line_callback(decoder->line_count);
        }
        
        decoder->line_count++;
    }
    
    // Handle SAV (Start of Active Video)
    if (sync.sav) {
        decoder->in_active_video = true;
        decoder->phase = BT656_PHASE_Y1;
        decoder->pixel_count = 0;
    } else {
        decoder->in_active_video = false;
    }
    
    // Update current sync state
    decoder->sync = sync;
}

// ============================================================================
// Core BT656 Decoder Functions
// ============================================================================

bool bt656_decoder_init(bt656_decoder_t* decoder, const bt656_config_t* config) {
    if (!decoder) {
        Serial.println("ERROR: Invalid decoder pointer");
        return false;
    }
    
    // Initialize decoder structure
    memset(decoder, 0, sizeof(bt656_decoder_t));
    
    // Set configuration
    if (config) {
        decoder->config = *config;
    } else {
        // Default configuration for PAL
        decoder->config.expected_width = BT656_PAL_ACTIVE_PIXELS;
        decoder->config.expected_height = BT656_PAL_ACTIVE_LINES;
        decoder->config.enable_rgb_conversion = true;
        decoder->config.enable_frame_buffer = false;
        decoder->config.output_format = 1; // RGB
    }
    
    // Initialize state
    decoder->state = BT656_STATE_IDLE;
    decoder->phase = BT656_PHASE_Y1;
    decoder->in_active_video = false;
    decoder->frame_started = false;
    decoder->line_started = false;
    
    // Initialize callbacks to NULL
    decoder->pixel_callback = nullptr;
    decoder->rgb_callback = nullptr;
    decoder->frame_callback = nullptr;
    decoder->line_callback = nullptr;
    
    Serial.println("BT656 decoder initialized successfully");
    return true;
}

void bt656_decoder_deinit(bt656_decoder_t* decoder) {
    if (decoder) {
        // Reset decoder state
        bt656_decoder_reset(decoder);
        Serial.println("BT656 decoder deinitialized");
    }
}

void bt656_decoder_reset(bt656_decoder_t* decoder) {
    if (!decoder) return;
    
    decoder->state = BT656_STATE_IDLE;
    decoder->phase = BT656_PHASE_Y1;
    decoder->in_active_video = false;
    decoder->frame_started = false;
    decoder->line_started = false;
    decoder->line_count = 0;
    decoder->pixel_count = 0;
    
    // Clear sync signals
    memset(&decoder->sync, 0, sizeof(bt656_sync_t));
    
    // Clear current pixel data
    memset(&decoder->current_pixel, 0, sizeof(bt656_ycbcr_t));
    memset(&decoder->current_rgb, 0, sizeof(bt656_rgb_t));
}

void bt656_decoder_process_byte(bt656_decoder_t* decoder, uint8_t data) {
    if (!decoder) return;
    
    // Check for timing reference pattern
    if (detect_timing_reference(decoder, data)) {
        return; // Wait for control byte
    }
    
    // Process control byte after timing reference
    if (decoder->state == BT656_STATE_CONTROL_BYTE) {
        bt656_sync_t sync = extract_sync_signals(data);
        handle_sync_signals(decoder, sync);
        decoder->state = BT656_STATE_IDLE;
        return;
    }
    
    // Process video data if in active video region
    if (decoder->in_active_video) {
        process_video_data(decoder, data);
    }
}

// ============================================================================
// Configuration Functions
// ============================================================================

void bt656_decoder_set_config(bt656_decoder_t* decoder, const bt656_config_t* config) {
    if (decoder && config) {
        decoder->config = *config;
    }
}

void bt656_decoder_set_pixel_callback(bt656_decoder_t* decoder, void (*callback)(bt656_ycbcr_t* pixel, uint16_t x, uint16_t y)) {
    if (decoder) {
        decoder->pixel_callback = callback;
    }
}

void bt656_decoder_set_rgb_callback(bt656_decoder_t* decoder, void (*callback)(bt656_rgb_t* pixel, uint16_t x, uint16_t y)) {
    if (decoder) {
        decoder->rgb_callback = callback;
    }
}

void bt656_decoder_set_frame_callback(bt656_decoder_t* decoder, void (*callback)(void)) {
    if (decoder) {
        decoder->frame_callback = callback;
    }
}

void bt656_decoder_set_line_callback(bt656_decoder_t* decoder, void (*callback)(uint16_t line_number)) {
    if (decoder) {
        decoder->line_callback = callback;
    }
}

// ============================================================================
// Status and Statistics Functions
// ============================================================================

bt656_stats_t bt656_decoder_get_stats(bt656_decoder_t* decoder) {
    if (decoder) {
        return decoder->stats;
    }
    bt656_stats_t empty_stats = {0};
    return empty_stats;
}

void bt656_decoder_reset_stats(bt656_decoder_t* decoder) {
    if (decoder) {
        memset(&decoder->stats, 0, sizeof(bt656_stats_t));
    }
}

bool bt656_decoder_is_frame_active(bt656_decoder_t* decoder) {
    return decoder ? decoder->frame_started : false;
}

uint16_t bt656_decoder_get_current_line(bt656_decoder_t* decoder) {
    return decoder ? decoder->line_count : 0;
}

uint16_t bt656_decoder_get_current_pixel(bt656_decoder_t* decoder) {
    return decoder ? decoder->pixel_count : 0;
}

// ============================================================================
// Color Space Conversion Functions
// ============================================================================

bt656_rgb_t bt656_ycbcr_to_rgb(bt656_ycbcr_t ycbcr) {
    bt656_rgb_t rgb;
    
    // YCbCr to RGB conversion (BT.601 standard)
    int y = ycbcr.y - 16;
    int cb = ycbcr.cb - 128;
    int cr = ycbcr.cr - 128;
    
    // Convert using BT.601 coefficients
    int r = y + 1.402 * cr;
    int g = y - 0.344 * cb - 0.714 * cr;
    int b = y + 1.772 * cb;
    
    // Clamp values to 0-255 range
    rgb.r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    rgb.g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    rgb.b = (b < 0) ? 0 : (b > 255) ? 255 : b;
    
    return rgb;
}

uint8_t bt656_ycbcr_to_grayscale(bt656_ycbcr_t ycbcr) {
    // Simple Y component extraction (luminance)
    return ycbcr.y;
}

uint16_t bt656_rgb_to_rgb565(bt656_rgb_t rgb) {
    // Convert RGB888 to RGB565
    uint16_t r = (rgb.r >> 3) & 0x1F;
    uint16_t g = (rgb.g >> 2) & 0x3F;
    uint16_t b = (rgb.b >> 3) & 0x1F;
    
    return (r << 11) | (g << 5) | b;
}

// ============================================================================
// Utility Functions
// ============================================================================

void bt656_decoder_print_stats(bt656_decoder_t* decoder) {
    if (!decoder) return;
    
    Serial.println("=== BT656 Decoder Statistics ===");
    Serial.printf("Frames Received: %lu\n", decoder->stats.frames_received);
    Serial.printf("Lines Received: %lu\n", decoder->stats.lines_received);
    Serial.printf("Pixels Received: %lu\n", decoder->stats.pixels_received);
    Serial.printf("Timing Errors: %lu\n", decoder->stats.timing_errors);
    Serial.printf("Sync Errors: %lu\n", decoder->stats.sync_errors);
    Serial.printf("Data Errors: %lu\n", decoder->stats.data_errors);
    Serial.printf("Last Frame Time: %llu us\n", decoder->stats.last_frame_time);
    Serial.printf("Current State: %s\n", bt656_state_to_string(decoder->state));
    Serial.printf("Current Phase: %s\n", bt656_phase_to_string(decoder->phase));
    Serial.printf("In Active Video: %s\n", decoder->in_active_video ? "YES" : "NO");
    Serial.printf("Current Line: %d\n", decoder->line_count);
    Serial.printf("Current Pixel: %d\n", decoder->pixel_count);
    Serial.println("================================");
}

const char* bt656_state_to_string(bt656_state_t state) {
    switch(state) {
        case BT656_STATE_IDLE: return "IDLE";
        case BT656_STATE_FF: return "FF";
        case BT656_STATE_FF00: return "FF00";
        case BT656_STATE_FF0000: return "FF0000";
        case BT656_STATE_CONTROL_BYTE: return "CONTROL_BYTE";
        case BT656_STATE_ACTIVE_VIDEO: return "ACTIVE_VIDEO";
        default: return "UNKNOWN";
    }
}

const char* bt656_phase_to_string(bt656_data_phase_t phase) {
    switch(phase) {
        case BT656_PHASE_Y1: return "Y1";
        case BT656_PHASE_CB: return "CB";
        case BT656_PHASE_Y2: return "Y2";
        case BT656_PHASE_CR: return "CR";
        default: return "UNKNOWN";
    }
} 