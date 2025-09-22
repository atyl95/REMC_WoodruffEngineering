// Aaron Note: 9.19.2025
// This HardwareTimer is currently working by using TIM2 as the low word and TIM5 as the high word.
// It currently works but there are some concerns about the usage of TIM2 and TIM5 in the Arduino GIGA R1.
// If you are seeing anything interrupting PWM or serial, this *might* be the cause.
//
// There is also a nice upgrade that could be made to this class at some point:
// TIM2 *should* trigger an increment of TIM5 on overflow at the hardware level. It does not right now.
// Right now, checkRollover() or one of the get functions must be called at least once every 71 minutes.
// This is because of the 1MHz clock of TIM2 incrementing a uint32_t and the rollover check being called manually.
// It should work just fine in any reasonable scenario but it's ugly.

#include "HardwareTimer.h"

#ifdef CORE_CM7
bool HardwareTimer::begin() {
  
  // Compute prescaler for exactly 1 MHz from actual timer clock
  uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
  RCC_ClkInitTypeDef clk;
  uint32_t flashLatency;
  HAL_RCC_GetClockConfig(&clk, &flashLatency);
  bool apbDivIs1 = (clk.APB1CLKDivider == RCC_HCLK_DIV1);
  uint32_t timclk = apbDivIs1 ? pclk1 : (pclk1 * 2U);
  
  uint32_t presc = (timclk / 1000000U) - 1U;
  
  // ---------- TIM2 @ 1 MHz ----------
  __HAL_RCC_TIM2_CLK_ENABLE();
  TIM_HandleTypeDef htim2;
  htim2.Instance = TIM2;
  htim2.Init.Prescaler         = presc;
  htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim2.Init.Period            = 0xFFFFFFFFU; // 32-bit
  htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK) return false;
  
  // ---------- TIM5 counts TIM2 overflows ----------
  __HAL_RCC_TIM5_CLK_ENABLE();
  TIM_HandleTypeDef htim5;
  htim5.Instance               = TIM5;
  htim5.Init.Prescaler         = 0;                 
  htim5.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim5.Init.Period            = 0xFFFFFFFFU;       // 32-bit high word
  htim5.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim5) != HAL_OK) return false;
  
  
  // // Configure TIM2 as master
  // TIM_MasterConfigTypeDef m = {0};
  // m.MasterOutputTrigger = TIM_TRGO_UPDATE;          // TRGO = update event
  // m.MasterSlaveMode     = TIM_MASTERSLAVEMODE_ENABLE;
  // if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &m) != HAL_OK) return false;
  // AL note:
  // I have no idea why TIM5 has to be set to slave mode (even with no input trigger) in order for it to work properly as a manual ticker.
  // Without this, TIM5 will just keep counting up.
  TIM_SlaveConfigTypeDef s = {0};
  s.SlaveMode    = TIM_SLAVEMODE_EXTERNAL1;  // count trigger edges as clock
  // s.InputTrigger = TIM_TS_ITR0;              // ITR1 maps to TIM2_TRGO for TIM5 on H7
  if (HAL_TIM_SlaveConfigSynchro(&htim5, &s) != HAL_OK) return false;

  __HAL_TIM_SET_COUNTER(&htim2, 0);
  if (HAL_TIM_Base_Start(&htim2) != HAL_OK) return false;
  __HAL_TIM_SET_COUNTER(&htim5, 0);
  if (HAL_TIM_Base_Start(&htim5) != HAL_OK) return false;

  __DMB(); __DSB(); // make sure other core sees started timers
  return true;
}

#endif // CORE_CM7

uint32_t HardwareTimer::lastTIM2Value = 0;
uint32_t HardwareTimer::lastTIM5Value = 0;
void HardwareTimer::checkRollover() {
  if (!isInitialized()) return;
  
  uint32_t currentTIM2 = TIM2->CNT;
  uint32_t currentTIM5 = TIM5->CNT;
  // Check if TIM2 rolled over (current value is less than last value)
  // Using a threshold to avoid false triggers from reading during an update
  if (currentTIM2 < lastTIM2Value && currentTIM5 == lastTIM5Value) {
    TIM5->CNT = TIM5->CNT + 1;
  }
  lastTIM2Value = currentTIM2;
  lastTIM5Value = currentTIM5;
}

bool HardwareTimer::isInitialized() {
  // If TIM2 is running, we’re good (shared register visible to both cores)
  return (TIM2->CR1 & TIM_CR1_CEN) != 0;
}


uint32_t HardwareTimer::getMicros() {
  if (!isInitialized()) return 0;
  return TIM2->CNT;  // 1 tick = 1 µs
}

uint32_t HardwareTimer::getMillis() {
  if (!isInitialized()) return 0;
  return TIM2->CNT / 1000U;
}

// ---- 64-bit readers (race-safe) ----
static inline uint64_t compose64(uint32_t hi, uint32_t lo) {
  return ( (uint64_t)hi << 32 ) | lo;
}

// Read HI (TIM5), then LO (TIM2), then re-read HI to check for wrap
uint64_t HardwareTimer::getMicros64() {
  if (!isInitialized()) return 0;

  uint32_t hi1 = TIM5->CNT;   // number of TIM2 overflows seen
  uint32_t lo  = TIM2->CNT;   // low 32 bits (µs)
  uint32_t hi2 = TIM5->CNT;

  if (hi2 != hi1) {
    // Overflow happened between reads; re-sample LO under the new HI
    lo  = TIM2->CNT;
    hi1 = hi2;
  }
  return compose64(hi1, lo);   // total microseconds since reset
}

uint64_t HardwareTimer::getMillis64() {
  return getMicros64() / 1000ULL;
}

void HardwareTimer::reset() {
  if (!isInitialized()) return;
  // Reset both counters; order doesn’t matter because we read with hi-lo-hi
  TIM2->CNT = 0;
  TIM5->CNT = 0;
  __DMB(); __DSB();
}
