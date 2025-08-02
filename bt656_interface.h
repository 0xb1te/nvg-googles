#ifndef BT656_INTERFACE_H
#define BT656_INTERFACE_H

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>
#include "bt656_decoder.h"
#include "pin_config.h"

// ============================================================================
// BT656 Interface Configuration
// ============================================================================

// Interrupt configuration
#define BT656_INTERRUPT_PRIORITY   1        // High priority interrupt
#define BT656_BUFFER_SIZE          1024     // Circular buffer size
#define BT656_MAX_SAMPLES_PER_ISR  8        // Max samples to process per ISR

// Pin definitions (from pin_config.h)
extern const uint8_t BT656_DATA_PINS[8];    // D0-D7 pins
extern const uint8_t BT656_PCLK_PIN;        // Pixel clock pin

// ============================================================================
// Data Structures
// ============================================================================

// BT656 interface configuration
typedef struct {
    uint8_t data_pins[8];          // Data pins D0-D7
    uint8_t pclk_pin;              // Pixel clock pin
    uint8_t interrupt_priority;     // Interrupt priority
    uint32_t buffer_size;          // Circular buffer size
    bool enable_interrupts;        // Enable interrupt-driven capture
    bool enable_debug_output;      // Enable debug serial output
} bt656_interface_config_t;

// BT656 interface statistics
typedef struct {
    uint32_t interrupts_handled;   // Total interrupts handled
    uint32_t bytes_captured;       // Total bytes captured
    uint32_t buffer_overflows;     // Buffer overflow count
    uint32_t missed_samples;       // Missed samples count
    uint32_t isr_execution_time;   // Average ISR execution time (us)
    uint64_t last_interrupt_time;  // Timestamp of last interrupt
} bt656_interface_stats_t;

// BT656 interface instance
typedef struct {
    bt656_interface_config_t config;        // Interface configuration
    bt656_interface_stats_t stats;          // Interface statistics
    bt656_decoder_t* decoder;               // BT656 decoder instance
    
    // Circular buffer for data
    uint8_t* data_buffer;                   // Data buffer
    volatile uint32_t buffer_head;          // Buffer head (write position)
    volatile uint32_t buffer_tail;          // Buffer tail (read position)
    volatile bool buffer_full;              // Buffer full flag
    
    // Interrupt handling
    volatile bool interrupt_enabled;        // Interrupt enabled flag
    volatile uint32_t isr_count;            // ISR execution counter
    volatile uint64_t last_isr_time;        // Last ISR timestamp
    
    // Callback functions
    void (*data_ready_callback)(uint8_t* data, uint32_t count);
    void (*error_callback)(uint32_t error_code);
} bt656_interface_t;

// ============================================================================
// Function Prototypes
// ============================================================================

// Core interface functions
bool bt656_interface_init(bt656_interface_t* interface, const bt656_interface_config_t* config);
void bt656_interface_deinit(bt656_interface_t* interface);
bool bt656_interface_start(bt656_interface_t* interface);
void bt656_interface_stop(bt656_interface_t* interface);
bool bt656_interface_is_running(bt656_interface_t* interface);

// Configuration functions
void bt656_interface_set_config(bt656_interface_t* interface, const bt656_interface_config_t* config);
void bt656_interface_set_decoder(bt656_interface_t* interface, bt656_decoder_t* decoder);
void bt656_interface_set_data_callback(bt656_interface_t* interface, void (*callback)(uint8_t* data, uint32_t count));
void bt656_interface_set_error_callback(bt656_interface_t* interface, void (*callback)(uint32_t error_code));

// Data processing functions
uint32_t bt656_interface_read_data(bt656_interface_t* interface, uint8_t* buffer, uint32_t max_count);
uint32_t bt656_interface_get_available_data(bt656_interface_t* interface);
void bt656_interface_process_buffer(bt656_interface_t* interface);

// Status and statistics functions
bt656_interface_stats_t bt656_interface_get_stats(bt656_interface_t* interface);
void bt656_interface_reset_stats(bt656_interface_t* interface);
void bt656_interface_print_stats(bt656_interface_t* interface);

// Interrupt service routine (IRAM_ATTR for fast execution)
void IRAM_ATTR bt656_pclk_isr(void* arg);

// Utility functions
bool bt656_interface_validate_pins(const bt656_interface_config_t* config);
void bt656_interface_print_config(const bt656_interface_config_t* config);

// ============================================================================
// Default Configuration
// ============================================================================

// Default BT656 interface configuration
static const bt656_interface_config_t BT656_DEFAULT_CONFIG = {
    .data_pins = {TVP5150_D0_PIN, TVP5150_D1_PIN, TVP5150_D2_PIN, TVP5150_D3_PIN,
                  TVP5150_D4_PIN, TVP5150_D5_PIN, TVP5150_D6_PIN, TVP5150_D7_PIN},
    .pclk_pin = TVP5150_PCLK_PIN,
    .interrupt_priority = BT656_INTERRUPT_PRIORITY,
    .buffer_size = BT656_BUFFER_SIZE,
    .enable_interrupts = true,
    .enable_debug_output = false
};

#endif // BT656_INTERFACE_H 