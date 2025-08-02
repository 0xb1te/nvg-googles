#include "bt656_example.h"
#include <Arduino.h>

// ============================================================================
// Global Variables
// ============================================================================

// Global frame buffer instance
static frame_buffer_t g_frame_buffer;

// Global processing configuration
static video_processing_config_t g_processing_config = DEFAULT_PROCESSING_CONFIG;

// Frame processing statistics
static uint32_t g_total_frames_processed = 0;
static uint32_t g_total_pixels_processed = 0;
static uint64_t g_last_frame_time = 0;

// ============================================================================
// Frame Buffer Functions
// ============================================================================

bool frame_buffer_init(frame_buffer_t* buffer, uint16_t width, uint16_t height) {
    if (!buffer) {
        Serial.println("ERROR: Invalid buffer pointer");
        return false;
    }
    
    // Initialize buffer structure
    memset(buffer, 0, sizeof(frame_buffer_t));
    buffer->width = width;
    buffer->height = height;
    buffer->format = FRAME_FORMAT_YCBCR;
    
    // Calculate buffer sizes
    size_t ycbcr_size = width * height * 3;  // Y, Cb, Cr for each pixel
    size_t rgb_size = width * height * 3;    // R, G, B for each pixel
    size_t rgb565_size = width * height * 2; // RGB565 for each pixel
    size_t gray_size = width * height;       // Grayscale for each pixel
    
    // Allocate buffers
    buffer->ycbcr_buffer = (uint8_t*)malloc(ycbcr_size);
    buffer->rgb_buffer = (uint8_t*)malloc(rgb_size);
    buffer->rgb565_buffer = (uint16_t*)malloc(rgb565_size);
    buffer->gray_buffer = (uint8_t*)malloc(gray_size);
    
    // Check allocation
    if (!buffer->ycbcr_buffer || !buffer->rgb_buffer || 
        !buffer->rgb565_buffer || !buffer->gray_buffer) {
        Serial.println("ERROR: Failed to allocate frame buffers");
        frame_buffer_deinit(buffer);
        return false;
    }
    
    // Clear buffers
    memset(buffer->ycbcr_buffer, 0, ycbcr_size);
    memset(buffer->rgb_buffer, 0, rgb_size);
    memset(buffer->rgb565_buffer, 0, rgb565_size);
    memset(buffer->gray_buffer, 0, gray_size);
    
    Serial.printf("Frame buffer initialized: %dx%d\n", width, height);
    Serial.printf("YCbCr buffer: %d bytes\n", ycbcr_size);
    Serial.printf("RGB buffer: %d bytes\n", rgb_size);
    Serial.printf("RGB565 buffer: %d bytes\n", rgb565_size);
    Serial.printf("Grayscale buffer: %d bytes\n", gray_size);
    
    return true;
}

void frame_buffer_deinit(frame_buffer_t* buffer) {
    if (!buffer) return;
    
    // Free allocated buffers
    if (buffer->ycbcr_buffer) {
        free(buffer->ycbcr_buffer);
        buffer->ycbcr_buffer = nullptr;
    }
    
    if (buffer->rgb_buffer) {
        free(buffer->rgb_buffer);
        buffer->rgb_buffer = nullptr;
    }
    
    if (buffer->rgb565_buffer) {
        free(buffer->rgb565_buffer);
        buffer->rgb565_buffer = nullptr;
    }
    
    if (buffer->gray_buffer) {
        free(buffer->gray_buffer);
        buffer->gray_buffer = nullptr;
    }
    
    // Reset buffer structure
    memset(buffer, 0, sizeof(frame_buffer_t));
    
    Serial.println("Frame buffer deinitialized");
}

void frame_buffer_reset(frame_buffer_t* buffer) {
    if (!buffer) return;
    
    buffer->frame_complete = false;
    buffer->frame_ready = false;
    buffer->pixels_received = 0;
    buffer->lines_received = 0;
    buffer->frame_errors = 0;
    
    // Clear buffers
    if (buffer->ycbcr_buffer) {
        memset(buffer->ycbcr_buffer, 0, buffer->width * buffer->height * 3);
    }
    if (buffer->rgb_buffer) {
        memset(buffer->rgb_buffer, 0, buffer->width * buffer->height * 3);
    }
    if (buffer->rgb565_buffer) {
        memset(buffer->rgb565_buffer, 0, buffer->width * buffer->height * 2);
    }
    if (buffer->gray_buffer) {
        memset(buffer->gray_buffer, 0, buffer->width * buffer->height);
    }
}

bool frame_buffer_is_ready(frame_buffer_t* buffer) {
    return buffer ? buffer->frame_ready : false;
}

// ============================================================================
// Frame Buffer Access Functions
// ============================================================================

uint8_t* frame_buffer_get_ycbcr(frame_buffer_t* buffer) {
    return buffer ? buffer->ycbcr_buffer : nullptr;
}

uint8_t* frame_buffer_get_rgb(frame_buffer_t* buffer) {
    return buffer ? buffer->rgb_buffer : nullptr;
}

uint16_t* frame_buffer_get_rgb565(frame_buffer_t* buffer) {
    return buffer ? buffer->rgb565_buffer : nullptr;
}

uint8_t* frame_buffer_get_gray(frame_buffer_t* buffer) {
    return buffer ? buffer->gray_buffer : nullptr;
}

// ============================================================================
// Video Processing Functions
// ============================================================================

bool video_processing_init(const video_processing_config_t* config) {
    if (config) {
        g_processing_config = *config;
    }
    
    // Initialize frame buffer
    if (!frame_buffer_init(&g_frame_buffer, FRAME_WIDTH, FRAME_HEIGHT)) {
        Serial.println("ERROR: Failed to initialize frame buffer");
        return false;
    }
    
    Serial.println("Video processing initialized successfully");
    return true;
}

void video_processing_deinit(void) {
    frame_buffer_deinit(&g_frame_buffer);
    Serial.println("Video processing deinitialized");
}

void video_processing_process_frame(frame_buffer_t* buffer) {
    if (!buffer || !buffer->frame_ready) return;
    
    g_total_frames_processed++;
    g_total_pixels_processed += buffer->pixels_received;
    g_last_frame_time = micros();
    
    // Process frame based on configuration
    switch (g_processing_config.process_mode) {
        case PROCESS_MODE_DISPLAY:
            // Process for display
            if (g_processing_config.enable_debug) {
                Serial.printf("Processing frame %lu for display\n", buffer->frame_number);
            }
            break;
            
        case PROCESS_MODE_SAVE:
            // Process for saving
            if (g_processing_config.enable_debug) {
                Serial.printf("Processing frame %lu for saving\n", buffer->frame_number);
            }
            break;
            
        case PROCESS_MODE_STREAM:
            // Process for streaming
            if (g_processing_config.enable_debug) {
                Serial.printf("Processing frame %lu for streaming\n", buffer->frame_number);
            }
            break;
            
        default:
            // No processing
            break;
    }
    
    // Mark frame as processed
    buffer->frame_ready = false;
}

void video_processing_set_config(const video_processing_config_t* config) {
    if (config) {
        g_processing_config = *config;
        Serial.println("Video processing configuration updated");
    }
}

// ============================================================================
// BT656 Decoder Callback Functions
// ============================================================================

void example_ycbcr_callback(bt656_ycbcr_t* pixel, uint16_t x, uint16_t y) {
    if (!pixel || x >= g_frame_buffer.width || y >= g_frame_buffer.height) {
        return;
    }
    
    // Calculate buffer index
    uint32_t index = (y * g_frame_buffer.width + x) * 3;
    
    // Store YCbCr pixel in buffer
    if (g_frame_buffer.ycbcr_buffer) {
        g_frame_buffer.ycbcr_buffer[index + 0] = pixel->y;
        g_frame_buffer.ycbcr_buffer[index + 1] = pixel->cb;
        g_frame_buffer.ycbcr_buffer[index + 2] = pixel->cr;
    }
    
    // Convert to RGB and store
    bt656_rgb_t rgb = bt656_ycbcr_to_rgb(*pixel);
    if (g_frame_buffer.rgb_buffer) {
        g_frame_buffer.rgb_buffer[index + 0] = rgb.r;
        g_frame_buffer.rgb_buffer[index + 1] = rgb.g;
        g_frame_buffer.rgb_buffer[index + 2] = rgb.b;
    }
    
    // Convert to RGB565 and store
    if (g_frame_buffer.rgb565_buffer) {
        uint32_t rgb565_index = y * g_frame_buffer.width + x;
        g_frame_buffer.rgb565_buffer[rgb565_index] = bt656_rgb_to_rgb565(rgb);
    }
    
    // Convert to grayscale and store
    if (g_frame_buffer.gray_buffer) {
        uint32_t gray_index = y * g_frame_buffer.width + x;
        g_frame_buffer.gray_buffer[gray_index] = bt656_ycbcr_to_grayscale(*pixel);
    }
    
    g_frame_buffer.pixels_received++;
}

void example_rgb_callback(bt656_rgb_t* pixel, uint16_t x, uint16_t y) {
    if (!pixel || x >= g_frame_buffer.width || y >= g_frame_buffer.height) {
        return;
    }
    
    // Calculate buffer index
    uint32_t index = (y * g_frame_buffer.width + x) * 3;
    
    // Store RGB pixel in buffer
    if (g_frame_buffer.rgb_buffer) {
        g_frame_buffer.rgb_buffer[index + 0] = pixel->r;
        g_frame_buffer.rgb_buffer[index + 1] = pixel->g;
        g_frame_buffer.rgb_buffer[index + 2] = pixel->b;
    }
    
    // Convert to RGB565 and store
    if (g_frame_buffer.rgb565_buffer) {
        uint32_t rgb565_index = y * g_frame_buffer.width + x;
        g_frame_buffer.rgb565_buffer[rgb565_index] = bt656_rgb_to_rgb565(*pixel);
    }
    
    // Convert to grayscale and store
    if (g_frame_buffer.gray_buffer) {
        uint32_t gray_index = y * g_frame_buffer.width + x;
        g_frame_buffer.gray_buffer[gray_index] = (pixel->r + pixel->g + pixel->b) / 3;
    }
}

void example_frame_callback(void) {
    g_frame_buffer.frame_number++;
    g_frame_buffer.timestamp = micros();
    g_frame_buffer.frame_complete = true;
    g_frame_buffer.frame_ready = true;
    
    if (g_processing_config.enable_debug) {
        Serial.printf("Frame %lu complete: %lu pixels, %lu lines\n", 
                     g_frame_buffer.frame_number,
                     g_frame_buffer.pixels_received,
                     g_frame_buffer.lines_received);
    }
    
    // Process the frame
    if (g_processing_config.enable_processing) {
        video_processing_process_frame(&g_frame_buffer);
    }
    
    // Reset frame buffer for next frame
    frame_buffer_reset(&g_frame_buffer);
}

void example_line_callback(uint16_t line_number) {
    g_frame_buffer.lines_received++;
    
    if (g_processing_config.enable_debug && line_number % 100 == 0) {
        Serial.printf("Line %d received\n", line_number);
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

void example_print_frame_info(frame_buffer_t* buffer) {
    if (!buffer) return;
    
    Serial.println("=== Frame Information ===");
    Serial.printf("Frame Number: %lu\n", buffer->frame_number);
    Serial.printf("Frame Size: %dx%d\n", buffer->width, buffer->height);
    Serial.printf("Pixels Received: %lu\n", buffer->pixels_received);
    Serial.printf("Lines Received: %lu\n", buffer->lines_received);
    Serial.printf("Frame Complete: %s\n", buffer->frame_complete ? "YES" : "NO");
    Serial.printf("Frame Ready: %s\n", buffer->frame_ready ? "YES" : "NO");
    Serial.printf("Frame Errors: %lu\n", buffer->frame_errors);
    Serial.printf("Timestamp: %llu us\n", buffer->timestamp);
    Serial.println("========================");
}

void example_save_frame_to_file(frame_buffer_t* buffer, const char* filename) {
    if (!buffer || !filename) return;
    
    // This is a placeholder for file saving functionality
    // In a real implementation, you would save the frame data to a file
    Serial.printf("Saving frame %lu to %s\n", buffer->frame_number, filename);
    
    // Example: Save RGB565 data
    if (buffer->rgb565_buffer) {
        Serial.printf("RGB565 data available: %d bytes\n", 
                     buffer->width * buffer->height * 2);
    }
}

void example_display_frame_statistics(frame_buffer_t* buffer) {
    if (!buffer) return;
    
    Serial.println("=== Frame Statistics ===");
    Serial.printf("Total Frames Processed: %lu\n", g_total_frames_processed);
    Serial.printf("Total Pixels Processed: %lu\n", g_total_pixels_processed);
    Serial.printf("Current Frame: %lu\n", buffer->frame_number);
    Serial.printf("Current Pixels: %lu\n", buffer->pixels_received);
    Serial.printf("Current Lines: %lu\n", buffer->lines_received);
    
    // Calculate frame rate
    if (g_total_frames_processed > 1) {
        uint64_t time_diff = micros() - g_last_frame_time;
        float frame_rate = 1000000.0f / time_diff;
        Serial.printf("Estimated Frame Rate: %.2f fps\n", frame_rate);
    }
    
    // Calculate memory usage
    size_t total_memory = 0;
    if (buffer->ycbcr_buffer) total_memory += buffer->width * buffer->height * 3;
    if (buffer->rgb_buffer) total_memory += buffer->width * buffer->height * 3;
    if (buffer->rgb565_buffer) total_memory += buffer->width * buffer->height * 2;
    if (buffer->gray_buffer) total_memory += buffer->width * buffer->height;
    
    Serial.printf("Total Memory Usage: %d bytes\n", total_memory);
    Serial.println("========================");
} 