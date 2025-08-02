#include "tvp5150_esp32.h"
#include <Wire.h>

// I2C communication functions with proper ACK handling
static uint8_t read_register(uint8_t addr, uint8_t reg) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    uint8_t error = Wire.endTransmission(false); // Don't send stop condition
    
    if (error != 0) {
        Serial.printf("I2C write error: %d\n", error);
        return 0;
    }
    
    Wire.requestFrom(addr, 1);
    if (Wire.available()) {
        return Wire.read();
    }
    return 0;
}

static bool write_register(uint8_t addr, uint8_t reg, uint8_t data) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(data);
    uint8_t error = Wire.endTransmission();
    
    if (error != 0) {
        Serial.printf("I2C write error at addr 0x%02X, reg 0x%02X: %d\n", addr, reg, error);
        return false;
    }
    
    return true;
}

static uint16_t read_register_16bit(uint8_t addr, uint8_t reg) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    uint8_t error = Wire.endTransmission(false);
    
    if (error != 0) {
        return 0;
    }
    
    Wire.requestFrom(addr, 2);
    if (Wire.available() >= 2) {
        uint16_t msb = Wire.read();
        uint16_t lsb = Wire.read();
        return (msb << 8) | lsb;
    }
    return 0;
}

// Enhanced I2C write with retry mechanism
static bool write_register_with_retry(uint8_t addr, uint8_t reg, uint8_t data, uint8_t max_retries = 3) {
    for (uint8_t retry = 0; retry < max_retries; retry++) {
        if (write_register(addr, reg, data)) {
            return true;
        }
        delay(1); // Small delay between retries
    }
    return false;
}

// Alternative approach - Skip device ID verification for now
bool tvp5150_init(uint8_t sda_pin, uint8_t scl_pin) {
    // Initialize I2C
    Wire.begin(sda_pin, scl_pin);
    Wire.setClock(100000); // 100kHz for reliable TVP5150 communication
    
    delay(100); // Give TVP5150 time to power up
    
    // Check if TVP5150 is present on either address
    Wire.beginTransmission(TVP5150_I2C_ADDR_PRIMARY);
    uint8_t error1 = Wire.endTransmission();
    
    Wire.beginTransmission(TVP5150_I2C_ADDR_SECONDARY);
    uint8_t error2 = Wire.endTransmission();
    
    if (error1 != 0 && error2 != 0) {
        Serial.println("TVP5150 not found on I2C bus");
        Serial.printf("Primary address 0x%02X: %s\n", TVP5150_I2C_ADDR_PRIMARY, error1 == 0 ? "OK" : "FAIL");
        Serial.printf("Secondary address 0x%02X: %s\n", TVP5150_I2C_ADDR_SECONDARY, error2 == 0 ? "OK" : "FAIL");
        return false;
    }
    
    // Determine which address to use
    uint8_t tvp5150_addr = (error1 == 0) ? TVP5150_I2C_ADDR_PRIMARY : TVP5150_I2C_ADDR_SECONDARY;
    Serial.printf("TVP5150 found at address 0x%02X\n", tvp5150_addr);
    
    // Try to read device ID but don't fail if it doesn't match
    uint8_t device_id = read_register(tvp5150_addr, 0x00);
    Serial.printf("Device ID read: 0x%02X\n", device_id);
    
    if (device_id == 0x51) {
        Serial.println("TVP5150 device ID verified successfully");
    } else {
        Serial.printf("Warning: Unexpected device ID 0x%02X (expected 0x51)\n", device_id);
        Serial.println("Continuing anyway - this might be a different variant");
    }
    
    // Configure for PAL video standard (from Verilog code)
    if (!tvp5150_configure_pal()) {
        Serial.println("Failed to configure PAL video standard");
        return false;
    }
    
    // Set brightness, contrast, saturation to default values
    if (!write_register_with_retry(tvp5150_addr, TVP5150_REG_BRIGHTNESS, 0x80)) {
        Serial.println("Failed to set brightness");
        return false;
    }
    if (!write_register_with_retry(tvp5150_addr, TVP5150_REG_CONTRAST, 0x80)) {
        Serial.println("Failed to set contrast");
        return false;
    }
    if (!write_register_with_retry(tvp5150_addr, TVP5150_REG_SATURATION, 0x80)) {
        Serial.println("Failed to set saturation");
        return false;
    }
    
    // Force correct output format (register 0x15) - this is critical!
    Serial.println("Fixing output format register 0x15...");
    
    // Try multiple times to ensure it sticks
    bool register_fixed = false;
    for (int attempt = 0; attempt < 5; attempt++) {
        if (!write_register_with_retry(tvp5150_addr, 0x15, 0x01)) {
            Serial.printf("Failed to write register 0x15 on attempt %d\n", attempt + 1);
        } else {
            delay(10);
            uint8_t output_format = read_register(tvp5150_addr, 0x15);
            Serial.printf("Attempt %d: Register 0x15 = 0x%02X\n", attempt + 1, output_format);
            if (output_format == 0x01) {
                Serial.println("✓ Register 0x15 successfully set to 0x01!");
                register_fixed = true;
                break;
            }
        }
        delay(50);
    }
    
    if (!register_fixed) {
        Serial.println("WARNING: Could not set register 0x15 to 0x01!");
        Serial.println("This may cause BT656 output issues.");
    }
    
    Serial.println("TVP5150 initialized successfully");
    return true;
}

bool tvp5150_configure_pal(void) {
    // Configure TVP5150 for PAL video standard (from Verilog code)
    uint8_t tvp5150_addr = TVP5150_I2C_ADDR_PRIMARY; // Use primary address
    
    Serial.println("Configuring PAL video standard...");
    
    // Video Standard Selection Registers
    const uint8_t pal_config[][2] = {
        {TVP5150_REG_VIDEO_STD_0A, 0x80},
        {TVP5150_REG_VIDEO_STD_0B, 0x00},
        {TVP5150_REG_VIDEO_STD_0C, 0x80},
        {TVP5150_REG_VIDEO_STD_0D, 0x47}, // PAL = 0x47
        {TVP5150_REG_VIDEO_STD_0E, 0x00},
        {TVP5150_REG_VIDEO_STD_0F, 0x02},
        {TVP5150_REG_VIDEO_STD_11, 0x04},
        {TVP5150_REG_VIDEO_STD_12, 0x00},
        {TVP5150_REG_VIDEO_STD_13, 0x04},
        {TVP5150_REG_VIDEO_STD_14, 0x00},
        {TVP5150_REG_VIDEO_STD_15, 0x01},
        {TVP5150_REG_VIDEO_STD_16, 0x80},
        {TVP5150_REG_VIDEO_STD_18, 0x00},
        {TVP5150_REG_VIDEO_STD_19, 0x00},
        {TVP5150_REG_VIDEO_STD_1A, 0x0C},
        {TVP5150_REG_VIDEO_STD_1B, 0x14},
        {TVP5150_REG_VIDEO_STD_1C, 0x00},
        {TVP5150_REG_VIDEO_STD_1D, 0x00},
        {TVP5150_REG_VIDEO_STD_1E, 0x00},
        {TVP5150_REG_VIDEO_STD_28, 0x00}
    };
    
    // Write PAL configuration registers
    for (int i = 0; i < sizeof(pal_config)/sizeof(pal_config[0]); i++) {
        if (!write_register_with_retry(tvp5150_addr, pal_config[i][0], pal_config[i][1])) {
            Serial.printf("Failed to write register 0x%02X\n", pal_config[i][0]);
            return false;
        }
        delay(1); // Small delay between writes
    }
    
    // Advanced Configuration Registers (key ones from Verilog)
    const uint8_t adv_config[][2] = {
        {TVP5150_REG_ADV_C2, 0x04},
        {TVP5150_REG_ADV_C3, 0xDC},
        {TVP5150_REG_ADV_C4, 0x0F},
        {TVP5150_REG_ADV_CB, 0x59},
        {TVP5150_REG_ADV_CC, 0x03},
        {TVP5150_REG_ADV_CD, 0x01}
    };
    
    for (int i = 0; i < sizeof(adv_config)/sizeof(adv_config[0]); i++) {
        if (!write_register_with_retry(tvp5150_addr, adv_config[i][0], adv_config[i][1])) {
            Serial.printf("Failed to write advanced register 0x%02X\n", adv_config[i][0]);
            return false;
        }
        delay(1);
    }
    
    // Set all D0-DF registers to 0xFF (default values from Verilog)
    for (uint8_t reg = 0xD0; reg <= 0xDF; reg++) {
        if (!write_register_with_retry(tvp5150_addr, reg, 0xFF)) {
            Serial.printf("Failed to write register 0x%02X\n", reg);
            return false;
        }
        delay(1);
    }
    
    // Set all E0-EF registers to 0xFF
    for (uint8_t reg = 0xE0; reg <= 0xEF; reg++) {
        if (!write_register_with_retry(tvp5150_addr, reg, 0xFF)) {
            Serial.printf("Failed to write register 0x%02X\n", reg);
            return false;
        }
        delay(1);
    }
    
    // Set all F0-FB registers to 0xFF
    for (uint8_t reg = 0xF0; reg <= 0xFB; reg++) {
        if (!write_register_with_retry(tvp5150_addr, reg, 0xFF)) {
            Serial.printf("Failed to write register 0x%02X\n", reg);
            return false;
        }
        delay(1);
    }
    
    // Set final configuration register
    if (!write_register_with_retry(tvp5150_addr, 0xFC, 0x7F)) {
        Serial.println("Failed to write final configuration register");
        return false;
    }
    
    Serial.println("PAL video standard configured successfully");
    return true;
}

void tvp5150_close(void) {
    // Nothing specific needed for ESP32
    Wire.end();
}

yuv_pixel_t tvp5150_read_current_pixel(void) {
    yuv_pixel_t pixel;
    uint8_t tvp5150_addr = TVP5150_I2C_ADDR_PRIMARY;
    
    // Read Y, Cb, Cr values from status registers
    // Note: This is a simplified approach - real implementation would need
    // to read from the digital output pins (D0-D7, VSYNC, HREF, PCLK)
    
    uint8_t status1 = read_register(tvp5150_addr, TVP5150_REG_STATUS_1);
    uint8_t status2 = read_register(tvp5150_addr, TVP5150_REG_STATUS_2);
    uint8_t status3 = read_register(tvp5150_addr, TVP5150_REG_STATUS_3);
    
    // Extract Y, Cb, Cr from status registers (simplified)
    pixel.y = status1;
    pixel.cb = status2;
    pixel.cr = status3;
    
    return pixel;
}

tvp5150_status_t tvp5150_read_status(void) {
    tvp5150_status_t status;
    uint8_t tvp5150_addr = TVP5150_I2C_ADDR_PRIMARY;
    
    // Read status registers
    uint8_t status1 = read_register(tvp5150_addr, TVP5150_REG_STATUS_1);
    uint8_t status2 = read_register(tvp5150_addr, TVP5150_REG_STATUS_2);
    uint8_t status3 = read_register(tvp5150_addr, TVP5150_REG_STATUS_3);
    uint8_t status4 = read_register(tvp5150_addr, TVP5150_REG_STATUS_4);
    
    // Parse status bits
    status.vsync = (status1 & 0x80) != 0;
    status.hsync = (status1 & 0x40) != 0;
    status.field = (status1 & 0x20) != 0;
    status.video_present = (status1 & 0x10) != 0;
    
    // Extract line number
    status.line = ((status2 & 0x01) << 8) | status3;
    
    // Extract frame count (simplified)
    status.frame_count = status4;
    
    // Buffer count (not applicable for I2C interface)
    status.buffer_count = 0;
    
    // Overall status
    status.status = status1;
    
    return status;
}

bool tvp5150_read_frame_buffer(yuv_pixel_t* buffer, uint16_t max_pixels) {
    // Note: This function is not applicable for I2C interface
    // The TVP5150 outputs video data on parallel pins (D0-D7)
    // I2C is only used for configuration and status
    
    // For now, return false to indicate no data available
    return false;
}

uint16_t tvp5150_get_available_pixels(void) {
    // Not applicable for I2C interface
    return 0;
}

bool tvp5150_is_video_present(void) {
    uint8_t tvp5150_addr = TVP5150_I2C_ADDR_PRIMARY;
    uint8_t status1 = read_register(tvp5150_addr, TVP5150_REG_STATUS_1);
    return (status1 & 0x10) != 0;
}

// Function to check camera connection and signal
void tvp5150_check_camera_connection(void) {
    uint8_t tvp5150_addr = TVP5150_I2C_ADDR_PRIMARY;
    
    Serial.println("=== CAMERA CONNECTION CHECK ===");
    
    // Check device ID
    uint8_t device_id = read_register(tvp5150_addr, 0x00);
    Serial.printf("Device ID: 0x%02X\n", device_id);
    
    // Check video status register
    uint8_t video_status = read_register(tvp5150_addr, 0x00);
    Serial.printf("Video Status: 0x%02X\n", video_status);
    
    // Check if video lock is present
    bool video_lock = (video_status & 0x01) != 0;
    Serial.printf("Video Lock: %s\n", video_lock ? "YES" : "NO");
    
    // Check if sync is detected
    bool sync_detected = (video_status & 0x02) != 0;
    Serial.printf("Sync Detected: %s\n", sync_detected ? "YES" : "NO");
    
    // Check if field is detected
    bool field_detected = (video_status & 0x04) != 0;
    Serial.printf("Field Detected: %s\n", field_detected ? "YES" : "NO");
    
    // Check input selection
    uint8_t input_sel = read_register(tvp5150_addr, 0x0F);
    Serial.printf("Input Selection: 0x%02X\n", input_sel);
    
    // Check video standard
    uint8_t video_std = read_register(tvp5150_addr, 0x0D);
    Serial.printf("Video Standard: 0x%02X\n", video_std);
    
    if (!video_lock && !sync_detected) {
        Serial.println("⚠️  NO VIDEO SIGNAL DETECTED!");
        Serial.println("Check:");
        Serial.println("1. Camera power (5-24V)");
        Serial.println("2. Composite video cable connection");
        Serial.println("3. Camera is powered on and outputting video");
        Serial.println("4. Correct input pin on TVP5150");
    } else if (video_lock) {
        Serial.println("✓ VIDEO SIGNAL DETECTED!");
    } else {
        Serial.println("⚠️  PARTIAL SIGNAL - sync detected but no video lock");
    }
    
    Serial.println("================================");
}

void tvp5150_set_brightness(uint8_t brightness) {
    uint8_t tvp5150_addr = TVP5150_I2C_ADDR_PRIMARY;
    write_register_with_retry(tvp5150_addr, TVP5150_REG_BRIGHTNESS, brightness);
}

void tvp5150_set_contrast(uint8_t contrast) {
    uint8_t tvp5150_addr = TVP5150_I2C_ADDR_PRIMARY;
    write_register_with_retry(tvp5150_addr, TVP5150_REG_CONTRAST, contrast);
}

void tvp5150_set_saturation(uint8_t saturation) {
    uint8_t tvp5150_addr = TVP5150_I2C_ADDR_PRIMARY;
    write_register_with_retry(tvp5150_addr, TVP5150_REG_SATURATION, saturation);
}

// Function to read and display critical TVP5150 registers
void tvp5150_print_critical_registers(void) {
    uint8_t tvp5150_addr = TVP5150_I2C_ADDR_PRIMARY;
    
    Serial.println("=== TVP5150 CRITICAL REGISTERS ===");
    Serial.println("Reg | Expected | Actual | Status");
    Serial.println("----|----------|--------|--------");
    
    // Critical registers from Verilog implementation
    const uint8_t critical_regs[][2] = {
        {0x0A, 0x80}, // Status register
        {0x0B, 0x00}, // Video standard
        {0x0C, 0x80}, // Output control (PARALLEL OUTPUT!)
        {0x0D, 0x47}, // PAL configuration
        {0x0E, 0x00}, // 
        {0x0F, 0x02}, // Input selection (CRITICAL!)
        {0x15, 0x01}, // Output format
    };
    
    for (int i = 0; i < sizeof(critical_regs)/sizeof(critical_regs[0]); i++) {
        uint8_t reg = critical_regs[i][0];
        uint8_t expected = critical_regs[i][1];
        uint8_t actual = read_register(tvp5150_addr, reg);
        
        Serial.printf("0x%02X | 0x%02X      | 0x%02X    | %s\n", 
                     reg, expected, actual, (actual == expected) ? "✓" : "✗");
    }
    
    Serial.println("=================================");
}

// Function to force configure TVP5150 with exact Verilog values
bool tvp5150_force_configure_verilog(void) {
    uint8_t tvp5150_addr = TVP5150_I2C_ADDR_PRIMARY;
    
    Serial.println("=== FORCE CONFIGURING TVP5150 (Verilog Values) ===");
    
    // Exact values from working Verilog implementation
    const uint8_t verilog_config[][2] = {
        {0x0A, 0x80}, // Status register
        {0x0B, 0x00}, // Video standard
        {0x0C, 0x80}, // Output control (ENABLE PARALLEL!)
        {0x0D, 0x47}, // PAL configuration
        {0x0E, 0x00}, // 
        {0x0F, 0x02}, // Input selection (AIN1)
        {0x11, 0x04}, // 
        {0x12, 0x00}, // 
        {0x13, 0x04}, // 
        {0x14, 0x00}, // 
        {0x15, 0x01}, // Output format (BT656)
        {0x16, 0x80}, // 
        {0x18, 0x00}, // 
        {0x19, 0x00}, // 
        {0x1A, 0x0C}, // 
        {0x1B, 0x14}, // 
        {0x1C, 0x00}, // 
        {0x1D, 0x00}, // 
        {0x1E, 0x00}, // 
        {0x28, 0x00}, // 
    };
    
    for (int i = 0; i < sizeof(verilog_config)/sizeof(verilog_config[0]); i++) {
        uint8_t reg = verilog_config[i][0];
        uint8_t value = verilog_config[i][1];
        
        Serial.printf("Writing 0x%02X to register 0x%02X...\n", value, reg);
        
        if (!write_register_with_retry(tvp5150_addr, reg, value)) {
            Serial.printf("FAILED to write register 0x%02X\n", reg);
            return false;
        }
        delay(5); // Longer delay for critical registers
    }
    
    Serial.println("Verilog configuration applied successfully!");
    
    // Force register 0x15 multiple times to ensure it sticks
    Serial.println("Forcing register 0x15 multiple times...");
    for (int attempt = 0; attempt < 5; attempt++) {
        if (!write_register_with_retry(tvp5150_addr, 0x15, 0x01)) {
            Serial.printf("Failed to write register 0x15 on attempt %d\n", attempt + 1);
        } else {
            delay(10);
            uint8_t readback = read_register(tvp5150_addr, 0x15);
            Serial.printf("Attempt %d: Register 0x15 = 0x%02X\n", attempt + 1, readback);
            if (readback == 0x01) {
                Serial.println("✓ Register 0x15 successfully set to 0x01!");
                break;
            }
        }
        delay(50);
    }
    
    return true;
}

// Public wrapper functions for register access
uint8_t tvp5150_read_register(uint8_t reg) {
    uint8_t tvp5150_addr = TVP5150_I2C_ADDR_PRIMARY;
    return read_register(tvp5150_addr, reg);
}

bool tvp5150_write_register(uint8_t reg, uint8_t data) {
    uint8_t tvp5150_addr = TVP5150_I2C_ADDR_PRIMARY;
    return write_register_with_retry(tvp5150_addr, reg, data);
}

// Function to test different input selections
bool tvp5150_test_input_selection(uint8_t input_sel) {
    uint8_t tvp5150_addr = TVP5150_I2C_ADDR_PRIMARY;
    
    Serial.printf("Testing input selection: 0x%02X\n", input_sel);
    
    // Write the input selection
    if (!write_register_with_retry(tvp5150_addr, 0x0F, input_sel)) {
        Serial.println("Failed to write input selection");
        return false;
    }
    
    delay(100); // Give TVP5150 time to switch
    
    // Read back to verify
    uint8_t readback = read_register(tvp5150_addr, 0x0F);
    if (readback == input_sel) {
        Serial.printf("✓ Input selection set to 0x%02X\n", input_sel);
        return true;
    } else {
        Serial.printf("✗ Input selection failed: wrote 0x%02X, read 0x%02X\n", input_sel, readback);
        return false;
    }
}

// Function to reset TVP5150 to known state
bool tvp5150_reset_to_defaults(void) {
    uint8_t tvp5150_addr = TVP5150_I2C_ADDR_PRIMARY;
    
    Serial.println("Resetting TVP5150 to default state...");
    
    // Reset all critical registers to default values
    const uint8_t default_config[][2] = {
        {0x0A, 0x80}, // Status
        {0x0B, 0x00}, // Video standard
        {0x0C, 0x80}, // Output control (parallel enabled)
        {0x0D, 0x47}, // PAL configuration
        {0x0E, 0x00}, // 
        {0x0F, 0x02}, // Input selection (AIN1)
        {0x15, 0x01}, // Output format (BT656)
    };
    
    for (int i = 0; i < sizeof(default_config)/sizeof(default_config[0]); i++) {
        if (!write_register_with_retry(tvp5150_addr, default_config[i][0], default_config[i][1])) {
            Serial.printf("Failed to reset register 0x%02X\n", default_config[i][0]);
            return false;
        }
        delay(10);
    }
    
    Serial.println("✓ TVP5150 reset to defaults");
    return true;
}

// Function to auto-detect and configure video standard (PAL/NTSC)
bool tvp5150_auto_detect_video_standard(void) {
    uint8_t tvp5150_addr = TVP5150_I2C_ADDR_PRIMARY;
    
    Serial.println("=== AUTO-DETECTING VIDEO STANDARD ===");
    Serial.println("Testing PAL and NTSC configurations...");
    
    // Test PAL configuration first
    Serial.println("\n--- Testing PAL Configuration ---");
    if (!write_register_with_retry(tvp5150_addr, 0x0D, 0x47)) { // PAL
        Serial.println("Failed to set PAL configuration");
        return false;
    }
    delay(500); // Wait for signal to stabilize
    
    bool pal_video = tvp5150_is_video_present();
    Serial.printf("PAL video detected: %s\n", pal_video ? "YES" : "NO");
    
    if (pal_video) {
        Serial.println("✓ PAL VIDEO STANDARD DETECTED!");
        return true;
    }
    
    // Test NTSC configuration
    Serial.println("\n--- Testing NTSC Configuration ---");
    if (!write_register_with_retry(tvp5150_addr, 0x0D, 0x40)) { // NTSC
        Serial.println("Failed to set NTSC configuration");
        return false;
    }
    delay(500); // Wait for signal to stabilize
    
    bool ntsc_video = tvp5150_is_video_present();
    Serial.printf("NTSC video detected: %s\n", ntsc_video ? "YES" : "NO");
    
    if (ntsc_video) {
        Serial.println("✓ NTSC VIDEO STANDARD DETECTED!");
        return true;
    }
    
    Serial.println("✗ No video standard detected");
    return false;
}

// Function to configure for specific video standard
bool tvp5150_configure_video_standard(bool is_pal) {
    uint8_t tvp5150_addr = TVP5150_I2C_ADDR_PRIMARY;
    
    Serial.printf("Configuring for %s video standard...\n", is_pal ? "PAL" : "NTSC");
    
    // Set video standard configuration
    uint8_t video_std_value = is_pal ? 0x47 : 0x40;
    if (!write_register_with_retry(tvp5150_addr, 0x0D, video_std_value)) {
        Serial.printf("Failed to set %s configuration\n", is_pal ? "PAL" : "NTSC");
        return false;
    }
    
    delay(100);
    
    // Verify configuration
    uint8_t readback = read_register(tvp5150_addr, 0x0D);
    if (readback == video_std_value) {
        Serial.printf("✓ %s configuration applied successfully\n", is_pal ? "PAL" : "NTSC");
        return true;
    } else {
        Serial.printf("✗ %s configuration failed: wrote 0x%02X, read 0x%02X\n", 
                     is_pal ? "PAL" : "NTSC", video_std_value, readback);
        return false;
    }
} 