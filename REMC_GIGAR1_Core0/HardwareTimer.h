#ifndef HARDWARE_TIMER_H
#define HARDWARE_TIMER_H

#include <Arduino.h>
#include "stm32h7xx_hal.h"

class HardwareTimer {
public:
    // Initialize the shared hardware timer (call once from M7 core)
    static bool begin();
    
    // Get synchronized microseconds (callable from both cores)
    static uint32_t getMicros();
    
    // Get synchronized milliseconds (callable from both cores)
    static uint32_t getMillis();
    
    // Check if timer is initialized (checks hardware state)
    static bool isInitialized();
    
    // Reset the timer counter to zero
    static void reset();

private:
    static TIM_HandleTypeDef htim2;
    
    // Private constructor - static class only
    HardwareTimer() = delete;
};

#endif // HARDWARE_TIMER_H