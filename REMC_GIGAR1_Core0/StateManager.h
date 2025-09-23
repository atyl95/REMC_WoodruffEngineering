// StateManager.h
#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include <Arduino.h>
#include "ActuatorManager.h"

namespace StateManager {

  // Auto‐mode FSM states
  enum class SystemState {
    STATE_IDLE,
    STATE_ARM_START_ENGAGE,
    STATE_ARM_PAUSE_BEFORE_PULLBACK,
    STATE_ARM_PULL_BACK,
    STATE_ARMED_READY,
    STATE_FIRING,
    STATE_HOLD_AFTER_FIRE
  };

  // Simplified status for telemetry
  enum class OperationalStatus : uint8_t {
    STATUS_IDLE = 0,
    STATUS_ENGAGING = 1,
    STATUS_PAUSE_BEFORE_PULLBACK = 2,
    STATUS_PULLING_BACK = 3,
    STATUS_ARMED = 4,
    STATUS_FIRING = 5,
    STATUS_HOLDING = 6,
    STATUS_MANUAL_MODE = 7,
    STATUS_UNKNOWN = 8
  };

  // Core lifecycle
  void init();
  void update();

  // Auto‐mode commands
  void requestArm();                 // IDLE → ARM sequence
  void requestDisarm();              // Force reset to IDLE
  void triggerSoftwareActuate();     // UDP "FIRE SWITCH" button

  void manualActuatorControl(ActuatorMoveState moveCmd);
  void manualEMEnable();
  void manualEMDisable();


  // Mode switch
  void enableManualMode();           // Enter Manual Mode
  void disableManualMode();          // Return to Auto Mode
  void enableHoldAfterFireMode();    // UDP "Enable Hold-After-Fire Mode"
  void disableHoldAfterFireMode();   // UDP "Disable Hold-After-Fire Mode"
  bool isHoldAfterFireModeActive();


  // Telemetry
  bool isReady();                    // True only if in STATE_ARMED_READY & Auto
  bool isEmActActive();
  bool getActuateState();
  const char* getCurrentStateName();
  OperationalStatus getOperationalStatus();
  bool isManualModeActive();

// MSW inputs snapshot (cached each update)
  struct InputMswSnapshot {
    // Active-LOW semantics preserved (true == pin is LOW/triggered)
    bool mswA_low;
    bool mswB_low;
    // Optional: timestamp microseconds (when the snapshot was taken)
    uint32_t read_us;
  };

// Returns the most recent snapshot captured by StateManager::update()
InputMswSnapshot getInputMswSnapshot();

  // ─── New error flags ────────────────────────────────────────────────────────
  // Bit 0: ARM → MSW_A never triggered within timeout   ("Arm‐timeout")
  // Bit 1: PULL‐BACK → MSW_B never triggered within timeout   ("Pull‐back timeout")
  // Bit 2: EM Retention lost while "armed"   ("Retain‐fail")
  uint8_t getErrorFlags();           // returns 0–7, each bit is one of the above
}

#endif // STATE_MANAGER_H
