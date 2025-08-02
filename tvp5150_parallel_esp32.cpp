// File: tvp5150_parallel_esp32.cpp
// Implementation of TVP5150 parallel interface for ESP32

#include "tvp5150_parallel_esp32.h"
#include <Arduino.h>

// Global variables
static tvp5150_pins_t current_pins;
static bool parallel_initialized = false;
static bool capturing = false;
static uint32_t frame_count = 0;
static void (*frame_callback)(video_frame_t* frame) = nullptr;

// Video capture configuration
static video_config_t current_config = {0};

// Frame buffer (if needed)
static uint8_t* frame_buffer = nullptr;
static size_t frame_buffer_size = 0;

// ============================================================================
// Pin Management Functions
// ============================================================================

// Check if pin is connected (not 255)
static bool is_pin_connected(uint8_t pin) {
    return pin != 255;
}

// Initialize GPIO pins for parallel interface
static bool init_gpio_pins(const tvp5150_pins_t* pins) {
    if (!pins) {
        Serial.println("ERROR: Invalid pin configuration");
        return false;
    }
    
    // Note: Data pins (D0-D7) are configured by BT656 interface to avoid conflicts
    // The parallel interface only reads data pins, it doesn't configure them
    
    // Configure control pins as inputs
    if (is_pin_connected(pins->vsync_pin)) {
        pinMode(pins->vsync_pin, INPUT);
        Serial.printf("VSYNC pin configured: GPIO %d\n", pins->vsync_pin);
    }
    
    if (is_pin_connected(pins->href_pin)) {
        pinMode(pins->href_pin, INPUT);
        Serial.printf("HREF pin configured: GPIO %d\n", pins->href_pin);
    }
    
    // Note: PCLK pin is configured by BT656 interface to avoid conflicts
    // The parallel interface only reads PCLK, it doesn't configure it
    
    return true;
}

// ============================================================================
// Data Reading Functions
// ============================================================================

// Read 8-bit data from parallel bus
// Data pins are configured by BT656 interface, but we can still read them
static uint8_t read_parallel_data(const tvp5150_pins_t* pins) {
    uint8_t data = 0;
    
    if (is_pin_connected(pins->d0_pin)) data |= (digitalRead(pins->d0_pin) << 0);
    if (is_pin_connected(pins->d1_pin)) data |= (digitalRead(pins->d1_pin) << 1);
    if (is_pin_connected(pins->d2_pin)) data |= (digitalRead(pins->d2_pin) << 2);
    if (is_pin_connected(pins->d3_pin)) data |= (digitalRead(pins->d3_pin) << 3);
    if (is_pin_connected(pins->d4_pin)) data |= (digitalRead(pins->d4_pin) << 4);
    if (is_pin_connected(pins->d5_pin)) data |= (digitalRead(pins->d5_pin) << 5);
    if (is_pin_connected(pins->d6_pin)) data |= (digitalRead(pins->d6_pin) << 6);
    if (is_pin_connected(pins->d7_pin)) data |= (digitalRead(pins->d7_pin) << 7);
    
    return data;
}

// Read control signals
static bool read_vsync(const tvp5150_pins_t* pins) {
    if (is_pin_connected(pins->vsync_pin)) {
        return digitalRead(pins->vsync_pin) == HIGH;
    }
    return false;
}

static bool read_href(const tvp5150_pins_t* pins) {
    if (is_pin_connected(pins->href_pin)) {
        return digitalRead(pins->href_pin) == HIGH;
    }
    return false;
}

static bool read_pclk(const tvp5150_pins_t* pins) {
    // PCLK pin is configured by BT656 interface, but we can still read it
    if (is_pin_connected(pins->pclk_pin)) {
        return digitalRead(pins->pclk_pin) == HIGH;
    }
    return false;
}

// ============================================================================
// Public Interface Functions
// ============================================================================

bool tvp5150_parallel_init(const tvp5150_pins_t* pins) {
    Serial.println("Initializing TVP5150 parallel interface...");
    
    if (!pins) {
        Serial.println("ERROR: Invalid pin configuration");
        return false;
    }
    
    // Store pin configuration
    memcpy(&current_pins, pins, sizeof(tvp5150_pins_t));
    
    // Initialize GPIO pins (only VSYNC/HREF - data pins configured by BT656 interface)
    if (!init_gpio_pins(pins)) {
        Serial.println("ERROR: Failed to initialize GPIO pins");
        return false;
    }
    
    // Reset state
    capturing = false;
    frame_count = 0;
    frame_callback = nullptr;
    
    // Clear configuration
    memset(&current_config, 0, sizeof(current_config));
    
    parallel_initialized = true;
    Serial.println("TVP5150 parallel interface initialized successfully");
    return true;
}

void tvp5150_parallel_deinit(void) {
    Serial.println("Deinitializing TVP5150 parallel interface...");
    
    // Stop capture if running
    if (capturing) {
        tvp5150_stop_capture();
    }
    
    // Free frame buffer
    if (frame_buffer) {
        free(frame_buffer);
        frame_buffer = nullptr;
        frame_buffer_size = 0;
    }
    
    // Reset state
    parallel_initialized = false;
    capturing = false;
    frame_count = 0;
    frame_callback = nullptr;
    
    Serial.println("TVP5150 parallel interface deinitialized");
}

bool tvp5150_capture_frame(video_frame_t* frame) {
    if (!parallel_initialized || !frame) {
        return false;
    }
    
    // Simple frame capture implementation
    // This is a basic implementation - you may need to enhance it based on your needs
    
    // Check if VSYNC is active (frame start)
    if (is_pin_connected(current_pins.vsync_pin)) {
        bool vsync = read_vsync(&current_pins);
        if (vsync) {
            Serial.println("VSYNC detected - frame start");
        }
    }
    
    // Read some sample data (simplified)
    uint8_t sample_data[8];
    for (int i = 0; i < 8; i++) {
        sample_data[i] = read_parallel_data(&current_pins);
        delayMicroseconds(100); // Small delay
    }
    
    // Fill frame structure
    frame->width = current_config.width > 0 ? current_config.width : 640;
    frame->height = current_config.height > 0 ? current_config.height : 480;
    frame->frame_number = frame_count++;
    frame->timestamp = millis();
    
    // Allocate buffer if needed
    if (!frame->buffer) {
        frame->size = frame->width * frame->height * 2; // YUV422 = 2 bytes per pixel
        frame->buffer = (uint8_t*)malloc(frame->size);
        if (!frame->buffer) {
            Serial.println("ERROR: Failed to allocate frame buffer");
            return false;
        }
    }
    
    // Copy sample data (this is just a placeholder)
    // In a real implementation, you would capture the full frame
    memcpy(frame->buffer, sample_data, min(frame->size, (size_t)8));
    
    return true;
}

bool tvp5150_start_capture(const video_config_t* config) {
    if (!parallel_initialized) {
        Serial.println("ERROR: Parallel interface not initialized");
        return false;
    }
    
    if (!config) {
        Serial.println("ERROR: Invalid configuration");
        return false;
    }
    
    Serial.println("Starting video capture...");
    Serial.printf("Resolution: %dx%d\n", config->width, config->height);
    Serial.printf("Format: %d\n", config->format);
    Serial.printf("FPS: %d\n", config->fps);
    
    // Store configuration
    memcpy(&current_config, config, sizeof(video_config_t));
    
    // Allocate frame buffer
    if (config->width > 0 && config->height > 0) {
        frame_buffer_size = config->width * config->height * 2; // YUV422
        frame_buffer = (uint8_t*)malloc(frame_buffer_size);
        if (!frame_buffer) {
            Serial.println("ERROR: Failed to allocate frame buffer");
            return false;
        }
        Serial.printf("Frame buffer allocated: %d bytes\n", frame_buffer_size);
    }
    
    capturing = true;
    frame_count = 0;
    
    Serial.println("Video capture started");
    return true;
}

void tvp5150_stop_capture(void) {
    if (!parallel_initialized) {
        return;
    }
    
    Serial.println("Stopping video capture...");
    capturing = false;
    
    // Free frame buffer
    if (frame_buffer) {
        free(frame_buffer);
        frame_buffer = nullptr;
        frame_buffer_size = 0;
    }
    
    Serial.println("Video capture stopped");
}

bool tvp5150_is_capturing(void) {
    return capturing;
}

uint32_t tvp5150_get_frame_count(void) {
    return frame_count;
}

void tvp5150_set_callback(void (*callback)(video_frame_t* frame)) {
    frame_callback = callback;
}

// ============================================================================
// Utility Functions
// ============================================================================

void tvp5150_yuv422_to_rgb565(uint8_t* yuv_data, uint16_t* rgb_data, size_t pixel_count) {
    if (!yuv_data || !rgb_data) {
        return;
    }
    
    for (size_t i = 0; i < pixel_count; i++) {
        uint8_t y = yuv_data[i * 2];
        uint8_t u = yuv_data[i * 2 + 1];
        uint8_t v = yuv_data[i * 2 + 3];
        
        // Simple YUV to RGB conversion (simplified)
        int r = y + 1.402 * (v - 128);
        int g = y - 0.344 * (u - 128) - 0.714 * (v - 128);
        int b = y + 1.772 * (u - 128);
        
        // Clamp values
        r = constrain(r, 0, 255);
        g = constrain(g, 0, 255);
        b = constrain(b, 0, 255);
        
        // Convert to RGB565
        rgb_data[i] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }
}

void tvp5150_yuv422_to_grayscale(uint8_t* yuv_data, uint8_t* gray_data, size_t pixel_count) {
    if (!yuv_data || !gray_data) {
        return;
    }
    
    for (size_t i = 0; i < pixel_count; i++) {
        // Y component is already grayscale
        gray_data[i] = yuv_data[i * 2];
    }
}