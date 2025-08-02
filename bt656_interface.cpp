#include "bt656_interface.h"
#include <Arduino.h>

// ============================================================================
// Global Variables
// ============================================================================

// Global interface instance for ISR access
static bt656_interface_t* g_interface = nullptr;

// Pin definitions
const uint8_t BT656_DATA_PINS[8] = {
    TVP5150_D0_PIN, TVP5150_D1_PIN, TVP5150_D2_PIN, TVP5150_D3_PIN,
    TVP5150_D4_PIN, TVP5150_D5_PIN, TVP5150_D6_PIN, TVP5150_D7_PIN
};

const uint8_t BT656_PCLK_PIN = TVP5150_PCLK_PIN;

// ============================================================================
// Internal Helper Functions
// ============================================================================

// Read 8-bit data from parallel pins (optimized for speed)
static inline uint8_t IRAM_ATTR read_parallel_data(const uint8_t* data_pins) {
    // Use Arduino digitalRead for compatibility
    uint8_t data = 0;
    data |= (digitalRead(data_pins[0]) & 0x01) << 0;
    data |= (digitalRead(data_pins[1]) & 0x01) << 1;
    data |= (digitalRead(data_pins[2]) & 0x01) << 2;
    data |= (digitalRead(data_pins[3]) & 0x01) << 3;
    data |= (digitalRead(data_pins[4]) & 0x01) << 4;
    data |= (digitalRead(data_pins[5]) & 0x01) << 5;
    data |= (digitalRead(data_pins[6]) & 0x01) << 6;
    data |= (digitalRead(data_pins[7]) & 0x01) << 7;
    
    return data;
}

// Add data to circular buffer (ISR-safe)
static inline bool IRAM_ATTR add_to_buffer(bt656_interface_t* interface, uint8_t data) {
    if (interface->buffer_full) {
        interface->stats.buffer_overflows++;
        return false;
    }
    
    interface->data_buffer[interface->buffer_head] = data;
    interface->buffer_head = (interface->buffer_head + 1) % interface->config.buffer_size;
    
    if (interface->buffer_head == interface->buffer_tail) {
        interface->buffer_full = true;
    }
    
    return true;
}

// ============================================================================
// Interrupt Service Routine
// ============================================================================

void IRAM_ATTR bt656_pclk_isr(void* arg) {
    if (!g_interface || !g_interface->interrupt_enabled) {
        return;
    }
    
    uint64_t isr_start_time = esp_timer_get_time();
    
    // Read data from parallel pins
    uint8_t data = read_parallel_data(g_interface->config.data_pins);
    
    // Add to circular buffer
    if (add_to_buffer(g_interface, data)) {
        g_interface->stats.bytes_captured++;
    }
    
    // Process data through decoder if available
    if (g_interface->decoder) {
        bt656_decoder_process_byte(g_interface->decoder, data);
    }
    
    // Update ISR statistics
    g_interface->isr_count++;
    g_interface->last_isr_time = isr_start_time;
    g_interface->stats.interrupts_handled++;
    g_interface->stats.last_interrupt_time = isr_start_time;
    
    // Calculate ISR execution time
    uint64_t isr_end_time = esp_timer_get_time();
    uint32_t execution_time = isr_end_time - isr_start_time;
    g_interface->stats.isr_execution_time = 
        (g_interface->stats.isr_execution_time + execution_time) / 2; // Running average
}

// ============================================================================
// Core Interface Functions
// ============================================================================

bool bt656_interface_init(bt656_interface_t* interface, const bt656_interface_config_t* config) {
    if (!interface) {
        Serial.println("ERROR: Invalid interface pointer");
        return false;
    }
    
    // Initialize interface structure
    memset(interface, 0, sizeof(bt656_interface_t));
    
    // Set configuration
    if (config) {
        interface->config = *config;
    } else {
        interface->config = BT656_DEFAULT_CONFIG;
    }
    
    // Validate pin configuration
    if (!bt656_interface_validate_pins(&interface->config)) {
        Serial.println("ERROR: Invalid pin configuration");
        return false;
    }
    
    // Allocate circular buffer
    interface->data_buffer = (uint8_t*)malloc(interface->config.buffer_size);
    if (!interface->data_buffer) {
        Serial.println("ERROR: Failed to allocate data buffer");
        return false;
    }
    
    // Initialize buffer state
    interface->buffer_head = 0;
    interface->buffer_tail = 0;
    interface->buffer_full = false;
    
    // Configure GPIO pins
    for (int i = 0; i < 8; i++) {
        if (interface->config.data_pins[i] != 255) {
            pinMode(interface->config.data_pins[i], INPUT);
            Serial.printf("Data pin %d configured: GPIO %d\n", i, interface->config.data_pins[i]);
        }
    }
    
    // Configure pixel clock pin
    if (interface->config.pclk_pin != 255) {
        pinMode(interface->config.pclk_pin, INPUT);
        Serial.printf("PCLK pin configured: GPIO %d\n", interface->config.pclk_pin);
    }
    
    // Set global interface pointer for ISR
    g_interface = interface;
    
    // Initialize callbacks to NULL
    interface->data_ready_callback = nullptr;
    interface->error_callback = nullptr;
    interface->decoder = nullptr;
    
    Serial.println("BT656 interface initialized successfully");
    return true;
}

void bt656_interface_deinit(bt656_interface_t* interface) {
    if (!interface) return;
    
    // Stop interface if running
    bt656_interface_stop(interface);
    
    // Free buffer
    if (interface->data_buffer) {
        free(interface->data_buffer);
        interface->data_buffer = nullptr;
    }
    
    // Clear global pointer
    if (g_interface == interface) {
        g_interface = nullptr;
    }
    
    Serial.println("BT656 interface deinitialized");
}

bool bt656_interface_start(bt656_interface_t* interface) {
    if (!interface || !interface->data_buffer) {
        Serial.println("ERROR: Interface not initialized");
        return false;
    }
    
    // Reset buffer state
    interface->buffer_head = 0;
    interface->buffer_tail = 0;
    interface->buffer_full = false;
    
    // Reset statistics
    bt656_interface_reset_stats(interface);
    
    // Attach interrupt if enabled
    if (interface->config.enable_interrupts && interface->config.pclk_pin != 255) {
        attachInterruptArg(digitalPinToInterrupt(interface->config.pclk_pin),
                          bt656_pclk_isr, interface, RISING);
        
        interface->interrupt_enabled = true;
        Serial.printf("BT656 interrupt attached to GPIO %d\n", interface->config.pclk_pin);
    }
    
    Serial.println("BT656 interface started");
    return true;
}

void bt656_interface_stop(bt656_interface_t* interface) {
    if (!interface) return;
    
    // Disable interrupt
    if (interface->interrupt_enabled && interface->config.pclk_pin != 255) {
        detachInterrupt(digitalPinToInterrupt(interface->config.pclk_pin));
        interface->interrupt_enabled = false;
        Serial.printf("BT656 interrupt detached from GPIO %d\n", interface->config.pclk_pin);
    }
    
    Serial.println("BT656 interface stopped");
}

bool bt656_interface_is_running(bt656_interface_t* interface) {
    return interface ? interface->interrupt_enabled : false;
}

// ============================================================================
// Configuration Functions
// ============================================================================

void bt656_interface_set_config(bt656_interface_t* interface, const bt656_interface_config_t* config) {
    if (interface && config) {
        // Stop interface if running
        bool was_running = bt656_interface_is_running(interface);
        if (was_running) {
            bt656_interface_stop(interface);
        }
        
        interface->config = *config;
        
        // Restart if it was running
        if (was_running) {
            bt656_interface_start(interface);
        }
    }
}

void bt656_interface_set_decoder(bt656_interface_t* interface, bt656_decoder_t* decoder) {
    if (interface) {
        interface->decoder = decoder;
    }
}

void bt656_interface_set_data_callback(bt656_interface_t* interface, void (*callback)(uint8_t* data, uint32_t count)) {
    if (interface) {
        interface->data_ready_callback = callback;
    }
}

void bt656_interface_set_error_callback(bt656_interface_t* interface, void (*callback)(uint32_t error_code)) {
    if (interface) {
        interface->error_callback = callback;
    }
}

// ============================================================================
// Data Processing Functions
// ============================================================================

uint32_t bt656_interface_read_data(bt656_interface_t* interface, uint8_t* buffer, uint32_t max_count) {
    if (!interface || !buffer || !max_count) {
        return 0;
    }
    
    uint32_t count = 0;
    
    // Disable interrupts during buffer access
    portDISABLE_INTERRUPTS();
    
    while (count < max_count && interface->buffer_tail != interface->buffer_head) {
        buffer[count] = interface->data_buffer[interface->buffer_tail];
        interface->buffer_tail = (interface->buffer_tail + 1) % interface->config.buffer_size;
        interface->buffer_full = false;
        count++;
    }
    
    // Re-enable interrupts
    portENABLE_INTERRUPTS();
    
    return count;
}

uint32_t bt656_interface_get_available_data(bt656_interface_t* interface) {
    if (!interface) return 0;
    
    // Disable interrupts during calculation
    portDISABLE_INTERRUPTS();
    
    uint32_t available;
    if (interface->buffer_full) {
        available = interface->config.buffer_size;
    } else if (interface->buffer_head >= interface->buffer_tail) {
        available = interface->buffer_head - interface->buffer_tail;
    } else {
        available = interface->config.buffer_size - interface->buffer_tail + interface->buffer_head;
    }
    
    // Re-enable interrupts
    portENABLE_INTERRUPTS();
    
    return available;
}

void bt656_interface_process_buffer(bt656_interface_t* interface) {
    if (!interface) return;
    
    uint8_t temp_buffer[64]; // Temporary buffer for processing
    uint32_t count = bt656_interface_read_data(interface, temp_buffer, sizeof(temp_buffer));
    
    if (count > 0 && interface->data_ready_callback) {
        interface->data_ready_callback(temp_buffer, count);
    }
}

// ============================================================================
// Status and Statistics Functions
// ============================================================================

bt656_interface_stats_t bt656_interface_get_stats(bt656_interface_t* interface) {
    if (interface) {
        return interface->stats;
    }
    bt656_interface_stats_t empty_stats = {0};
    return empty_stats;
}

void bt656_interface_reset_stats(bt656_interface_t* interface) {
    if (interface) {
        memset(&interface->stats, 0, sizeof(bt656_interface_stats_t));
    }
}

void bt656_interface_print_stats(bt656_interface_t* interface) {
    if (!interface) return;
    
    Serial.println("=== BT656 Interface Statistics ===");
    Serial.printf("Interrupts Handled: %lu\n", interface->stats.interrupts_handled);
    Serial.printf("Bytes Captured: %lu\n", interface->stats.bytes_captured);
    Serial.printf("Buffer Overflows: %lu\n", interface->stats.buffer_overflows);
    Serial.printf("Missed Samples: %lu\n", interface->stats.missed_samples);
    Serial.printf("Avg ISR Time: %lu us\n", interface->stats.isr_execution_time);
    Serial.printf("Last Interrupt: %llu us\n", interface->stats.last_interrupt_time);
    Serial.printf("Available Data: %lu\n", bt656_interface_get_available_data(interface));
    Serial.printf("Buffer Full: %s\n", interface->buffer_full ? "YES" : "NO");
    Serial.printf("Interrupt Enabled: %s\n", interface->interrupt_enabled ? "YES" : "NO");
    Serial.println("==================================");
}

// ============================================================================
// Utility Functions
// ============================================================================

bool bt656_interface_validate_pins(const bt656_interface_config_t* config) {
    if (!config) return false;
    
    // Check data pins
    for (int i = 0; i < 8; i++) {
        if (config->data_pins[i] != 255) {
            if (config->data_pins[i] > 39) {
                Serial.printf("ERROR: Invalid data pin %d: GPIO %d\n", i, config->data_pins[i]);
                return false;
            }
        }
    }
    
    // Check pixel clock pin
    if (config->pclk_pin != 255) {
        if (config->pclk_pin > 39) {
            Serial.printf("ERROR: Invalid PCLK pin: GPIO %d\n", config->pclk_pin);
            return false;
        }
    }
    
    return true;
}

void bt656_interface_print_config(const bt656_interface_config_t* config) {
    if (!config) return;
    
    Serial.println("=== BT656 Interface Configuration ===");
    Serial.println("Data Pins:");
    for (int i = 0; i < 8; i++) {
        Serial.printf("  D%d: GPIO %d\n", i, config->data_pins[i]);
    }
    Serial.printf("PCLK Pin: GPIO %d\n", config->pclk_pin);
    Serial.printf("Interrupt Priority: %d\n", config->interrupt_priority);
    Serial.printf("Buffer Size: %lu\n", config->buffer_size);
    Serial.printf("Interrupts Enabled: %s\n", config->enable_interrupts ? "YES" : "NO");
    Serial.printf("Debug Output: %s\n", config->enable_debug_output ? "YES" : "NO");
    Serial.println("=====================================");
} 