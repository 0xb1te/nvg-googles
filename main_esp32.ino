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
    
    // Enable basic crash debugging
    Serial.println("=== CRASH DEBUGGING CONFIGURATION ===");
    Serial.println("Enhanced error reporting enabled");
    Serial.println("If system crashes, check Serial Monitor for error messages");
    Serial.println("GPIO 27 will be automatically skipped for safety");
    Serial.println("=====================================================");
    
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
        
        // Auto-detect and configure video standard for your camera
        Serial.println("Auto-detecting video standard for Running Night Eagle 3 camera...");
        
        if (tvp5150_auto_detect_video_standard()) {
            Serial.println("Video standard detected and configured!");
            
            // Verify critical registers
            tvp5150_print_critical_registers();
        } else {
            Serial.println("WARNING: Could not auto-detect video standard");
            Serial.println("Falling back to Verilog PAL configuration...");
            
            if (tvp5150_force_configure_verilog()) {
                Serial.println("TVP5150 configured with Verilog PAL values");
                tvp5150_print_critical_registers();
            } else {
                Serial.println("ERROR: Failed to configure TVP5150");
            }
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
            
            if (bt656_interface_init(&bt656_interface, &interface_config)) {
                bt656_interface_ready = true;
                Serial.println("BT656 interface initialized successfully!");
                
                // Set up interface callbacks
                bt656_interface_set_data_callback(&bt656_interface, on_data_ready);
                bt656_interface_set_error_callback(&bt656_interface, on_interface_error);
                
                // Connect decoder to interface
                bt656_interface_set_decoder(&bt656_interface, &bt656_decoder);
                
                // Verify GPIO ISR service is working correctly
                if (bt656_interface_verify_gpio_isr_service()) {
                    Serial.println("✓ GPIO ISR service verified - system stable");
                    Serial.println("Ready to capture BT656 video stream at 27 MHz");
                } else {
                    Serial.println("⚠️  GPIO ISR service verification failed - using polling mode");
                    Serial.println("System will continue in polling mode for safety");
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
        
        // Enhanced video signal debugging
        Serial.println("=== VIDEO SIGNAL DEBUG ===");
        
        // Read status registers for detailed analysis
        uint8_t status_00 = tvp5150_read_register(0x00);  // Main status register
        uint8_t status_01 = tvp5150_read_register(0x01);  // Status register 2
        uint8_t status_02 = tvp5150_read_register(0x02);  // Status register 3
        
        // Parse status register 0x00 (main video status)
        bool video_lock = (status_00 & 0x01) != 0;        // Bit 0: Video lock
        bool sync_detected = (status_00 & 0x02) != 0;     // Bit 1: Sync detected
        bool field_detected = (status_00 & 0x04) != 0;    // Bit 2: Field detected
        bool video_present_bit = (status_00 & 0x10) != 0; // Bit 4: Video present (what tvp5150_is_video_present checks)
        
        Serial.printf("Status 0x00: 0x%02X | Video Lock: %s | Sync: %s | Field: %s | Video: %s\n",
                     status_00, video_lock ? "YES" : "NO", sync_detected ? "YES" : "NO", 
                     field_detected ? "YES" : "NO", video_present_bit ? "YES" : "NO");
        Serial.printf("Status 0x01: 0x%02X | Status 0x02: 0x%02X\n", status_01, status_02);
        
        // Print status information
        Serial.printf("Video: %s | VSYNC: %s | HSYNC: %s | Line: %d\n", 
                     video_present ? "YES" : "NO",
                     status.vsync ? "HIGH" : "LOW",
                     status.hsync ? "HIGH" : "LOW",
                     status.line);
        
        // Quick video diagnostics
        if (!video_present) {
            Serial.println("No video signal detected - Enhanced diagnostics:");
            
            // Read ALL critical registers from Verilog implementation
            uint8_t reg_0A = tvp5150_read_register(0x0A);
            uint8_t reg_0B = tvp5150_read_register(0x0B);
            uint8_t reg_0C = tvp5150_read_register(0x0C);
            uint8_t reg_0D = tvp5150_read_register(0x0D);
            uint8_t reg_0E = tvp5150_read_register(0x0E);
            uint8_t reg_0F = tvp5150_read_register(0x0F);
            uint8_t reg_15 = tvp5150_read_register(0x15);
            
            Serial.printf("Critical Registers:\n");
            Serial.printf("0x0A (Status): 0x%02X (expected: 0x80)\n", reg_0A);
            Serial.printf("0x0B (Video Std): 0x%02X (expected: 0x00)\n", reg_0B);
            Serial.printf("0x0C (Output Ctrl): 0x%02X (expected: 0x80) %s\n", 
                         reg_0C, (reg_0C & 0x80) ? "✓" : "✗");
            Serial.printf("0x0D (PAL Config): 0x%02X (expected: 0x47) %s\n", 
                         reg_0D, (reg_0D == 0x47) ? "✓" : "✗");
            Serial.printf("0x0E: 0x%02X (expected: 0x00)\n", reg_0E);
            Serial.printf("0x0F (Input Sel): 0x%02X (expected: 0x02) %s\n", 
                         reg_0F, (reg_0F == 0x02) ? "✓" : "✗");
            Serial.printf("0x15 (Output Format): 0x%02X (expected: 0x01) %s\n", 
                         reg_15, (reg_15 == 0x01) ? "✓" : "✗");
            
            // Check for specific issues
            if (reg_0F != 0x02) {
                Serial.println("✗ CRITICAL: Input selection wrong! Should be 0x02 for AIN1");
            }
            if (!(reg_0C & 0x80)) {
                Serial.println("✗ CRITICAL: Parallel output disabled!");
            }
            if (reg_0D != 0x47) {
                Serial.println("✗ CRITICAL: Not configured for PAL!");
            }
            
            // Additional debugging for sync signals
            Serial.println("\n=== SYNC SIGNAL ANALYSIS ===");
            Serial.printf("VSYNC pin (GPIO %d): %s\n", TVP5150_VSYNC_PIN, 
                         TVP5150_VSYNC_PIN != 255 ? (digitalRead(TVP5150_VSYNC_PIN) ? "HIGH" : "LOW") : "NOT CONNECTED");
            Serial.printf("HREF pin (GPIO %d): %s\n", TVP5150_HREF_PIN, 
                         TVP5150_HREF_PIN != 255 ? (digitalRead(TVP5150_HREF_PIN) ? "HIGH" : "LOW") : "NOT CONNECTED");
            Serial.printf("PCLK pin (GPIO %d): %s\n", TVP5150_PCLK_PIN, 
                         digitalRead(TVP5150_PCLK_PIN) ? "HIGH" : "LOW");
            
            // Check data bus for any activity
            Serial.println("\n=== DATA BUS ANALYSIS ===");
            uint8_t data_pins[] = {TVP5150_D0_PIN, TVP5150_D1_PIN, TVP5150_D2_PIN, TVP5150_D3_PIN,
                                  TVP5150_D4_PIN, TVP5150_D5_PIN, TVP5150_D6_PIN, TVP5150_D7_PIN};
            Serial.print("Data bus (D0-D7): ");
            for (int i = 0; i < 8; i++) {
                if (data_pins[i] != 255) {
                    Serial.printf("%s ", digitalRead(data_pins[i]) ? "1" : "0");
                } else {
                    Serial.print("X ");
                }
            }
            Serial.println();
            
            // Suggest troubleshooting steps
            Serial.println("\n=== TROUBLESHOOTING SUGGESTIONS ===");
            if (!video_lock && !sync_detected) {
                Serial.println("1. Check camera power supply (5-24V)");
                Serial.println("2. Verify composite video cable connection");
                Serial.println("3. Ensure camera is powered on and outputting video");
                Serial.println("4. Check if camera output is PAL or NTSC");
            } else if (sync_detected && !video_lock) {
                Serial.println("1. Sync detected but no video lock - check video standard configuration");
                Serial.println("2. Try switching between PAL and NTSC");
            } else if (video_lock && !video_present_bit) {
                Serial.println("1. Video lock detected but video present bit not set");
                Serial.println("2. Check output format configuration");
            }
        }
        
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
            Serial.printf("Frames: %lu | Pixels: %lu\n", total_frames_received, total_pixels_received);
            
            // DEBUG: Show current D0-D7 pin states
            bt656_interface_print_pin_states(&bt656_interface);
            
                    // DEBUG: Look for Verilog BT656 patterns
        static bool pattern_analysis_done = false;
        if (!pattern_analysis_done) {
            Serial.println("=== DEBUG: Analyzing for Verilog patterns ===");
            bt656_interface_look_for_verilog_patterns(&bt656_interface, 100); // Analyze 100 samples
            pattern_analysis_done = true;
        }
        
        // Enhanced video signal detection and debugging
        static bool enhanced_video_debug_done = false;
        if (!enhanced_video_debug_done && !video_present) {
            Serial.println("\n=== ENHANCED VIDEO SIGNAL DEBUGGING ===");
            
            // Test different input selections systematically
            Serial.println("1. Testing different input selections...");
            uint8_t input_selections[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
            bool found_working_input = false;
            
            for (int i = 0; i < sizeof(input_selections)/sizeof(input_selections[0]); i++) {
                uint8_t input_sel = input_selections[i];
                Serial.printf("   Testing input 0x%02X... ", input_sel);
                
                if (tvp5150_test_input_selection(input_sel)) {
                    delay(200); // Wait for signal to stabilize
                    
                    // Read status registers after input change
                    uint8_t status_00 = tvp5150_read_register(0x00);
                    bool video_lock = (status_00 & 0x01) != 0;
                    bool sync_detected = (status_00 & 0x02) != 0;
                    bool video_present_bit = (status_00 & 0x10) != 0;
                    
                    Serial.printf("Status: 0x%02X | Lock: %s | Sync: %s | Video: %s\n",
                                 status_00, video_lock ? "YES" : "NO", sync_detected ? "YES" : "NO", 
                                 video_present_bit ? "YES" : "NO");
                    
                    if (video_lock || sync_detected) {
                        Serial.printf("   ✓ INPUT 0x%02X SHOWS SIGNAL ACTIVITY!\n", input_sel);
                        found_working_input = true;
                        break;
                    }
                } else {
                    Serial.println("FAILED");
                }
            }
            
            if (!found_working_input) {
                Serial.println("   ✗ No input shows signal activity");
            }
            
            // Test different video standards
            Serial.println("\n2. Testing different video standards...");
            
            // Test PAL
            Serial.print("   Testing PAL... ");
            if (tvp5150_configure_video_standard(true)) {
                delay(300);
                uint8_t status_00 = tvp5150_read_register(0x00);
                bool video_lock = (status_00 & 0x01) != 0;
                bool sync_detected = (status_00 & 0x02) != 0;
                Serial.printf("Status: 0x%02X | Lock: %s | Sync: %s\n",
                             status_00, video_lock ? "YES" : "NO", sync_detected ? "YES" : "NO");
            } else {
                Serial.println("FAILED");
            }
            
            // Test NTSC
            Serial.print("   Testing NTSC... ");
            if (tvp5150_configure_video_standard(false)) {
                delay(300);
                uint8_t status_00 = tvp5150_read_register(0x00);
                bool video_lock = (status_00 & 0x01) != 0;
                bool sync_detected = (status_00 & 0x02) != 0;
                Serial.printf("Status: 0x%02X | Lock: %s | Sync: %s\n",
                             status_00, video_lock ? "YES" : "NO", sync_detected ? "YES" : "NO");
            } else {
                Serial.println("FAILED");
            }
            
            // Check raw pin states
            Serial.println("\n3. Checking raw pin states...");
            Serial.printf("   PCLK (GPIO %d): %s\n", TVP5150_PCLK_PIN, 
                         digitalRead(TVP5150_PCLK_PIN) ? "HIGH" : "LOW");
            Serial.printf("   XCLK (GPIO %d): %s\n", TVP5150_XCLK_PIN, 
                         digitalRead(TVP5150_XCLK_PIN) ? "HIGH" : "LOW");
            
            // Check data bus for any activity
            uint8_t data_pins[] = {TVP5150_D0_PIN, TVP5150_D1_PIN, TVP5150_D2_PIN, TVP5150_D3_PIN,
                                  TVP5150_D4_PIN, TVP5150_D5_PIN, TVP5150_D6_PIN, TVP5150_D7_PIN};
            Serial.print("   Data bus (D0-D7): ");
            for (int i = 0; i < 8; i++) {
                if (data_pins[i] != 255) {
                    Serial.printf("%s ", digitalRead(data_pins[i]) ? "1" : "0");
                } else {
                    Serial.print("X ");
                }
            }
            Serial.println();
            
            // Check I2C communication
            Serial.println("\n4. Checking I2C communication...");
            uint8_t device_id = tvp5150_read_register(0x00);
            Serial.printf("   Device ID register (0x00): 0x%02X\n", device_id);
            
            // Final recommendations
            Serial.println("\n=== FINAL RECOMMENDATIONS ===");
            Serial.println("If no video signal is detected:");
            Serial.println("1. Verify camera is powered (5-24V) and outputting video");
            Serial.println("2. Check composite video cable connection to TVP5150");
            Serial.println("3. Ensure camera is set to PAL or NTSC output");
            Serial.println("4. Try different input pins on TVP5150 (AIN1, AIN2, etc.)");
            Serial.println("5. Check if camera requires specific video standard");
            
            enhanced_video_debug_done = true;
        }
        
        // Run comprehensive diagnostics once
        static bool diagnostics_run = false;
        if (!diagnostics_run) {
            delay(2000); // Wait for everything to stabilize
            run_comprehensive_diagnostics();
            diagnostics_run = true;
        }
        
        // Check if camera is actually connected and providing signal
        static bool camera_check_done = false;
        if (!camera_check_done) {
            delay(1000);
            Serial.println("=== CHECKING CAMERA CONNECTION ===");
            tvp5150_check_camera_connection();
            camera_check_done = true;
        }
        
        // Monitor data bus activity for debugging
        static unsigned long last_data_monitor = 0;
        static uint32_t data_activity_count = 0;
        if (millis() - last_data_monitor >= 2000) { // Every 2 seconds
            last_data_monitor = millis();
            
            // Quick check for data bus activity
            uint8_t data_pins[] = {TVP5150_D0_PIN, TVP5150_D1_PIN, TVP5150_D2_PIN, TVP5150_D3_PIN,
                                  TVP5150_D4_PIN, TVP5150_D5_PIN, TVP5150_D6_PIN, TVP5150_D7_PIN};
            
            // Sample data bus multiple times to detect activity
            uint8_t samples[10][8];
            for (int sample = 0; sample < 10; sample++) {
                for (int pin = 0; pin < 8; pin++) {
                    if (data_pins[pin] != 255) {
                        samples[sample][pin] = digitalRead(data_pins[pin]);
                    } else {
                        samples[sample][pin] = 0;
                    }
                }
                delayMicroseconds(100); // Small delay between samples
            }
            
            // Check if data is changing (indicating activity)
            bool data_changing = false;
            for (int pin = 0; pin < 8; pin++) {
                if (data_pins[pin] != 255) {
                    uint8_t first_value = samples[0][pin];
                    for (int sample = 1; sample < 10; sample++) {
                        if (samples[sample][pin] != first_value) {
                            data_changing = true;
                            break;
                        }
                    }
                }
            }
            
            if (data_changing) {
                data_activity_count++;
                Serial.printf("Data bus activity detected! (Count: %lu)\n", data_activity_count);
                
                // Show current data bus state
                Serial.print("Current data bus: ");
                for (int pin = 0; pin < 8; pin++) {
                    if (data_pins[pin] != 255) {
                        Serial.printf("%s ", digitalRead(data_pins[pin]) ? "1" : "0");
                    } else {
                        Serial.print("X ");
                    }
                }
                Serial.println();
            }
        }
        
        // Check PCLK pin safety
        static bool pclk_check_done = false;
        if (!pclk_check_done) {
            delay(500);
            Serial.println("=== PCLK PIN SAFETY CHECK ===");
            Serial.printf("Current PCLK pin: GPIO %d\n", TVP5150_PCLK_PIN);
            
            // List of safe pins for interrupts
            Serial.println("Safe interrupt pins: 4, 5, 12, 13, 14, 15, 18, 19, 21, 22, 23, 25, 26, 32, 33");
            Serial.println("Problematic pins: 0, 2, 27, 28, 29, 30, 31, 34-39");
            
            if (TVP5150_PCLK_PIN == 18) {
                Serial.println("✓ GPIO 18 is safe for interrupts");
            } else {
                Serial.println("⚠️  Consider changing to GPIO 18 for better stability");
            }
            pclk_check_done = true;
        }
        }
    }
    
            // Process BT656 buffer if interface is ready
        if (bt656_interface_ready) {
            // If interrupts are disabled, use polling mode
            if (!bt656_interface_is_running(&bt656_interface)) {
                bt656_interface_poll_data(&bt656_interface);
            }
            
            // Process any available data
            bt656_interface_process_buffer(&bt656_interface);
            
            // DEBUG: Check why interrupts might be disabled
            static bool interrupt_debug_done = false;
            if (!interrupt_debug_done) {
                Serial.println("=== DEBUG: Interrupt Status ===");
                Serial.printf("Interface running: %s\n", bt656_interface_is_running(&bt656_interface) ? "YES" : "NO");
                Serial.printf("Interrupt enabled: %s\n", bt656_interface.interrupt_enabled ? "YES" : "NO");
                Serial.printf("PCLK pin: GPIO %d\n", bt656_interface.config.pclk_pin);
                Serial.printf("PCLK state: %s\n", digitalRead(bt656_interface.config.pclk_pin) ? "HIGH" : "LOW");
                interrupt_debug_done = true;
            }
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
        // Note: bt656_interface_start() was removed - functionality merged into bt656_interface_init()
        // To restart, you would need to call bt656_interface_init() again
        Serial.println("BT656 interface stopped (restart requires re-initialization)");
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

// Function to capture raw D0-D7 data for debugging
void capture_raw_data(uint32_t sample_count) {
    if (bt656_interface_ready) {
        Serial.println("Starting raw data capture...");
        bt656_interface_print_raw_data(&bt656_interface, sample_count);
    } else {
        Serial.println("BT656 interface not ready for data capture");
    }
}

// Function to force enable interrupts
void force_enable_interrupts() {
    if (bt656_interface_ready) {
        Serial.println("Force enabling interrupts...");
        bt656_interface_stop(&bt656_interface);
        delay(100);
        // Note: bt656_interface_start() was removed - functionality merged into bt656_interface_init()
        // To re-enable interrupts, you would need to call bt656_interface_init() again
        Serial.println("Interrupts stopped (re-enable requires re-initialization)");
    }
}

// Function to test different TVP5150 input selections
void test_tvp5150_inputs() {
    Serial.println("=== TESTING TVP5150 INPUT SELECTIONS ===");
    
    // Test different input selections
    uint8_t input_selections[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    
    for (int i = 0; i < sizeof(input_selections)/sizeof(input_selections[0]); i++) {
        uint8_t input_sel = input_selections[i];
        
        Serial.printf("\n--- Testing Input 0x%02X ---\n", input_sel);
        
        if (tvp5150_test_input_selection(input_sel)) {
            delay(500); // Wait for signal to stabilize
            
            // Check if we get video signal
            bool video_present = tvp5150_is_video_present();
            Serial.printf("Video present: %s\n", video_present ? "YES" : "NO");
            
            if (video_present) {
                Serial.printf("✓ INPUT 0x%02X HAS VIDEO SIGNAL!\n", input_sel);
                return; // Found working input
            }
        }
    }
    
    Serial.println("✗ No working input found");
}

// Function to test both PAL and NTSC configurations
void test_video_standards() {
    Serial.println("=== TESTING VIDEO STANDARDS ===");
    
    // Test PAL
    Serial.println("\n--- Testing PAL Configuration ---");
    if (tvp5150_configure_video_standard(true)) { // PAL
        delay(1000); // Wait for signal to stabilize
        
        bool pal_video = tvp5150_is_video_present();
        Serial.printf("PAL video detected: %s\n", pal_video ? "YES" : "NO");
        
        if (pal_video) {
            Serial.println("✓ PAL VIDEO WORKING!");
            return;
        }
    }
    
    // Test NTSC
    Serial.println("\n--- Testing NTSC Configuration ---");
    if (tvp5150_configure_video_standard(false)) { // NTSC
        delay(1000); // Wait for signal to stabilize
        
        bool ntsc_video = tvp5150_is_video_present();
        Serial.printf("NTSC video detected: %s\n", ntsc_video ? "YES" : "NO");
        
        if (ntsc_video) {
            Serial.println("✓ NTSC VIDEO WORKING!");
            return;
        }
    }
    
    Serial.println("✗ Neither PAL nor NTSC working");
}

// Comprehensive diagnostic function
void run_comprehensive_diagnostics() {
    Serial.println("\n=== COMPREHENSIVE DIAGNOSTICS ===");
    
    // 1. Check camera connection and signal
    Serial.println("1. Camera Connection Check:");
    tvp5150_check_camera_connection();
    
    // 2. Check I2C communication
    Serial.println("2. I2C Communication Test:");
    uint8_t device_id = tvp5150_read_register(0x00);
    Serial.printf("   Device ID: 0x%02X\n", device_id);
    
    // 3. Check critical registers
    Serial.println("3. Critical Registers:");
    tvp5150_print_critical_registers();
    
    // 4. Test video standards
    Serial.println("4. Video Standard Test:");
    test_video_standards();
    
    // 5. Test input selections
    Serial.println("5. Input Selection Test:");
    test_tvp5150_inputs();
    
    // 6. Check raw data patterns
    Serial.println("6. Raw Data Pattern Analysis:");
    bt656_interface_look_for_verilog_patterns(&bt656_interface, 200);
    
    Serial.println("=== DIAGNOSTICS COMPLETE ===");
}

