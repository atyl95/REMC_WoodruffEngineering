/*
  ---------------------------------------------------------------------------
  HardwareTimer – Shared Dual-Core Timer System (TIM2 + TIM5)
  ---------------------------------------------------------------------------

  This class provides a unified timebase that both the M7 and M4 cores can
  read. It builds on the STM32H747’s hardware timers:

    - TIM2: a 32-bit timer running at 1 MHz (1 tick = 1 microsecond).
    - TIM5: another 32-bit timer, set up to increment once every time TIM2
            overflows (every ~71 minutes at 1 MHz).

  Together, TIM2 (low word) and TIM5 (high word) act like a single 64-bit
  counter. This gives you a free-running clock that never wraps within any
  realistic timeframe (~584,000 years at 1 MHz).

  Functions:
    - getMicros() / getMillis():
        Return the raw 32-bit TIM2 value. Simple and fast, but will roll
        over every ~71 minutes.
    - getMicros64() / getMillis64():
        Return the full 64-bit value composed from TIM5 (overflows) and
        TIM2 (microseconds). This behaves like an "extended micros()" that
        is effectively endless for practical use.

  In short:
    - Use the 32-bit versions when you only care about short intervals and
      want maximum speed.
    - Use the 64-bit versions when you need an absolute timestamp or
      guaranteed correctness across the 71-minute wrap boundary.

  All counters are read directly from the hardware registers, so the M7 and
  M4 cores always see the same clock values with no software synchronization.
  ---------------------------------------------------------------------------
*/

#ifndef HARDWARE_TIMER_H
#define HARDWARE_TIMER_H

#include <Arduino.h>
#include "stm32h7xx_hal.h"

class HardwareTimer {
public:
    static bool begin();
    static bool isInitialized();
    static uint32_t getMicros();
    static uint32_t getMillis();
    static uint64_t getMicros64();
    static uint64_t getMillis64();
    static void checkRollover();
    static void reset();
private:
    static uint32_t lastTIM2Value;
    static uint32_t lastTIM5Value;
};
#endif // HARDWARE_TIMER_H