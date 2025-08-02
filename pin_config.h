// File: pin_config.h
// ESP32 WROOM-32 to TVP5150 Pin Configuration
// Based on your provided pin mapping

#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

#include "tvp5150_esp32.h"
#include "tvp5150_parallel_esp32.h"

// ============================================================================
// ESP32 WROOM-32 to TVP5150 Pin Mapping
// ============================================================================
// 
// SINGLE SOURCE OF TRUTH: All pin definitions are centralized here
// Other modules should use these constants, not define their own
// ============================================================================

// I2C Interface (for configuration and status)
#define I2C_SDA_PIN 21   // ESP32 D21 → TVP5150 Pin 4 (SDA)
#define I2C_SCL_PIN 22   // ESP32 D22 → TVP5150 Pin 3 (SCL)

// Parallel Interface (for video data capture)
// Data Bus (8-bit parallel) - Note: Your mapping shows D0-D7 in reverse order
#define TVP5150_D0_PIN 34   // ESP32 D34 → TVP5150 Pin 16 (D0)
#define TVP5150_D1_PIN 35   // ESP32 D35 → TVP5150 Pin 15 (D1)
#define TVP5150_D2_PIN 36   // ESP32 VN  → TVP5150 Pin 14 (D2)
#define TVP5150_D3_PIN 39   // ESP32 VP  → TVP5150 Pin 13 (D3)
#define TVP5150_D4_PIN 32   // ESP32 D32 → TVP5150 Pin 12 (D4)
#define TVP5150_D5_PIN 33   // ESP32 D33 → TVP5150 Pin 11 (D5)
#define TVP5150_D6_PIN 25   // ESP32 D25 → TVP5150 Pin 10 (D6)
#define TVP5150_D7_PIN 26   // ESP32 D26 → TVP5150 Pin 9  (D7)

// Control Signals
#define TVP5150_PCLK_PIN 5    // ESP32 D5  → TVP5150 Pin 7 (PCLK) - Valid interrupt pin
#define TVP5150_XCLK_PIN 4   // ESP32 D4  → TVP5150 Pin 8 (XCLK)

// Note: VSYNC and HREF are not in your mapping - they may be internal to TVP5150
// or connected differently. Using 255 (0xFF) to indicate not connected
#define TVP5150_VSYNC_PIN 255  // Not connected in your mapping
#define TVP5150_HREF_PIN  255  // Not connected in your mapping

// Power and Control
#define TVP5150_PWDN_PIN  255  // Power down (not used)
#define TVP5150_RESET_PIN 255  // Reset (not used)

// ============================================================================
// Pin Configuration Structure
// ============================================================================

// Create pin configuration structure for parallel interface
static const tvp5150_pins_t tvp5150_pin_config = {
    .d0_pin = TVP5150_D0_PIN,
    .d1_pin = TVP5150_D1_PIN,
    .d2_pin = TVP5150_D2_PIN,
    .d3_pin = TVP5150_D3_PIN,
    .d4_pin = TVP5150_D4_PIN,
    .d5_pin = TVP5150_D5_PIN,
    .d6_pin = TVP5150_D6_PIN,
    .d7_pin = TVP5150_D7_PIN,
    .vsync_pin = TVP5150_VSYNC_PIN,
    .href_pin = TVP5150_HREF_PIN,
    .pclk_pin = TVP5150_PCLK_PIN
};

// ============================================================================
// Pin Validation Functions
// ============================================================================

// Check if pin is valid for ESP32 WROOM-32
static bool is_valid_esp32_pin(uint8_t pin) {
    // ESP32 WROOM-32 valid GPIO pins: 0-39 (some are input-only)
    return pin <= 39;
}

// Check if pin is input-only on ESP32 WROOM-32
static bool is_input_only_pin(uint8_t pin) {
    // ESP32 WROOM-32 input-only pins: 34-39
    return (pin >= 34 && pin <= 39);
}

// Check if pin is connected (not 255)
static bool is_pin_connected(uint8_t pin) {
    return pin != 255;
}

// Validate all configured pins
static bool validate_pin_configuration() {
    uint8_t pins[] = {
        I2C_SDA_PIN, I2C_SCL_PIN,
        TVP5150_D0_PIN, TVP5150_D1_PIN, TVP5150_D2_PIN, TVP5150_D3_PIN,
        TVP5150_D4_PIN, TVP5150_D5_PIN, TVP5150_D6_PIN, TVP5150_D7_PIN,
        TVP5150_PCLK_PIN, TVP5150_XCLK_PIN
    };
    
    for (int i = 0; i < sizeof(pins)/sizeof(pins[0]); i++) {
        if (is_pin_connected(pins[i])) {
            if (!is_valid_esp32_pin(pins[i])) {
                Serial.printf("Invalid pin configuration: GPIO %d\n", pins[i]);
                return false;
            }
            
            // Check if data pins are input-only (which is OK for reading)
            if (pins[i] >= 34 && pins[i] <= 39) {
                Serial.printf("Warning: GPIO %d is input-only (OK for data reading)\n", pins[i]);
            }
        }
    }
    return true;
}

// ============================================================================
// Pin Information Functions
// ============================================================================

// Print pin configuration information
static void print_pin_configuration() {
    Serial.println("ESP32 WROOM-32 to TVP5150 Pin Configuration:");
    Serial.println("=============================================");
    Serial.printf("I2C SDA: GPIO %d → TVP5150 Pin 4 (SDA)\n", I2C_SDA_PIN);
    Serial.printf("I2C SCL: GPIO %d → TVP5150 Pin 3 (SCL)\n", I2C_SCL_PIN);
    Serial.println();
    Serial.println("Parallel Data Bus:");
    Serial.printf("D0: GPIO %d → TVP5150 Pin 16 (D0)\n", TVP5150_D0_PIN);
    Serial.printf("D1: GPIO %d → TVP5150 Pin 15 (D1)\n", TVP5150_D1_PIN);
    Serial.printf("D2: GPIO %d → TVP5150 Pin 14 (D2)\n", TVP5150_D2_PIN);
    Serial.printf("D3: GPIO %d → TVP5150 Pin 13 (D3)\n", TVP5150_D3_PIN);
    Serial.printf("D4: GPIO %d → TVP5150 Pin 12 (D4)\n", TVP5150_D4_PIN);
    Serial.printf("D5: GPIO %d → TVP5150 Pin 11 (D5)\n", TVP5150_D5_PIN);
    Serial.printf("D6: GPIO %d → TVP5150 Pin 10 (D6)\n", TVP5150_D6_PIN);
    Serial.printf("D7: GPIO %d → TVP5150 Pin 9  (D7)\n", TVP5150_D7_PIN);
    Serial.println();
    Serial.println("Control Signals:");
    Serial.printf("PCLK: GPIO %d → TVP5150 Pin 7 (PCLK) - Valid interrupt pin\n", TVP5150_PCLK_PIN);
    Serial.printf("XCLK: GPIO %d → TVP5150 Pin 8 (XCLK)\n", TVP5150_XCLK_PIN);
    
    if (!is_pin_connected(TVP5150_VSYNC_PIN)) {
        Serial.println("VSYNC: Not connected");
    } else {
        Serial.printf("VSYNC: GPIO %d\n", TVP5150_VSYNC_PIN);
    }
    
    if (!is_pin_connected(TVP5150_HREF_PIN)) {
        Serial.println("HREF: Not connected");
    } else {
        Serial.printf("HREF: GPIO %d\n", TVP5150_HREF_PIN);
    }
    
    Serial.println();
    Serial.println("Power:");
    Serial.println("3V3 → TVP5150 Pin 1 (VCC)");
    Serial.println("GND → TVP5150 Pin 2 (GND)");
    Serial.println();
}

// ============================================================================
// Board-Specific Configuration
// ============================================================================

// Define board type for conditional compilation
#define BOARD_ESP32_WROOM32

#ifdef BOARD_ESP32_WROOM32
    // ESP32 WROOM-32 specific settings
    #define I2C_CLOCK_SPEED 100000  // 100kHz for reliable communication
    #define PARALLEL_CLOCK_SPEED 27000000  // 27MHz for video data
#endif

#endif // PIN_CONFIG_H