# Core 0 - Communication & Control Hub

This is the **Core 0 (Cortex-M7)** firmware for the high-voltage control system running on the Arduino GIGA R1. Core 0 handles serial communication, telemetry transmission, system coordination, and serves as the primary interface to the Python web server.

## Core Responsibilities

### 1. **Inter-Core Communication**
- **RPC (Remote Procedure Call)**: Manages communication with Core 1 via Arduino's RPC system
- **Message Relay**: Forwards debug messages from Core 1 to Serial output
- **Coordination**: Orchestrates overall system operation

### 2. **Telemetry Processing**
- **Sample Collection**: Consumes high-speed samples from shared ring buffer
- **Data Packaging**: Formats samples into Neutrino protocol packets
- **Network Transmission**: Sends telemetry via UDP multicast to web server
- **Batch Processing**: Handles multiple samples per packet for efficiency

### 3. **Command Processing**
- **UDP Command Reception**: Listens for control commands from web server
- **Command Validation**: Ensures command integrity and safety
- **System Control**: Executes arm/disarm, fire, mode changes, etc.
- **State Management**: Maintains finite state machine for automatic operation

## Key Components

### SharedRing Buffer Consumer
```cpp
size_t count = SharedRing_Consume(sampleBuffer, MAX_FETCH);
```
- Fetches up to 1024 samples per loop iteration from Core 1
- Thread-safe lock-free ring buffer implementation
- Handles overrun conditions gracefully

### Logger System
```cpp
Logger::init(0);  // Core 0 uses Serial output
Logger::log("Debug message");
```
- **Core 0**: Direct Serial output at 115200 baud
- **Debug Output**: System status, sample rates, network activity

### Network Protocol
- **Neutrino Header**: 64-byte structured header with metadata
- **Sample Data**: Variable payload with 26 or 30-byte samples
- **Multicast**: `239.9.9.33:13013` for telemetry output
- **Command Input**: `239.9.9.32:13012` for control commands

## File Structure

```
REMC_GIGAR1_Core0/
├── REMC_GIGAR1_Core0.ino    # Main firmware entry point
├── Logger.h/.cpp            # Debug logging system
├── SharedRing.h/.cpp        # Inter-core ring buffer
└── README.md                # This file
```

## Key Features

### High-Performance Data Path
- **Lock-Free Ring Buffer**: Zero-mutex inter-core communication
- **Batch Processing**: Processes up to 1024 samples per loop
- **Sample Rate Analysis**: Real-time monitoring of sampling intervals
- **Memory Management**: Efficient buffer management in shared SRAM4

### Debug & Diagnostics
- **Sample Rate Monitoring**: Rolling window analysis of timing intervals
- **Buffer Status**: Head/tail positions, overrun counting
- **Network Activity**: Packet transmission logging
- **RPC Message Relay**: Core 1 debug output forwarding

### Network Communication
- **UDP Multicast**: Efficient one-to-many telemetry distribution
- **Packet Batching**: Multiple samples per network packet
- **Protocol Versioning**: Support for legacy and enhanced sample formats
- **Error Handling**: Robust network error recovery

## Configuration

### Timing Parameters
```cpp
constexpr int MAX_FETCH = 1024;      // Max samples per loop
constexpr int WINDOW = 20;           // Sample rate analysis window
```

### Memory Layout
- **Shared Ring Buffer**: Located in STM32H747 SRAM4 (dual-core accessible)
- **Buffer Address**: Top of SRAM4 to avoid OpenAMP conflicts
- **Sample Buffer**: 1024 samples × 16 bytes = 16KB local buffer

## Development Notes

### Debugging
- **Serial Monitor**: 115200 baud for Core 0 debug output
- **Sample Diagnostics**: Enable `DEBUG_printSampleDiagnostics()` for timing analysis
- **RPC Messages**: Core 1 debug messages appear in Core 0 serial output

### Performance Optimization
- **Minimal Loop Overhead**: Optimized for maximum sample throughput
- **Memory Barriers**: Proper synchronization for dual-core safety
- **Buffer Sizing**: Tuned for 10kHz sample rate with network jitter tolerance

### Safety Considerations
- **Overrun Handling**: Graceful degradation when sample rate exceeds network capacity
- **Command Validation**: Input sanitization for all network commands
- **State Machine**: Prevents unsafe state transitions

## Integration

This core works in conjunction with:
- **Core 1**: Provides high-speed analog samples via SharedRing
- **Python Web Server**: Receives telemetry and sends control commands
- **Web Dashboard**: Real-time visualization and control interface

For system-wide setup and operation, see the main project README.