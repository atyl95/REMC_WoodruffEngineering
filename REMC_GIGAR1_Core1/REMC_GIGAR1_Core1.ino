#include <Arduino.h>
#include <RPC.h>
#include <mbed.h>

#include "SharedRing.h"
#include "Logger.h"
#include "PinConfig.h"

using namespace std::chrono_literals;  // enables 100us, 10ms, etc.

// Fast AnalogIn handles (GIGA / ArduinoCore-mbed)
static mbed::AnalogIn ain_switchCurrent((PinName)digitalPinToPinName(PIN_SWITCH_CURRENT));
static mbed::AnalogIn ain_switchVoltage((PinName)digitalPinToPinName(PIN_SWITCH_VOLTAGE));
static mbed::AnalogIn ain_outA((PinName)digitalPinToPinName(PIN_OUTPUT_VOLTAGE_A));
static mbed::AnalogIn ain_outB((PinName)digitalPinToPinName(PIN_OUTPUT_VOLTAGE_B));
static mbed::AnalogIn ain_temp1((PinName)digitalPinToPinName(PIN_TEMP_1));

mbed::Ticker sampleTicker;

volatile uint16_t g_switchCurrentRaw = 0;
volatile uint16_t g_switchVoltageRaw = 0;
volatile uint16_t g_outputVoltageARaw = 0;
volatile uint16_t g_outputVoltageBRaw = 0;
volatile uint16_t g_temp1Raw = 0;
// Divider for slower temperature sampling
const uint16_t TEMP_DIVIDER_THRESHOLD = 10000;
uint16_t tempSampleCounter = TEMP_DIVIDER_THRESHOLD;

void push_sample() {

  // Read inputs (mbed returns 0..65535, shift to 12-bit scale)
  g_switchCurrentRaw = ain_switchCurrent.read_u16() >> 4;
  g_switchVoltageRaw = ain_switchVoltage.read_u16() >> 4;
  g_outputVoltageARaw = ain_outA.read_u16() >> 4;
  g_outputVoltageBRaw = ain_outB.read_u16() >> 4;

  // Throttle temperature read
  tempSampleCounter++;
  if (tempSampleCounter >= TEMP_DIVIDER_THRESHOLD) {
    g_temp1Raw = ain_temp1.read_u16() >> 4;
    tempSampleCounter = 0;
  }

  Sample s;
  s.swI   = g_switchCurrentRaw;
  s.swV   = g_switchVoltageRaw;
  s.outA  = g_outputVoltageARaw;
  s.outB  = g_outputVoltageBRaw;
  s.t1    = g_temp1Raw;
  s.t_us = micros();
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

  // START ISR LOOP
  //sampleTicker.attach(mbed::callback(push_sample), 100us);
}


constexpr uint32_t SAMPLE_RATE_US = 100;
constexpr uint32_t OVERHEAD_US = 2;
void loop() {
  uint32_t loop_start_us = micros();
  push_sample();
  uint32_t loop_end_us = micros();

  int32_t remain = (int32_t)SAMPLE_RATE_US
                - (int32_t)(loop_end_us - loop_start_us)
                - (int32_t)OVERHEAD_US;
  if (remain > 0) {
    delayMicroseconds((uint32_t)remain);
  }
}
