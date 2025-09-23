#include <Arduino.h>
#include <RPC.h>
#include <mbed.h>
#include <AdvancedADC.h>

#include "SharedRing.h"
#include "Logger.h"
#include "PinConfig.h"
#include "HardwareTimer.h"

using namespace std::chrono_literals;  // enables 100us, 10ms, etc.

// 5 channels in one scan (minimizes inter-channel skew to ~conversion time)
static AdvancedADC adc(
  PIN_SWITCH_CURRENT,
  PIN_SWITCH_VOLTAGE,
  PIN_OUTPUT_VOLTAGE_A,
  PIN_OUTPUT_VOLTAGE_B,
  PIN_TEMP_1
);

// 10 kHz
constexpr uint32_t SAMPLE_INTERVAL_US = 100;
constexpr uint32_t SAMPLE_HZ = 1000000UL / SAMPLE_INTERVAL_US; 

// --- epoch + index, using 64-bit timer ---
static uint64_t g_epoch_us64 = 0;     // set right after adc.begin()
static uint64_t g_sample_idx = 0;     // increments per sample drained
// helper: split 64-bit µs timestamp into (t_us, rollover_count)
static inline void decompose_us64(uint64_t t64, uint32_t &t_us, uint32_t &roll) {
  t_us = (uint32_t)(t64 & 0xFFFFFFFFULL);
  roll = (uint32_t)(t64 >> 32);
}

void push_samples_until_caught_up() {
  while (adc.available()) {
    SampleBuffer buf = adc.read();

    REMCSample s;
    s.swI  = (uint16_t)buf[0];
    s.swV  = (uint16_t)buf[1];
    s.outA = (uint16_t)buf[2];
    s.outB = (uint16_t)buf[3];
    s.t1   = (uint16_t)buf[4];

    // Aaron Note 9.23.2025
    // The samples are all being captured in the library so I really have no idea what the true start times and end times are.. But it's hardware based
    // so in place of that we will:
    // Exact 64-bit timestamps tied to index on a 100 µs grid
    const uint64_t t64_start = g_epoch_us64 + g_sample_idx * (uint64_t)SAMPLE_INTERVAL_US;
    const uint64_t t64_end   = t64_start + 0;

    // Decompose into (t_us, rollover_count) so they ALWAYS match the sample
    decompose_us64(t64_start, s.t_us, s.rollover_count);
    decompose_us64(t64_end,   s.t_us_end, s.rollover_count_end);

    SharedRing_Add(s);
    buf.release();

    ++g_sample_idx;
  }
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

  // resolution, sample_rate_hz, n_samples_per_buffer, buffer_pool_depth
  if (!adc.begin(AN_RESOLUTION_12, SAMPLE_HZ, /*n_samples=*/1, /*n_buffers=*/128)) {
    Logger::log("[Sampling Core] AdvancedADC begin() failed");
    while (1) { __ASM volatile("nop"); }
  }
  // Anchor the epoch *immediately after* the ADC starts
  g_epoch_us64 = HardwareTimer::getMicros64();
  g_sample_idx = 0;

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

void loop() {
  push_samples_until_caught_up();
}
