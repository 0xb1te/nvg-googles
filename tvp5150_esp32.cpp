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