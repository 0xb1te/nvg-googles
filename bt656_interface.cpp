#include "bt656_interface.h"
#include <Arduino.h>
#include "driver/gpio.h"  // For GPIO ISR service
#include "esp_err.h"      // For ESP error codes

// ============================================================================
// Global Variables
// ============================================================================



// Global interface instance for ISR access
static bt656_interface_t* g_interface = nullptr;

// Flag to track if GPIO ISR service is installed
static bool g_gpio_isr_installed = false;

// Pin definitions - use TVP5150 pin constants directly
// No need for redundant arrays since we use interface->config.data_pins

// ============================================================================
// Internal Helper Functions
// ============================================================================

// Optimized 8-bit data reading using direct register access
static inline uint8_t IRAM_ATTR read_parallel_data_optimized(const uint8_t* data_pins) {
  // Read both GPIO registers in one operation
  uint32_t gpio_in_reg = REG_READ(GPIO_IN_REG);    // GPIO 0-31
  uint32_t gpio_in1_reg = REG_READ(GPIO_IN1_REG);  // GPIO 32-39

  uint8_t data = 0;

  // Extract bits for each pin using bit masking
  for (int i = 0; i < 8; i++) {
    uint8_t pin = data_pins[i];
    if (pin != 255) {
      uint32_t bit_value;
      if (pin < 32) {
        bit_value = (gpio_in_reg >> pin) & 0x01;
      } else {
        bit_value = (gpio_in1_reg >> (pin - 32)) & 0x01;
      }
      data |= (bit_value << i);
    }
  }

  return data;
}

// Add data to circular buffer (ISR-safe, simplified)
static inline bool IRAM_ATTR add_to_buffer_optimized(bt656_interface_t* interface, uint8_t data) {
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
// Interrupt Service Routines
// ============================================================================



// IRAM-safe ultra-fast ISR with minimal operations
// This ISR is placed in IRAM to avoid flash access during execution
// Following ESP32 best practices from: https://lastminuteengineers.com/handling-esp32-gpio-interrupts-tutorial/
// Based on forum post: https://forum.arduino.cc/t/esp32-using-a-basic-interrupt-crashes/954541/4
void IRAM_ATTR bt656_pclk_isr_optimized(void* arg) {
  // CRITICAL: No Serial calls, no complex operations, no delays
  // This ISR must be as fast as possible to avoid watchdog timeouts

  if (!g_interface || !g_interface->interrupt_enabled) {
    return;
  }

  // Read data using optimized function (direct register access)
  uint8_t data = read_parallel_data_optimized(g_interface->config.data_pins);

  // Add to buffer (minimal processing)
  if (add_to_buffer_optimized(g_interface, data)) {
    g_interface->stats.bytes_captured++;
  }

  // Simple counter increment (no complex operations)
  g_interface->stats.interrupts_handled++;

  // CRITICAL: Return immediately - no delays, no Serial, no complex logic
}

// Alternative ISR with direct BT656 processing (for advanced users)
void IRAM_ATTR bt656_pclk_isr_direct(void* arg) {
  if (!g_interface || !g_interface->interrupt_enabled || !g_interface->decoder) {
    return;
  }

  // Read data using optimized function
  uint8_t data = read_parallel_data_optimized(g_interface->config.data_pins);

  // Process BT656 data directly in ISR (minimal processing)
  // This is more advanced and requires careful testing
  bt656_decoder_process_byte(g_interface->decoder, data);

  g_interface->stats.interrupts_handled++;
  g_interface->stats.bytes_captured++;
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

  // Install GPIO ISR service (only once during initialization)
  // CRITICAL: Multiple installations can cause kernel panics
  // Based on: https://github.com/espressif/esp32-camera/pull/145
  if (!g_gpio_isr_installed) {
    esp_err_t ret = gpio_install_isr_service(0);
    if (ret == ESP_OK) {
      g_gpio_isr_installed = true;
      Serial.println("GPIO ISR service installed successfully");
    } else if (ret == ESP_ERR_INVALID_STATE) {
      // Service already installed - this is normal and expected
      g_gpio_isr_installed = true;
      Serial.println("GPIO ISR service already installed (normal)");
    } else {
      Serial.printf("ERROR: Failed to install GPIO ISR service: %s\n", esp_err_to_name(ret));
      Serial.println("This may cause kernel panics - check system stability");
      return false;
    }
  }

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

  // Configure GPIO pins (this is the ONLY place data pins should be configured)
  // The parallel interface reads data pins but doesn't configure them to avoid conflicts
  for (int i = 0; i < 8; i++) {
    if (interface->config.data_pins[i] != 255) {
      pinMode(interface->config.data_pins[i], INPUT);
      Serial.printf("Data pin %d configured: GPIO %d\n", i, interface->config.data_pins[i]);
    }
  }

  // Configure pixel clock pin (this is the ONLY place PCLK should be configured)
  // The parallel interface reads PCLK but doesn't configure it to avoid conflicts
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

  // Attach interrupt if enabled and PCLK pin is set
  if (interface->config.enable_interrupts && interface->config.pclk_pin != 255) {
    int interrupt_number = digitalPinToInterrupt(interface->config.pclk_pin);
    if (interrupt_number == NOT_AN_INTERRUPT) {
      Serial.println("ERROR: Pin is not valid for interrupts!");
      return false;
    }

    // Detach any previous interrupt and attach new one
    detachInterrupt(interrupt_number);
    delay(10);
    attachInterruptArg(interrupt_number, bt656_pclk_isr_optimized, interface, RISING);
    interface->interrupt_enabled = true;
    Serial.printf("BT656 interrupt attached to GPIO %d\n", interface->config.pclk_pin);
  } else {
    interface->interrupt_enabled = false;
    Serial.println("BT656 interface initialized in polling mode");
  }

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

// bt656_interface_start() REMOVED - functionality merged into bt656_interface_init()
// This eliminates redundancy and simplifies the API

// ============================================================================
// Safety Functions
// ============================================================================

// Safely check GPIO ISR service status without causing kernel panics
bool bt656_interface_verify_gpio_isr_service() {
  if (!g_gpio_isr_installed) {
    Serial.println("WARNING: GPIO ISR service not marked as installed");
    return false;
  }

  // Try to install the service - if it's already installed, we get ESP_ERR_INVALID_STATE
  esp_err_t ret = gpio_install_isr_service(0);
  if (ret == ESP_ERR_INVALID_STATE) {
    Serial.println("✓ GPIO ISR service is properly installed");
    return true;
  } else if (ret == ESP_OK) {
    Serial.println("✓ GPIO ISR service installed successfully");
    return true;
  } else {
    Serial.printf("✗ GPIO ISR service verification failed: %s\n", esp_err_to_name(ret));
    return false;
  }
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

    // Note: Cannot restart automatically since bt656_interface_start() was removed
    // User must call bt656_interface_init() again if they want to change configuration
    if (was_running) {
      Serial.println("WARNING: Interface was running - call bt656_interface_init() to restart with new config");
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
// Polling Mode Functions (for when interrupts fail)
// ============================================================================

void bt656_interface_poll_data(bt656_interface_t* interface) {
  if (!interface || !interface->data_buffer || interface->interrupt_enabled) {
    return;  // Only poll if interrupts are disabled
  }

  // Read current PCLK state
  bool pclk_high = digitalRead(interface->config.pclk_pin) == HIGH;

  // Simple edge detection (rising edge)
  static bool last_pclk_state = false;
  if (pclk_high && !last_pclk_state) {
    // Rising edge detected - read data
    uint8_t data = read_parallel_data_optimized(interface->config.data_pins);

    // Add to buffer
    if (add_to_buffer_optimized(interface, data)) {
      interface->stats.bytes_captured++;
    }

    interface->stats.interrupts_handled++;  // Count as "interrupt" for stats
  }

  last_pclk_state = pclk_high;
}

// Debug function to print raw bytes from D0-D7 pins
void bt656_interface_print_raw_data(bt656_interface_t* interface, uint32_t sample_count) {
  if (!interface) return;

  Serial.println("=== RAW D0-D7 DATA CAPTURE ===");
  Serial.printf("Sampling %lu bytes from parallel pins...\n", sample_count);
  Serial.println("Format: [Sample#] 0xXX (Binary: D7D6D5D4D3D2D1D0)");
  Serial.println("----------------------------------------");

  uint32_t samples_taken = 0;
  uint32_t start_time = millis();

  while (samples_taken < sample_count && (millis() - start_time) < 5000) {  // 5 second timeout
    // Read raw data from parallel pins
    uint8_t data = read_parallel_data_optimized(interface->config.data_pins);

    // Print in hex and binary format
    Serial.printf("[%4lu] 0x%02X (Binary: %c%c%c%c%c%c%c%c)\n",
                  samples_taken,
                  data,
                  (data & 0x80) ? '1' : '0',   // D7
                  (data & 0x40) ? '1' : '0',   // D6
                  (data & 0x20) ? '1' : '0',   // D5
                  (data & 0x10) ? '1' : '0',   // D4
                  (data & 0x08) ? '1' : '0',   // D3
                  (data & 0x04) ? '1' : '0',   // D2
                  (data & 0x02) ? '1' : '0',   // D1
                  (data & 0x01) ? '1' : '0');  // D0

    samples_taken++;
    delay(1);  // Small delay to make output readable
  }

  Serial.printf("Captured %lu samples in %lu ms\n", samples_taken, millis() - start_time);
  Serial.println("========================================");
}

// Quick debug function to show current pin states
void bt656_interface_print_pin_states(bt656_interface_t* interface) {
  if (!interface) return;

  Serial.println("=== CURRENT D0-D7 PIN STATES ===");
  Serial.println("Pin | GPIO | State | Binary");
  Serial.println("----|------|-------|--------");

  uint8_t data = read_parallel_data_optimized(interface->config.data_pins);

  for (int i = 0; i < 8; i++) {
    uint8_t pin = interface->config.data_pins[i];
    bool state = (data & (1 << i)) != 0;
    Serial.printf("D%d  | %4d  | %s    | %c\n",
                  i, pin, state ? "HIGH" : "LOW", state ? '1' : '0');
  }

  Serial.printf("Raw byte: 0x%02X\n", data);
  Serial.println("================================");
}

// Function to look for specific BT656 patterns from Verilog implementation
void bt656_interface_look_for_verilog_patterns(bt656_interface_t* interface, uint32_t sample_count) {
  if (!interface) return;

  Serial.println("=== LOOKING FOR VERILOG BT656 PATTERNS ===");
  Serial.println("Expected patterns from working Verilog:");
  Serial.println("- FF 00 00 (timing reference)");
  Serial.println("- 80 or C7 (SAV markers)");
  Serial.println("- 9D or F1 (EAV markers)");
  Serial.println("----------------------------------------");

  uint32_t samples_taken = 0;
  uint32_t ff_count = 0;
  uint32_t sav_count = 0;
  uint32_t eav_count = 0;
  uint32_t timing_ref_count = 0;

  // State for timing reference detection
  uint8_t state = 0;  // 0=idle, 1=FF, 2=FF00, 3=FF0000

  uint32_t start_time = millis();

  while (samples_taken < sample_count && (millis() - start_time) < 10000) {  // 10 second timeout
    uint8_t data = read_parallel_data_optimized(interface->config.data_pins);

    // Count specific patterns
    if (data == 0xFF) ff_count++;
    if (data == 0x80 || data == 0xC7) sav_count++;
    if (data == 0x9D || data == 0xF1) eav_count++;

    // Detect timing reference pattern (FF 00 00)
    switch (state) {
      case 0:  // idle
        if (data == 0xFF) state = 1;
        break;
      case 1:  // FF
        if (data == 0x00) state = 2;
        else state = 0;
        break;
      case 2:  // FF00
        if (data == 0x00) {
          state = 0;
          timing_ref_count++;
          Serial.printf("TIMING REFERENCE FOUND at sample %lu!\n", samples_taken);
        } else {
          state = 0;
        }
        break;
    }

    // Print interesting data
    if (data == 0xFF || data == 0x80 || data == 0xC7 || data == 0x9D || data == 0xF1) {
      Serial.printf("[%4lu] 0x%02X - ", samples_taken, data);
      if (data == 0xFF) Serial.println("FF (timing ref start)");
      else if (data == 0x80 || data == 0xC7) Serial.println("SAV marker");
      else if (data == 0x9D || data == 0xF1) Serial.println("EAV marker");
    }

    samples_taken++;
    delay(1);
  }

  Serial.printf("Pattern Analysis Results:\n");
  Serial.printf("- FF bytes: %lu\n", ff_count);
  Serial.printf("- SAV markers: %lu\n", sav_count);
  Serial.printf("- EAV markers: %lu\n", eav_count);
  Serial.printf("- Complete timing refs: %lu\n", timing_ref_count);
  Serial.printf("- Total samples: %lu\n", samples_taken);

  if (timing_ref_count > 0) {
    Serial.println("✓ VALID BT656 STREAM DETECTED!");
  } else {
    Serial.println("✗ No valid BT656 timing references found");
  }

  Serial.println("========================================");
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

  uint8_t temp_buffer[64];  // Temporary buffer for processing
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
  bt656_interface_stats_t empty_stats = { 0 };
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
  Serial.printf("Mode: %s\n", interface->interrupt_enabled ? "INTERRUPT" : "POLLING");
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

