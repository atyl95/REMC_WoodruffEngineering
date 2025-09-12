#ifndef TIMER_MANAGER_H
#define TIMER_MANAGER_H

#include <Arduino.h>

namespace TimerManager {
  // Initialize timer interrupt
  void init();
  void update();

  // --- Accessor functions for RAW ADC values ---
  uint16_t getSwitchCurrentRaw();
  uint16_t getSwitchVoltageRaw();
  uint16_t getTemp1Raw();
  uint16_t getOutputVoltageARaw();
  uint16_t getOutputVoltageBRaw();

  // --- Accessor functions for SCALED physical values ---
  float getSwitchCurrentA();   
  float getSwitchVoltageKV();  
  float getTemp1DegC();         
  float getOutputVoltageAKV();  
  float getOutputVoltageBKV();  
}

#endif // TIMER_MANAGER_H