# High-Voltage Control System

A high-performance telemetry and control system featuring dual-core Arduino processing and real-time web monitoring. The system monitors electrical parameters (voltage, current, temperature) and provides armed/disarmed control with safety interlocks.

## System Architecture

The system consists of three main components:

### 1. **Arduino Dual-Core Controller** (STM32H747 - Arduino GIGA R1)
- **Core 0 (CM7)**: Serial communication, telemetry transmission, and system coordination
- **Core 1 (CM4)**: High-speed analog sampling (10kHz) and data acquisition
- **Shared Memory**: Inter-core communication via lock-free ring buffer in SRAM4

### 2. **Python Web Server** 
- **Flask Application**: Real-time web dashboard for system monitoring and control
- **UDP Multicast**: Bidirectional communication with Arduino (telemetry + commands)
- **Data Logging**: High-frequency sample logging with CSV export capability

### 3. **Network Communication**
- **Multicast Groups**: 
  - `239.9.9.33:13013` - Telemetry data (Arduino → PC)
  - `239.9.9.32:13012` - Command data (PC → Arduino)
- **Protocol**: Custom Neutrino packet format with batched sampling support

## Key Features

- **High-Speed Sampling**: 10kHz analog data acquisition on dedicated core
- **Real-Time Monitoring**: Live web dashboard with 500ms update rate
- **Dual Operation Modes**: Automatic FSM control and Manual override
- **Safety Systems**: Multiple interlocks, emergency stops, and state monitoring
- **Data Logging**: Continuous telemetry logging with microsecond timestamps
- **Fault Tolerance**: Overrun handling, packet validation, and error reporting

## Quick Start

### Prerequisites
```bash
# Python environment
python -m venv venv
.\venv\Scripts\activate
pip install -r reuirements.txt
```

### Network Configuration
Configure your PC's Ethernet adapter:
- IP address: `192.168.1.10`
- Subnet mask: `255.255.255.0`
- Connect Arduino via Ethernet

### Running the System
1. **Upload Arduino firmware** to both cores using Arduino IDE
2. **Start the web server**:
   ```bash
   python CFS_REMC_WEB_PAGE-250827_batched_with_per_sample_timestamp_us.py
   ```
3. **Access dashboard** at `http://192.168.1.10:5002`

## Project Structure

```
├── REMC_GIGAR1_Core0/          # Serial communication & telemetry core
├── REMC_GIGAR1_Core1/          # High-speed sampling core  
├── CFS_REMC_WEB_PAGE-*.py      # Python web server & dashboard
├── reuirements.txt             # Python dependencies
└── *.csv                       # Telemetry log files
```

## Safety Notice

⚠️ **HIGH VOLTAGE SYSTEM** - This system controls high-voltage electrical equipment. Only qualified personnel should operate this system. Always follow proper safety protocols.

