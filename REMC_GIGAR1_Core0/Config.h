#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Ethernet.h>

namespace Config {

// ----- Network configuration -----
// W5x00 MAC/IP — keep these same for your LAN (PC IP)
static const byte MAC_ADDRESS[]     = { 0xD2,0x4F,0x1A,0xC8,0x7E,0x3B };
static const IPAddress LOCAL_IP     (192, 168, 1, 50);
static const IPAddress GATEWAY_IP   (192, 168, 1, 1);
static const IPAddress SUBNET_MASK  (255, 255, 255, 0);

static const IPAddress TELEMETRY_IP (239, 9, 9, 33);
static const uint16_t  TELEMETRY_PORT = 13013;

static const IPAddress COMMAND_MCAST_IP (239, 9, 9, 32);
static const uint16_t  COMMAND_PORT = 13012;

static const IPAddress NTP_IP (239, 9, 9, 34);
static const uint16_t  NTP_PORT = 13014;

// ----- NTP Client configuration -----
static const char* NTP_SERVER = "192.168.1.10";  // NTP server IP/hostname
static const uint16_t NTP_CLIENT_PORT = 123;     // NTP server port

// ----- Timing configuration -----
static const unsigned int  ANALOG_SAMPLE_FREQUENCY_HZ  = 10000; // ADC sample & PWM update rate
static const unsigned int  ANALOG_OUTPUT_FREQUENCY_HZ  = 10000; // PWM frequency
}

#endif // CONFIG_H
