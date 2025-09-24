#include <Arduino.h>
#include <RPC.h>
#include <mbed.h>

#include "SharedRing.h"
#include "Logger.h"
#include "PinConfig.h"
#include "HardwareTimer.h"   // for getMicros64()

using namespace std::chrono_literals;

// ================== YOUR ADC CHANNEL IDs ==================
// These are STM32 *ADC channel numbers* (SQx values), not Ax/Dx labels.
#ifndef ADC_CHAN_SWITCH_CURRENT
  #define ADC_CHAN_SWITCH_CURRENT    8   // A3 -> e.g. ADC1_IN8 (set to your map)
#endif
#ifndef ADC_CHAN_SWITCH_VOLTAGE
  #define ADC_CHAN_SWITCH_VOLTAGE    3   // A6 -> e.g. ADC1_IN3
#endif
#ifndef ADC_CHAN_OUTPUT_VOLTAGE_A
  #define ADC_CHAN_OUTPUT_VOLTAGE_A 11  // A4 -> e.g. ADC1_IN11
#endif
#ifndef ADC_CHAN_OUTPUT_VOLTAGE_B
  #define ADC_CHAN_OUTPUT_VOLTAGE_B 12  // A5 -> e.g. ADC1_IN12
#endif
#ifndef ADC_CHAN_TEMP_1
  #define ADC_CHAN_TEMP_1           10  // A2 -> e.g. ADC1_IN10
#endif
// ==========================================================

// 10 kHz
constexpr uint32_t SAMPLE_INTERVAL_US = 100;

// ---------- ISR-safe push ----------
static inline void SharedRing_AddFromISR_Safe(const REMCSample &s) {
  SharedRing_Add(s);
}

// ---------- Use ADC1 (adjust if you split across ADC3) ----------
#define ADCx        ADC1
#define ADCx_COMMON ADC12_COMMON

// mbed ticker -> IRQ every 100 µs
static mbed::Ticker g_samplerTicker;

// ---------- helpers ----------
static inline void decompose_us64(uint64_t t64, uint32_t &t_us, uint32_t &roll) {
  t_us  = (uint32_t)(t64 & 0xFFFFFFFFULL);
  roll  = (uint32_t)(t64 >> 32);
}

static void adc_config_once() {
  __HAL_RCC_ADC12_CLK_ENABLE();

  // --- H7: regulator + power-up lives in ADCx->CR, not CCR ---
  // Exit deep-power-down if present
#ifdef ADC_CR_DEEPPWD
  CLEAR_BIT(ADCx->CR, ADC_CR_DEEPPWD);
#endif
  // Enable the ADC internal voltage regulator (ADVREGEN)
#ifdef ADC_CR_ADVREGEN_0
  // Some headers expose _0/_1 fields
  MODIFY_REG(ADCx->CR, ADC_CR_ADVREGEN, ADC_CR_ADVREGEN_0);
#else
  // Others expose a single mask
  SET_BIT(ADCx->CR, ADC_CR_ADVREGEN);
#endif
  delayMicroseconds(20); // regulator startup

  // make sure disabled before config
  if (READ_BIT(ADCx->CR, ADC_CR_ADEN)) {
    SET_BIT(ADCx->CR, ADC_CR_ADDIS);
    while (READ_BIT(ADCx->CR, ADC_CR_ADEN)) {}
  }

  // single conversion, SW trigger
  ADCx->CFGR = 0;

  // sampling times — fast but safe default
  const uint32_t SMP_47CYC = 0b101; // ~47.5 cycles
  ADCx->SMPR1 =
      (SMP_47CYC << ADC_SMPR1_SMP0_Pos) |
      (SMP_47CYC << ADC_SMPR1_SMP1_Pos) |
      (SMP_47CYC << ADC_SMPR1_SMP2_Pos) |
      (SMP_47CYC << ADC_SMPR1_SMP3_Pos) |
      (SMP_47CYC << ADC_SMPR1_SMP4_Pos) |
      (SMP_47CYC << ADC_SMPR1_SMP5_Pos) |
      (SMP_47CYC << ADC_SMPR1_SMP6_Pos) |
      (SMP_47CYC << ADC_SMPR1_SMP7_Pos) |
      (SMP_47CYC << ADC_SMPR1_SMP8_Pos) |
      (SMP_47CYC << ADC_SMPR1_SMP9_Pos);

  // if you use channels >9, set SMPR2 bits too
  ADCx->SMPR2 =
      (SMP_47CYC << ADC_SMPR2_SMP10_Pos) |
      (SMP_47CYC << ADC_SMPR2_SMP11_Pos) |
      (SMP_47CYC << ADC_SMPR2_SMP12_Pos) |
      (SMP_47CYC << ADC_SMPR2_SMP13_Pos) |
      (SMP_47CYC << ADC_SMPR2_SMP14_Pos) |
      (SMP_47CYC << ADC_SMPR2_SMP15_Pos) |
      (SMP_47CYC << ADC_SMPR2_SMP16_Pos) |
      (SMP_47CYC << ADC_SMPR2_SMP17_Pos) |
      (SMP_47CYC << ADC_SMPR2_SMP18_Pos) |
      (SMP_47CYC << ADC_SMPR2_SMP19_Pos);

  // sequence length = 5 (L=4), ranks 1..5
  ADCx->SQR1 =
      (4u << ADC_SQR1_L_Pos) |
      (ADC_CHAN_SWITCH_CURRENT   << ADC_SQR1_SQ1_Pos) |  // rank1
      (ADC_CHAN_SWITCH_VOLTAGE   << ADC_SQR1_SQ2_Pos) |  // rank2
      (ADC_CHAN_OUTPUT_VOLTAGE_A << ADC_SQR1_SQ3_Pos) |  // rank3
      (ADC_CHAN_OUTPUT_VOLTAGE_B << ADC_SQR1_SQ4_Pos);   // rank4
  ADCx->SQR2 =
      (ADC_CHAN_TEMP_1 << ADC_SQR2_SQ5_Pos);             // rank5

  // calibrate & enable
  SET_BIT(ADCx->CR, ADC_CR_ADCAL);
  while (READ_BIT(ADCx->CR, ADC_CR_ADCAL)) {}
  SET_BIT(ADCx->ISR, ADC_ISR_ADRDY);
  SET_BIT(ADCx->CR, ADC_CR_ADEN);
  while (!READ_BIT(ADCx->ISR, ADC_ISR_ADRDY)) {}
}

static inline void adc_start_sequence() {
  // clear flags and start one scan
  SET_BIT(ADCx->ISR, ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR);
  SET_BIT(ADCx->CR,  ADC_CR_ADSTART);
}

static inline void adc_read_frame5(uint16_t out5[5]) {
  // poll EOC for each rank; EOS at the end
  for (int i = 0; i < 5; ++i) {
    while (!READ_BIT(ADCx->ISR, ADC_ISR_EOC)) {}
    out5[i] = (uint16_t)ADCx->DR; // DR read clears EOC
  }
  while (!READ_BIT(ADCx->ISR, ADC_ISR_EOS)) {}
  SET_BIT(ADCx->ISR, ADC_ISR_EOS);
}

// ---------- ISR: every 100 µs ----------
static void on_sample_tick() {
  const uint64_t t_start = HardwareTimer::getMicros64();

  uint16_t v[5];
  adc_start_sequence();
  adc_read_frame5(v);

  const uint64_t t_end = HardwareTimer::getMicros64();

  REMCSample s{};
  s.swI  = v[0];
  s.swV  = v[1];
  s.outA = v[2];
  s.outB = v[3];
  s.t1   = v[4];

  decompose_us64(t_start, s.t_us,     s.rollover_count);
  decompose_us64(t_end,   s.t_us_end, s.rollover_count_end);

  SharedRing_AddFromISR_Safe(s);
}

void setup() {
#ifdef CORE_CM7
  Logger::init(0);
  Logger::log("[Sampling Core] Logger debugging over serial");
#endif
#ifdef CORE_CM4
  Logger::init(1);
  Logger::log("[Sampling Core] Logger debugging over RPC");
#endif

  Logger::log("[Sampling Core] SharedRing Address");
  String addrStr = String("[Sampling Core] g_ring @ 0x") + String((uintptr_t)&g_ring, HEX);
  Logger::log(addrStr);
  SharedRing_Init();

  adc_config_once();

  g_samplerTicker.attach(mbed::callback(on_sample_tick), 100us);
  Logger::log("[Sampling Core] mbed::Ticker sampling @ 10 kHz");
}

void loop() {
  // ISR produces; main can drain or do nothing.
}
