// TimerManager.cpp
#include "TimerManager.h"
#include "PinConfig.h"
#include "Config.h"
#include <Arduino.h>
#include "UdpManager.h"  // So we can notify telemtry on each sample
#include <mbed.h>
#include <chrono>
using namespace std::chrono_literals;  // enables 100us, 10ms, etc.

// Fast AnalogIn handles (GIGA / ArduinoCore-mbed)
static mbed::AnalogIn ain_switchCurrent((PinName)digitalPinToPinName(PIN_SWITCH_CURRENT));
static mbed::AnalogIn ain_switchVoltage((PinName)digitalPinToPinName(PIN_SWITCH_VOLTAGE));
static mbed::AnalogIn ain_temp1((PinName)digitalPinToPinName(PIN_TEMP_1));
static mbed::AnalogIn ain_outA((PinName)digitalPinToPinName(PIN_OUTPUT_VOLTAGE_A));
static mbed::AnalogIn ain_outB((PinName)digitalPinToPinName(PIN_OUTPUT_VOLTAGE_B));

namespace {
// Raw ADC storage
volatile uint16_t g_switchCurrentRaw = 0;
volatile uint16_t g_switchVoltageRaw = 0;
volatile uint16_t g_temp1Raw = 0;
volatile uint16_t g_outputVoltageARaw = 0;
volatile uint16_t g_outputVoltageBRaw = 0;

// Divider for slower temperature sampling
uint16_t tempSampleDivider = 0;
const uint16_t TEMP_DIVIDER_THRESHOLD = 10000;

// ADC calibration
// ADC resolution (12-bit: 0 → 4095)
const float ADC_MAX_VALUE = 4095.0f;

// Switch current calibration:
//   physical current [A] = raw * SCALE + OFFSET
//   maps 0 → 4095 raw to –500 A → +500 A
const float SCALE_SWITCH_CURRENT_A = 1000.0f / ADC_MAX_VALUE;  // 1 count ≈ 0.244 A
const float OFFSET_SWITCH_CURRENT_A = -471.551f;               // raw = 0 → –500 A

// Switch voltage calibration:
//   physical voltage [kV] = raw * SCALE + OFFSET
//   maps 0 → 4095 raw to –10 kV → +10 kV
//   scale aquired by measuring voltage at set points and scaling the average results (v2-v1)/(vMeas2-vmeas1)
//   offset adjusted for scaling and amp reference
const float SCALE_VOLTAGE_KV = 0.004449458233f;  // kV = 1 count
const float OFFSET_VOLTAGE_KV = -8.939881545f;   // raw = 0 → –8.94 kV

// Output A voltage calibration (kV):
//   same mapping technique as switch voltage, but independent in case of separate cal
const float SCALE_OUTPUT_A_KV = 0.004447667531f;  // kV = 1 count
const float OFFSET_OUTPUT_A_KV = -8.941615805f;   // raw = 0 → –8.94 kV

// Output B voltage calibration (kV):
//   same mapping technique as switch voltage, but independent in case of separate cal
const float SCALE_OUTPUT_B_KV = 0.004445948727f;  // kV = 1 count
const float OFFSET_OUTPUT_B_KV = -8.936364074f;   // raw = 0 → –8.94 kV


// Temperature calibration:
//   physical temp [°C] = raw * SCALE + OFFSET
//   maps 0 → 4095 raw to 0 °C → 100 °C
const float SCALE_TEMP_DEGC = 100.0f / ADC_MAX_VALUE;  // 1 count ≈ 0.0244 °C
const float OFFSET_TEMP_DEGC = -5.5f;                  // raw = 0 → 0 °C


// PWM output scaling (match UDP calibration)
const float OUT_SCALE_VOLTAGE = ADC_MAX_VALUE / 20.0f;    // 20 kV span → 0–4095
const float OUT_OFFSET_VOLTAGE = 10.0f;                   // +10 kV offset
const float OUT_SCALE_CURRENT = ADC_MAX_VALUE / 1000.0f;  // 1000 A span → 0–4095
const float OUT_OFFSET_CURRENT = 500.0f;                  // +500 A offset

// --------------------------- 10 kHz ticker ---------------------------
mbed::Ticker sampleTicker;
volatile uint32_t isrCount = 0;  // stats
volatile uint32_t lastIrqUs = 0;

void sampleISR() {
  lastIrqUs = micros();
  isrCount++;
}

void startSampleTimer() {
  sampleTicker.attach(mbed::callback(sampleISR), 100us);
  Serial.print(F("[TimerManager] Target sample rate = "));
  Serial.println((uint32_t)Config::ANALOG_SAMPLE_FREQUENCY_HZ);
}
}

namespace TimerManager {

void init() {
  // ADC & PWM resolution
  analogReadResolution(12);

  // Configure analog inputs
  pinMode(PIN_SWITCH_CURRENT, INPUT);
  pinMode(PIN_SWITCH_VOLTAGE, INPUT);
  pinMode(PIN_TEMP_1, INPUT);
  pinMode(PIN_OUTPUT_VOLTAGE_A, INPUT);
  pinMode(PIN_OUTPUT_VOLTAGE_B, INPUT);


  // Start the 10 kHz sampling interrupt
  startSampleTimer();
}

void update() {
  static uint32_t seenCount = 0;  // last ISR tick we processed

  // snapshot to avoid race between count & timestamp ---
  uint32_t count_snapshot;
  uint32_t irq_us_snapshot;
  noInterrupts();
  count_snapshot = isrCount;
  irq_us_snapshot = lastIrqUs;
  interrupts();

  if (count_snapshot == seenCount) {
    return;  // nothing new to do
  }
  // exactly one processing per ISR tick
  seenCount = count_snapshot;

  // Read inputs (mbed returns 0..65535, shift to 12-bit scale)
  g_switchCurrentRaw = ain_switchCurrent.read_u16() >> 4;
  g_switchVoltageRaw = ain_switchVoltage.read_u16() >> 4;
  g_outputVoltageARaw = ain_outA.read_u16() >> 4;
  g_outputVoltageBRaw = ain_outB.read_u16() >> 4;

  // Throttle temperature read
  tempSampleDivider++;
  if (tempSampleDivider >= TEMP_DIVIDER_THRESHOLD) {
    g_temp1Raw = ain_temp1.read_u16() >> 4;
    tempSampleDivider = 0;
  }

  // Use the latched ISR timestamp for this sample
  UdpManager::onSampleTick(irq_us_snapshot);
}

static inline uint16_t getGenericRaw(volatile uint16_t &val) {
  noInterrupts();
  uint16_t temp = val;
  interrupts();
  return temp;
}

uint16_t getSwitchCurrentRaw() {
  return getGenericRaw(g_switchCurrentRaw);
}
uint16_t getSwitchVoltageRaw() {
  return getGenericRaw(g_switchVoltageRaw);
}
uint16_t getTemp1Raw() {
  return getGenericRaw(g_temp1Raw);
}
uint16_t getOutputVoltageARaw() {
  return getGenericRaw(g_outputVoltageARaw);
}
uint16_t getOutputVoltageBRaw() {
  return getGenericRaw(g_outputVoltageBRaw);
}

float getSwitchCurrentA() {
  return getSwitchCurrentRaw() * SCALE_SWITCH_CURRENT_A + OFFSET_SWITCH_CURRENT_A;
}
float getSwitchVoltageKV() {
  return getSwitchVoltageRaw() * SCALE_VOLTAGE_KV + OFFSET_VOLTAGE_KV;
}
float getTemp1DegC() {
  return getTemp1Raw() * SCALE_TEMP_DEGC + OFFSET_TEMP_DEGC;
}
float getOutputVoltageAKV() {
  return getOutputVoltageARaw() * SCALE_OUTPUT_A_KV + OFFSET_OUTPUT_A_KV;
}
float getOutputVoltageBKV() {
  return getOutputVoltageBRaw() * SCALE_OUTPUT_B_KV + OFFSET_OUTPUT_B_KV;
}

}  // namespace TimerManager