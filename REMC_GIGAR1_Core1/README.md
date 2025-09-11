# Core 1 - High-Speed Sampling Engine

This is the **Core 1 (Cortex-M4)** firmware for the high-voltage control system running on the Arduino GIGA R1. Core 1 is dedicated to high-speed analog data acquisition, running at 10kHz sampling rate with minimal latency and jitter.

## Core Responsibilities

### 1. **High-Speed Analog Sampling**
- **10kHz Sampling Rate**: Precise 100μs intervals for critical measurements
- **Multi-Channel ADC**: Simultaneous sampling of 5 analog channels
- **Low Latency**: Optimized timing with microsecond precision
- **Temperature Throttling**: Reduced sampling rate for temperature sensor

### 2. **Data Acquisition**
- **Voltage & Current Monitoring**: Primary electrical measurements
- **Dual Output Monitoring**: Output voltages A & B
- **Temperature Sensing**: Thermal monitoring with rate limiting
- **Raw ADC Values**: 12-bit resolution (0-4095 range)

### 3. **Inter-Core Communication**
- **SharedRing Producer**: Feeds samples to Core 0 via lock-free ring buffer
- **Memory Synchronization**: Proper barriers for dual-core safety
- **Overrun Management**: Handles buffer full conditions gracefully

## Hardware Interface

### Analog Inputs (12-bit ADC)
```cpp
// Pin assignments from PinConfig.h
#define PIN_SWITCH_CURRENT    A3    // Current measurement
#define PIN_SWITCH_VOLTAGE    A6    // Voltage measurement  
#define PIN_OUTPUT_VOLTAGE_A  A4    // Output voltage A
#define PIN_OUTPUT_VOLTAGE_B  A5    // Output voltage B
#define PIN_TEMP_1           A2    // Temperature sensor 1
```

### Sample Structure
```cpp
struct Sample {
    uint32_t t_us;      // Microsecond timestamp
    uint16_t swI;       // Current measurement (raw ADC)
    uint16_t swV;       // Voltage measurement (raw ADC)  
    uint16_t outA;      // Output voltage A (raw ADC)
    uint16_t outB;      // Output voltage B (raw ADC)
    uint16_t t1;        // Temperature 1 (raw ADC)
};
```

## Key Components

### Fast Analog Reading
```cpp
static mbed::AnalogIn ain_switchCurrent((PinName)digitalPinToPinName(PIN_SWITCH_CURRENT));
uint16_t raw = ain_switchCurrent.read_u16() >> 4;  // 16-bit → 12-bit
```
- **mbed AnalogIn**: Direct hardware access for maximum speed
- **Bit Shifting**: Converts 16-bit mbed values to 12-bit Arduino scale
- **No Arduino analogRead()**: Bypasses slow Arduino ADC functions

### Precision Timing Control
```cpp
constexpr uint32_t SAMPLE_RATE_US = 100;    // 10kHz = 100μs period
constexpr uint32_t OVERHEAD_US = 2;         // Compensation for processing
```
- **Loop-Based Timing**: Precise control using `delayMicroseconds()`
- **Overhead Compensation**: Accounts for processing time
- **Microsecond Timestamps**: Each sample tagged with `micros()`

### Temperature Rate Limiting
```cpp
const uint16_t TEMP_DIVIDER_THRESHOLD = 10000;  // Sample temp every 10k cycles
```
- **Reduced Rate**: Temperature sampled at ~1Hz instead of 10kHz
- **Thermal Time Constant**: Matches sensor response characteristics
- **Processing Optimization**: Reduces unnecessary ADC conversions

## File Structure

```
REMC_GIGAR1_Core1/
├── REMC_GIGAR1_Core1.ino    # Main sampling firmware
├── PinConfig.h              # Hardware pin definitions
├── Logger.h/.cpp            # Debug logging via RPC
├── SharedRing.h/.cpp        # Inter-core ring buffer
└── README.md                # This file
```

## Performance Characteristics

### Sampling Performance
- **Target Rate**: 10,000 samples/second (100μs period)
- **Actual Jitter**: <2μs typical timing variation
- **Processing Time**: ~15μs per sample including ring buffer write
- **Remaining Budget**: ~83μs available for other processing

### Memory Usage
- **Ring Buffer**: 1024 samples × 16 bytes = 16KB in shared SRAM4
- **Local Variables**: Minimal stack usage for ISR safety
- **ADC Handles**: Static allocation for performance

### Throughput Analysis
- **Data Rate**: 10kHz × 16 bytes = 160 KB/s sample data
- **Network Capacity**: Batched transmission reduces network overhead
- **Buffer Depth**: 1024 samples provides ~100ms buffering at 10kHz

## Development Features

### Debug Logging
```cpp
Logger::init(1);  // Core 1 uses RPC to Core 0
Logger::log("[Sampling Core] Debug message");
```
- **RPC Communication**: Debug messages sent to Core 0 for serial output
- **Conditional Compilation**: Debug code can be disabled for production
- **Performance Impact**: Minimal overhead when logging disabled

### Timing Diagnostics
- **Loop Timing**: Measures actual vs. target sample intervals
- **Overhead Analysis**: Tracks processing time per sample
- **Buffer Status**: Monitors ring buffer utilization

## Configuration Options

### Sampling Rate Adjustment
```cpp
constexpr uint32_t SAMPLE_RATE_US = 100;  // Change for different rates
```
- **10kHz**: Default high-speed sampling
- **Lower Rates**: Increase value for reduced CPU load
- **Higher Rates**: Limited by ADC conversion time (~13μs minimum)

### Temperature Sampling Rate
```cpp
const uint16_t TEMP_DIVIDER_THRESHOLD = 10000;  // Adjust temperature rate
```
- **Current**: ~1Hz (every 10,000 samples)
- **Faster**: Reduce threshold for more frequent temperature updates
- **Slower**: Increase threshold to reduce processing load

## Safety & Reliability

### Overrun Handling
- **Ring Buffer Full**: Oldest samples automatically discarded
- **Overrun Counting**: Tracks lost samples for diagnostics
- **Graceful Degradation**: System continues operation during overruns

### Memory Synchronization
- **Data Memory Barriers**: Ensures proper ordering for dual-core access
- **Atomic Operations**: Ring buffer updates are atomic
- **Cache Coherency**: Proper handling of shared memory regions

### Error Recovery
- **ADC Failure**: System continues with last known values
- **Timing Slip**: Automatic recovery to target sample rate
- **Buffer Corruption**: Detected and reported via overrun counter

## Integration Notes

### Shared Memory Layout
- **SRAM4 Location**: 0x38000000 - 0x3800FFFF (64KB total)
- **Ring Buffer**: Positioned at top of SRAM4 to avoid OpenAMP conflicts
- **Alignment**: 32-byte aligned for optimal cache performance

### Inter-Core Coordination
- **Producer Role**: Core 1 writes samples to ring buffer
- **Consumer Role**: Core 0 reads samples for network transmission
- **No Synchronization**: Lock-free design eliminates blocking

### Hardware Dependencies
- **STM32H747**: Dual-core ARM Cortex-M7/M4 architecture required
- **Arduino GIGA R1**: Specific pin assignments and mbed integration
- **12-bit ADC**: Hardware ADC resolution assumption

For complete system operation, this core must work in conjunction with Core 0 and the Python web server. See the main project README for system integration details.
