#include "HardwareTimer.h"

// One handle lives on the M7 only; don't rely on it for reads on M4.
TIM_HandleTypeDef HardwareTimer::htim2;

#ifdef CORE_CM7
bool HardwareTimer::begin() {
    // Enable TIM2 clock & configure for 1 MHz tick
    __HAL_RCC_TIM2_CLK_ENABLE();

    htim2.Instance = TIM2;

    // Compute prescaler from actual timer clock to avoid hardcoding 239
    // (TIMx clock on H7 is 2 * PCLK when APB prescaler != 1)
    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    RCC_ClkInitTypeDef clk;
    uint32_t flashLatency;
    HAL_RCC_GetClockConfig(&clk, &flashLatency);
    bool apbDivIs1 = (clk.APB1CLKDivider == RCC_HCLK_DIV1);
    uint32_t timclk = apbDivIs1 ? pclk1 : (pclk1 * 2U);

    uint32_t target = 1000000U; // 1 MHz
    uint32_t presc  = (timclk / target) - 1U;

    htim2.Init.Prescaler = presc;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 0xFFFFFFFFU;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) return false;

    __HAL_TIM_SET_COUNTER(&htim2, 0);
    if (HAL_TIM_Base_Start(&htim2) != HAL_OK) return false;

    // Ensure the write is visible to the other core
    __DMB(); __DSB();
    return true;
}
#endif

// Only check the CEN bit; don't trust the RCC macro on CM4.
bool HardwareTimer::isInitialized() {
    return (TIM2->CR1 & TIM_CR1_CEN) != 0;
}

uint32_t HardwareTimer::getMicros() {
    if ((TIM2->CR1 & TIM_CR1_CEN) == 0) return 0; // not started yet
    // Direct register read; no HAL handle needed
    return TIM2->CNT;
}

uint32_t HardwareTimer::getMillis() {
    if ((TIM2->CR1 & TIM_CR1_CEN) == 0) return 0;
    return TIM2->CNT / 1000U;
}

void HardwareTimer::reset() {
    if ((TIM2->CR1 & TIM_CR1_CEN) == 0) return;
    TIM2->CNT = 0;
    __DMB(); __DSB();
}
