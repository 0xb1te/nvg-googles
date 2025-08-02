// Updated main_esp32.ino with BT656 decoder integration
#include "tvp5150_esp32.h"
#include "tvp5150_parallel_esp32.h"
#include "bt656_decoder.h"
#include "bt656_interface.h"
#include "pin_config.h"

// Status variables
bool tvp5150_connected = false;
bool parallel_interface_ready = false;
bool bt656_interface_ready = false;
unsigned long last_status_check = 0;
const unsigned long STATUS_CHECK_INTERVAL = 1000;

// BT656 decoder and interface instances
bt656_decoder_t bt656_decoder;
bt656_interface_t bt656_interface;

// Frame statistics
uint32_t total_frames_received = 0;
uint32_t total_pixels_received = 0;
uint64_t last_frame_timestamp = 0;

// ============================================================================
// Callback Functions
// ============================================================================

// Callback for YCbCr pixels from BT656 decoder
void on_ycbcr_pixel(bt656_ycbcr_t* pixel, uint16_t x, uint16_t y) {
    // Process YCbCr pixel data
    total_pixels_received++;
    
    // Optional: Store pixel data for processing
    // This is where you would add your pixel processing logic
}

// Callback for RGB pixels from BT656 decoder
void on_rgb_pixel(bt656_rgb_t* pixel, uint16_t x, uint16_t y) {
    // Process RGB pixel data
    // This is where you would add your RGB processing logic
    
    // Example: Convert to RGB565 for display
    uint16_t rgb565 = bt656_rgb_to_rgb565(*pixel);
    
    // Optional: Store or transmit the RGB565 pixel
}

// Callback for frame start from BT656 decoder
void on_frame_start() {
    total_frames_received++;
    last_frame_timestamp = micros();
    
    Serial.printf("Frame %lu started at %llu us\n", total_frames_received, last_frame_timestamp);
}

// Callback for line start from BT656 decoder
void on_line_start(uint16_t line_number) {
    // Optional: Process line start events
    if (line_number % 100 == 0) {
        Serial.printf("Line %d started\n", line_number);
    }
}

// Callback for data ready from BT656 interface
void on_data_ready(uint8_t* data, uint32_t count) {
    // Optional: Process raw BT656 data
    // This callback is called when data is available in the buffer
}

// Callback for errors from BT656 interface
void on_interface_error(uint32_t error_code) {
    Serial.printf("BT656 interface error: 0x%08X\n", error_code);
}

// ============================================================================
// Setup Function
// ============================================================================

void setup() {
    Serial.begin(115200);
    
    // Wait for Serial to be ready
    delay(1000);
    
    // Simple Serial test
    Serial.println("=== SERIAL TEST ===");
    Serial.println("If you can see this, Serial is working!");
    Serial.println("ESP32 WROOM-32 TVP5150 Video Decoder Interface with BT656");
    Serial.println("=========================================================");
    
    // Print pin configuration
    print_pin_configuration();
    
    // Validate pin configuration
    if (!validate_pin_configuration()) {
        Serial.println("ERROR: Invalid pin configuration!");
        return;
    }
    
    // Initialize TVP5150 I2C interface
    Serial.println("\nInitializing TVP5150 I2C interface...");
    tvp5150_connected = tvp5150_init(I2C_SDA_PIN, I2C_SCL_PIN);
    
    if (tvp5150_connected) {
        Serial.println("TVP5150 I2C interface initialized successfully!");
        
        // Configure TVP5150 for PAL output
        if (tvp5150_configure_pal()) {
            Serial.println("TVP5150 configured for PAL video standard");
        } else {
            Serial.println("Warning: Failed to configure PAL video standard");
        }
        
        // Set initial video processing parameters
        tvp5150_set_brightness(0x80);
        tvp5150_set_contrast(0x80);
        tvp5150_set_saturation(0x80);
        
        Serial.println("Video processing parameters set");
        
        // Initialize BT656 decoder
        Serial.println("\nInitializing BT656 decoder...");
        bt656_config_t decoder_config = {
            .expected_width = BT656_PAL_ACTIVE_PIXELS,
            .expected_height = BT656_PAL_ACTIVE_LINES,
            .enable_rgb_conversion = true,
            .enable_frame_buffer = false,
            .output_format = 1  // RGB
        };
        
        if (bt656_decoder_init(&bt656_decoder, &decoder_config)) {
            Serial.println("BT656 decoder initialized successfully!");
            
            // Set up decoder callbacks
            bt656_decoder_set_pixel_callback(&bt656_decoder, on_ycbcr_pixel);
            bt656_decoder_set_rgb_callback(&bt656_decoder, on_rgb_pixel);
            bt656_decoder_set_frame_callback(&bt656_decoder, on_frame_start);
            bt656_decoder_set_line_callback(&bt656_decoder, on_line_start);
            
            // Initialize BT656 interface
            Serial.println("\nInitializing BT656 interface...");
            bt656_interface_config_t interface_config = BT656_DEFAULT_CONFIG;
            interface_config.enable_debug_output = true;
            
            if (bt656_interface_init(&bt656_interface, &interface_config)) {
                Serial.println("BT656 interface initialized successfully!");
                
                // Set up interface callbacks
                bt656_interface_set_data_callback(&bt656_interface, on_data_ready);
                bt656_interface_set_error_callback(&bt656_interface, on_interface_error);
                
                // Connect decoder to interface
                bt656_interface_set_decoder(&bt656_interface, &bt656_decoder);
                
                // Start BT656 interface
                if (bt656_interface_start(&bt656_interface)) {
                    bt656_interface_ready = true;
                    Serial.println("BT656 interface started successfully!");
                    Serial.println("Ready to capture BT656 video stream at 27 MHz");
                } else {
                    Serial.println("ERROR: Failed to start BT656 interface!");
                }
            } else {
                Serial.println("ERROR: Failed to initialize BT656 interface!");
            }
        } else {
            Serial.println("ERROR: Failed to initialize BT656 decoder!");
        }
        
        // Initialize parallel interface (if VSYNC/HREF are connected)
        if (TVP5150_VSYNC_PIN != -1 && TVP5150_HREF_PIN != -1) {
            Serial.println("Initializing parallel interface...");
            parallel_interface_ready = tvp5150_parallel_init(&tvp5150_pin_config);
            
            if (parallel_interface_ready) {
                Serial.println("Parallel interface initialized successfully!");
            } else {
                Serial.println("Parallel interface initialization failed!");
            }
        } else {
            Serial.println("Parallel interface pins not configured - skipping");
        }
    } else {
        Serial.println("Failed to initialize TVP5150!");
        Serial.println("Check I2C connections and power supply");
    }
    
    Serial.println("Setup complete\n");
}

// ============================================================================
// Loop Function
// ============================================================================

void loop() {
    // Simple heartbeat to show the ESP32 is running
    static unsigned long last_heartbeat = 0;
    if (millis() - last_heartbeat >= 5000) {
        last_heartbeat = millis();
        Serial.println("ESP32 is running - heartbeat");
    }
    
    if (!tvp5150_connected) {
        Serial.println("TVP5150 not connected. Retrying in 5 seconds...");
        delay(5000);
        tvp5150_connected = tvp5150_init(I2C_SDA_PIN, I2C_SCL_PIN);
        return;
    }
    
    // Check status periodically
    if (millis() - last_status_check >= STATUS_CHECK_INTERVAL) {
        last_status_check = millis();
        
        // Read TVP5150 status
        tvp5150_status_t status = tvp5150_read_status();
        
        // Check if video is present
        bool video_present = tvp5150_is_video_present();
        
        // Print status information
        Serial.println("=== TVP5150 Status ===");
        Serial.printf("Video Present: %s\n", video_present ? "YES" : "NO");
        Serial.printf("VSYNC: %s\n", status.vsync ? "HIGH" : "LOW");
        Serial.printf("HSYNC: %s\n", status.hsync ? "HIGH" : "LOW");
        Serial.printf("Field: %s\n", status.field ? "ODD" : "EVEN");
        Serial.printf("Line: %d\n", status.line);
        Serial.printf("Frame Count: %d\n", status.frame_count);
        Serial.printf("Status Register: 0x%02X\n", status.status);
        
        if (video_present) {
            // Read current pixel data (simplified)
            yuv_pixel_t pixel = tvp5150_read_current_pixel();
            Serial.printf("Current Pixel - Y: %d, Cb: %d, Cr: %d\n", 
                         pixel.y, pixel.cb, pixel.cr);
        }
        
        Serial.println("=====================\n");
        
        // Print BT656 interface statistics if available
        if (bt656_interface_ready) {
            bt656_interface_print_stats(&bt656_interface);
            bt656_decoder_print_stats(&bt656_decoder);
            
            // Print frame statistics
            Serial.println("=== Frame Statistics ===");
            Serial.printf("Total Frames: %lu\n", total_frames_received);
            Serial.printf("Total Pixels: %lu\n", total_pixels_received);
            Serial.printf("Last Frame: %llu us ago\n", 
                         micros() - last_frame_timestamp);
            
            // Calculate frame rate if we have frames
            if (total_frames_received > 1) {
                uint64_t time_diff = micros() - last_frame_timestamp;
                float frame_rate = 1000000.0f / time_diff;
                Serial.printf("Estimated Frame Rate: %.2f fps\n", frame_rate);
            }
            Serial.println("======================\n");
        }
    }
    
    // Process BT656 buffer if interface is ready
    if (bt656_interface_ready) {
        bt656_interface_process_buffer(&bt656_interface);
    }
    
    // Small delay to prevent overwhelming the serial output
    delay(100);
}

// ============================================================================
// Utility Functions
// ============================================================================

// Optional: Add functions to adjust video parameters
void adjust_brightness(int delta) {
    static uint8_t current_brightness = 0x80;
    current_brightness = constrain(current_brightness + delta, 0, 255);
    tvp5150_set_brightness(current_brightness);
    Serial.printf("Brightness set to: %d\n", current_brightness);
}

void adjust_contrast(int delta) {
    static uint8_t current_contrast = 0x80;
    current_contrast = constrain(current_contrast + delta, 0, 255);
    tvp5150_set_contrast(current_contrast);
    Serial.printf("Contrast set to: %d\n", current_contrast);
}

void adjust_saturation(int delta) {
    static uint8_t current_saturation = 0x80;
    current_saturation = constrain(current_saturation + delta, 0, 255);
    tvp5150_set_saturation(current_saturation);
    Serial.printf("Saturation set to: %d\n", current_saturation);
}

// Function to restart BT656 interface
void restart_bt656_interface() {
    if (bt656_interface_ready) {
        Serial.println("Restarting BT656 interface...");
        bt656_interface_stop(&bt656_interface);
        delay(100);
        bt656_interface_start(&bt656_interface);
        Serial.println("BT656 interface restarted");
    }
}

// Function to reset BT656 decoder
void reset_bt656_decoder() {
    Serial.println("Resetting BT656 decoder...");
    bt656_decoder_reset(&bt656_decoder);
    bt656_decoder_reset_stats(&bt656_decoder);
    total_frames_received = 0;
    total_pixels_received = 0;
    Serial.println("BT656 decoder reset complete");
}