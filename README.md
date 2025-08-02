# BT656 Video Decoder for ESP32

This implementation provides a complete BT656 video decoder for the ESP32, designed to work with the TVP5150 video decoder chip. The system captures BT656 video data at 27 MHz and processes it into usable video frames.

## Overview

The BT656 decoder implementation consists of several components:

1. **BT656 Decoder** (`bt656_decoder.h/cpp`) - Core decoding logic
2. **BT656 Interface** (`bt656_interface.h/cpp`) - High-speed data capture
3. **BT656 Example** (`bt656_example.h/cpp`) - Frame buffer management
4. **Main Application** (`main_esp32.ino`) - Complete integration

## Hardware Requirements

### ESP32 WROOM-32 Pin Connections

```
ESP32 WROOM-32 Pin → TVP5150 Pin
─────────────────────────────────
D34  → Pin 16 (D0)    Video Data
D35  → Pin 15 (D1)    Video Data  
VN   → Pin 14 (D2)    Video Data
VP   → Pin 13 (D3)    Video Data
D32  → Pin 12 (D4)    Video Data
D33  → Pin 11 (D5)    Video Data
D25  → Pin 10 (D6)    Video Data
D26  → Pin 9  (D7)    Video Data
D27  → Pin 7  (PCLK)  Pixel Clock
D4   → Pin 8  (XCLK)  Master Clock
D22  → Pin 3  (SCL)   I2C Clock
D21  → Pin 4  (SDA)   I2C Data
3V3  → Pin 1  (VCC)   Power
GND  → Pin 2  (GND)   Ground
```

## BT656 Video Format

### Data Stream Structure

The BT656 data stream operates at 27 MHz and contains:

1. **Timing Reference**: `FF 00 00` followed by control byte
2. **Active Video**: Y, Cb, Y, Cr pixel data (4:2:2 format)
3. **Sync Signals**: Embedded in timing reference control byte

### Timing Reference Detection

```cpp
// Look for FF 00 00 pattern
if (data == 0xFF) → if (data == 0x00) → if (data == 0x00)
    // Timing reference found, next byte is control byte
```

### Sync Signal Extraction

```cpp
// Control byte bit positions:
// Bit 6: Field indicator (odd/even)
// Bit 5: VSYNC (vertical sync)
// Bit 4: HSYNC (horizontal sync)
// Bit 3: SAV/EAV indicator
```

### Video Data Format (4:2:2 YCbCr)

```
Data Stream: Y1, Cb, Y2, Cr, Y1, Cb, Y2, Cr, ...
Phase:       Y1 → CB → Y2 → CR → Y1 → CB → Y2 → CR
```

## Usage Examples

### Basic BT656 Decoder Setup

```cpp
#include "bt656_decoder.h"
#include "bt656_interface.h"

// Initialize decoder
bt656_decoder_t decoder;
bt656_config_t config = {
    .expected_width = 720,      // PAL active pixels
    .expected_height = 576,     // PAL active lines
    .enable_rgb_conversion = true,
    .enable_frame_buffer = false,
    .output_format = 1          // RGB
};

bt656_decoder_init(&decoder, &config);

// Set up callbacks
bt656_decoder_set_pixel_callback(&decoder, on_ycbcr_pixel);
bt656_decoder_set_rgb_callback(&decoder, on_rgb_pixel);
bt656_decoder_set_frame_callback(&decoder, on_frame_start);
bt656_decoder_set_line_callback(&decoder, on_line_start);
```

### High-Speed Interface Setup

```cpp
#include "bt656_interface.h"

// Initialize interface
bt656_interface_t interface;
bt656_interface_config_t interface_config = BT656_DEFAULT_CONFIG;

bt656_interface_init(&interface, &interface_config);

// Connect decoder to interface
bt656_interface_set_decoder(&interface, &decoder);

// Interface is now ready to capture data (interrupts automatically enabled)
```

### Frame Buffer Management

```cpp
#include "bt656_example.h"

// Initialize video processing
video_processing_config_t processing_config = DEFAULT_PROCESSING_CONFIG;
video_processing_init(&processing_config);

// Access frame data
frame_buffer_t* buffer = &g_frame_buffer;
uint16_t* rgb565_data = frame_buffer_get_rgb565(buffer);
uint8_t* ycbcr_data = frame_buffer_get_ycbcr(buffer);
```

## Callback Functions

### YCbCr Pixel Callback

```cpp
void on_ycbcr_pixel(bt656_ycbcr_t* pixel, uint16_t x, uint16_t y) {
    // Process YCbCr pixel at position (x, y)
    uint8_t y = pixel->y;   // Luminance
    uint8_t cb = pixel->cb; // Chrominance Blue
    uint8_t cr = pixel->cr; // Chrominance Red
    
    // Store in frame buffer or process further
}
```

### RGB Pixel Callback

```cpp
void on_rgb_pixel(bt656_rgb_t* pixel, uint16_t x, uint16_t y) {
    // Process RGB pixel at position (x, y)
    uint8_t r = pixel->r; // Red component
    uint8_t g = pixel->g; // Green component
    uint8_t b = pixel->b; // Blue component
    
    // Convert to RGB565 for display
    uint16_t rgb565 = bt656_rgb_to_rgb565(*pixel);
}
```

### Frame Callback

```cpp
void on_frame_start() {
    // Called when a new frame starts
    Serial.println("New frame started");
    
    // Process complete frame
    if (frame_buffer_is_ready(&g_frame_buffer)) {
        video_processing_process_frame(&g_frame_buffer);
    }
}
```

## Color Space Conversion

### YCbCr to RGB Conversion

```cpp
bt656_ycbcr_t ycbcr = {y: 128, cb: 128, cr: 128};
bt656_rgb_t rgb = bt656_ycbcr_to_rgb(ycbcr);

// Result: rgb.r, rgb.g, rgb.b contain RGB values
```

### RGB to RGB565 Conversion

```cpp
bt656_rgb_t rgb = {r: 255, g: 128, b: 64};
uint16_t rgb565 = bt656_rgb_to_rgb565(rgb);

// Result: 16-bit RGB565 value for display
```

### YCbCr to Grayscale

```cpp
bt656_ycbcr_t ycbcr = {y: 128, cb: 128, cr: 128};
uint8_t gray = bt656_ycbcr_to_grayscale(ycbcr);

// Result: 8-bit grayscale value
```

## Performance Considerations

### Interrupt Service Routine (ISR)

The BT656 interface uses a high-priority interrupt to capture data at 27 MHz:

- **ISR Execution Time**: Typically < 5 microseconds
- **Buffer Size**: Configurable (default: 1024 bytes)
- **Interrupt Priority**: Level 1 (high priority)

### Memory Usage

For a PAL frame (720x576):

- **YCbCr Buffer**: 1,244,160 bytes (720 × 576 × 3)
- **RGB Buffer**: 1,244,160 bytes (720 × 576 × 3)
- **RGB565 Buffer**: 829,440 bytes (720 × 576 × 2)
- **Grayscale Buffer**: 414,720 bytes (720 × 576)

**Total Memory**: ~3.7 MB for all formats

### Frame Rate

- **PAL Standard**: 25 fps (40ms per frame)
- **Data Rate**: 27 MHz pixel clock
- **Active Video**: 720 × 576 pixels per frame

## Configuration Options

### BT656 Decoder Configuration

```cpp
bt656_config_t config = {
    .expected_width = 720,           // Expected video width
    .expected_height = 576,          // Expected video height
    .enable_rgb_conversion = true,   // Enable YCbCr→RGB conversion
    .enable_frame_buffer = false,    // Enable frame buffering
    .output_format = 1               // 0=YCbCr, 1=RGB, 2=Grayscale
};
```

### BT656 Interface Configuration

```cpp
bt656_interface_config_t config = {
    .data_pins = {34, 35, 36, 39, 32, 33, 25, 26}, // D0-D7 pins
    .pclk_pin = 27,                                 // Pixel clock pin
    .interrupt_priority = 1,                        // ISR priority
    .buffer_size = 1024,                           // Circular buffer size
    .enable_interrupts = true,                      // Enable interrupts
    .enable_debug_output = false                   // Enable debug output
};
```

## Troubleshooting

### Common Issues

1. **No Video Data**: Check TVP5150 I2C connection and power supply
2. **Buffer Overflows**: Increase buffer size or reduce processing load
3. **Sync Errors**: Verify pixel clock connection and timing
4. **Memory Issues**: Reduce frame buffer size or use external memory

### Debug Output

Enable debug output to monitor system performance:

```cpp
// Enable debug in interface configuration
bt656_interface_config_t config = BT656_DEFAULT_CONFIG;
config.enable_debug_output = true;

// Print statistics periodically
bt656_interface_print_stats(&interface);
bt656_decoder_print_stats(&decoder);
```

### Performance Monitoring

Monitor these key metrics:

- **Interrupts Handled**: Should match expected pixel count
- **Buffer Overflows**: Should be 0 for optimal performance
- **ISR Execution Time**: Should be < 10 microseconds
- **Frame Rate**: Should be ~25 fps for PAL

## API Reference

### Core Functions

```cpp
// BT656 Decoder
bool bt656_decoder_init(bt656_decoder_t* decoder, const bt656_config_t* config);
void bt656_decoder_process_byte(bt656_decoder_t* decoder, uint8_t data);
void bt656_decoder_reset(bt656_decoder_t* decoder);

// BT656 Interface
bool bt656_interface_init(bt656_interface_t* interface, const bt656_interface_config_t* config);
// bt656_interface_start() REMOVED - functionality merged into bt656_interface_init()
void bt656_interface_stop(bt656_interface_t* interface);

// Frame Buffer
bool frame_buffer_init(frame_buffer_t* buffer, uint16_t width, uint16_t height);
bool frame_buffer_is_ready(frame_buffer_t* buffer);
uint16_t* frame_buffer_get_rgb565(frame_buffer_t* buffer);
```

### Color Conversion Functions

```cpp
bt656_rgb_t bt656_ycbcr_to_rgb(bt656_ycbcr_t ycbcr);
uint16_t bt656_rgb_to_rgb565(bt656_rgb_t rgb);
uint8_t bt656_ycbcr_to_grayscale(bt656_ycbcr_t ycbcr);
```

## License

This implementation is provided as-is for educational and development purposes. Please ensure compliance with any applicable licenses for the TVP5150 chip and related components.

## Support

For questions or issues with this implementation, please refer to:

1. TVP5150 datasheet for hardware specifications
2. ESP32 technical reference for GPIO and interrupt details
3. BT656 standard documentation for video format specifications 