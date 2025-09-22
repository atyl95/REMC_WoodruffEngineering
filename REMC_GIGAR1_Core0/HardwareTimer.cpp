// Aaron Note: 9.19.2025
// This HardwareTimer is currently working by using TIM2 as the low word and TIM5 as the high word.
// It currently works but there are some concerns about the usage of TIM2 and TIM5 in the Arduino GIGA R1.
// If you are seeing anything interrupting PWM or serial, this *might* be the cause.
//
// There is also a nice upgrade that could be made to this class at some point:
// TIM2 *should* trigger an increment of TIM5 on overflow at the hardware level. It does not right now.
// Right now, checkRollover() must be called at least once every 71 minutes.
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
  
  __HAL_TIM_SET_COUNTER(&htim2, 0);
  if (HAL_TIM_Base_Start(&htim2) != HAL_OK) return false;

  __DMB(); __DSB(); // make sure other core sees started timers
  return true;
}

#endif // CORE_CM7

uint32_t HardwareTimer::lastTIM2Value = 0;
uint32_t HardwareTimer::rolloverCount = 0;
void HardwareTimer::checkRollover() {
  if (!isInitialized()) return;
  
  uint32_t currentTIM2 = TIM2->CNT;
  // Check if TIM2 rolled over (current value is less than last value)
  if (currentTIM2 < lastTIM2Value) {
    rolloverCount++;
  }
  lastTIM2Value = currentTIM2;
}

bool HardwareTimer::isInitialized() {
  // If TIM2 is running, we’re good (shared register visible to both cores)
  return (TIM2->CR1 & TIM_CR1_CEN) != 0;
}

uint32_t HardwareTimer::getRolloverCount() {
  return rolloverCount;
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
  checkRollover();

  uint32_t lo  = TIM2->CNT;   // low 32 bits (µs)
  uint32_t hi = rolloverCount;
  return compose64(hi, lo);   // total microseconds since reset
}

uint64_t HardwareTimer::getMillis64() {
  checkRollover();
  return getMicros64() / 1000ULL;
}

void HardwareTimer::reset() {
  if (!isInitialized()) return;
  // Reset both counters; order doesn’t matter because we read with hi-lo-hi
  TIM2->CNT = 0;
  __DMB(); __DSB();
}
