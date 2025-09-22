#include <Arduino.h>
#include <RPC.h>
#include <mbed.h>

#include "SharedRing.h"
#include "Logger.h"
#include "PinConfig.h"
#include "HardwareTimer.h"

using namespace std::chrono_literals;  // enables 100us, 10ms, etc.

// Fast AnalogIn handles (GIGA / ArduinoCore-mbed)
static mbed::AnalogIn ain_switchCurrent((PinName)digitalPinToPinName(PIN_SWITCH_CURRENT));
static mbed::AnalogIn ain_switchVoltage((PinName)digitalPinToPinName(PIN_SWITCH_VOLTAGE));
static mbed::AnalogIn ain_outA((PinName)digitalPinToPinName(PIN_OUTPUT_VOLTAGE_A));
static mbed::AnalogIn ain_outB((PinName)digitalPinToPinName(PIN_OUTPUT_VOLTAGE_B));
static mbed::AnalogIn ain_temp1((PinName)digitalPinToPinName(PIN_TEMP_1));

volatile uint16_t g_switchCurrentRaw = 0;
volatile uint16_t g_switchVoltageRaw = 0;
volatile uint16_t g_outputVoltageARaw = 0;
volatile uint16_t g_outputVoltageBRaw = 0;
volatile uint16_t g_temp1Raw = 0;
// Divider for slower temperature sampling
const uint16_t TEMP_DIVIDER_THRESHOLD = 10000;
uint16_t tempSampleCounter = TEMP_DIVIDER_THRESHOLD;

void push_sample(uint32_t atMicros) {
  // Capture timestamp FIRST for maximum accuracy
  uint32_t sample_time = atMicros;
  uint32_t rollover_count = HardwareTimer::getRolloverCount();
  
  // Read all ADC inputs in sequence (minimize timing variation)
  uint16_t swI_raw = ain_switchCurrent.read_u16() >> 4;
  uint16_t swV_raw = ain_switchVoltage.read_u16() >> 4;
  uint16_t outA_raw = ain_outA.read_u16() >> 4;
  uint16_t outB_raw = ain_outB.read_u16() >> 4;
  
  // Throttle temperature read (avoid conditional execution timing variation)
  uint16_t temp_raw = g_temp1Raw; // Use previous value by default
  tempSampleCounter++;
  if (tempSampleCounter >= TEMP_DIVIDER_THRESHOLD) {
    g_temp1Raw = temp_raw = ain_temp1.read_u16() >> 4;
    tempSampleCounter = 0;
  }
  
  // Build sample struct efficiently
  Sample s;
  s.swI = swI_raw;
  s.swV = swV_raw;
  s.outA = outA_raw;
  s.outB = outB_raw;
  s.t1 = temp_raw;
  s.t_us = sample_time;
  s.rollover_count = rollover_count;
  
  SharedRing_Add(s);
}
void setup() {
  
  // LOGGER SETUP
#ifdef CORE_CM7
  // Code that should only run on Core 0 (Cortex-M7)
  Logger::init(0);
  Logger::log("[Sampling Core] Logger debugging over serial connection");
#endif
#ifdef CORE_CM4
  Logger::init(1);
  Logger::log("[Sampling Core] Logger debugging over RPC");
#endif

  // PIN SETUP
  pinMode(PIN_SWITCH_CURRENT, INPUT);
  pinMode(PIN_SWITCH_VOLTAGE, INPUT);
  pinMode(PIN_TEMP_1, INPUT);
  pinMode(PIN_OUTPUT_VOLTAGE_A, INPUT);
  pinMode(PIN_OUTPUT_VOLTAGE_B, INPUT);

  // SHARED RING BUFFER SETUP
  Logger::log("[Sampling Core] SharedRing Address");
  String addrStr = String("[Sampling Core] g_ring @ 0x") + String((uintptr_t)&g_ring, HEX);
  Logger::log(addrStr);
  SharedRing_Init();

  // M4 doesn't need to call begin() - just use the timer
  while (!HardwareTimer::isInitialized()) {
    // Wait for M7 to initialize the timer
    delay(1);
  }
  Logger::log("[Sampling Core] Hardware timer initialized successfully");
}


constexpr uint32_t SAMPLE_INTERVAL_US = 100;

// Pre-calculated timing for better precision
static uint32_t next_sample_time = 0;
static bool first_sample = true;



void loop() {

  // Wait for precise timing at the start of loop (eliminates loop overhead)
  while ((int32_t)(HardwareTimer::getMicros() - next_sample_time) < 0) {
    // Busy wait for precise timing
    __asm volatile("nop");
  }
  
  
  // Initialize timing on first sample
  uint32_t current_time = HardwareTimer::getMicros();
  if (first_sample) {
    next_sample_time = current_time + SAMPLE_INTERVAL_US;
    first_sample = false;
    return; // Skip first sample to establish timing baseline
  }
  
  // Sample immediately when timing is met (no additional time checks)
  push_sample(current_time);
  
  // Schedule next sample time (accumulative to prevent drift)
  next_sample_time += SAMPLE_INTERVAL_US;
  
  // Handle case where we're running behind (skip missed samples)
  if ((int32_t)(current_time - next_sample_time) > (int32_t)SAMPLE_INTERVAL_US) {
    next_sample_time = current_time + SAMPLE_INTERVAL_US;
  }
}
