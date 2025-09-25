// StateManager.cpp
#include "StateManager.h"
#include "ActuatorManager.h"
#include "PinConfig.h"
#include "SampleCollector.h"
#include <Arduino.h>

// Snapshot of MSW switch status
namespace {
  StateManager::InputMswSnapshot g_inputs = {false, false, 0};
}

namespace {
  using SystemState = StateManager::SystemState;

  // ─── Internal FSM state ───────────────────────────────────────────────────
  static SystemState currentState = SystemState::STATE_IDLE;
  static bool isInManualMode       = false;
  static bool holdAfterFireMode    = false;
  static bool holdAfterFireModeEMFireFlag = false;  //mswA doesn't open fast enough for Hold mode so this makes sure mswA is open before looking for endstop on close

  // Output tracking
  static bool readyOutputState     = false;  // "Ready" LED
  static bool emActOutputState     = false;  // EM coil pin state

  // Software‐trigger flags (from UDP/web)
  volatile bool softwareArmTrigger     = false;
  volatile bool softwareFireTrigger    = false;

  // Timestamp markers for timeouts
  static unsigned long stateStartMs               = 0;  // when we entered the current state
  static unsigned long pauseBeforePullbackStartMs = 0;  // when MSW_A is fired  

  // Error bits (persist until cleared by next transition to IDLE)
  static bool errArmTimeout       = false; // bit 0
  static bool errPullbackTimeout  = false; // bit 1
  static bool errRetainFail       = false; // bit 2

  // constants:
  static constexpr unsigned long ARM_TIMEOUT_MS            = 1000; // ms by which MSW_A must fire
  static constexpr unsigned long PULLBACK_TIMEOUT_MS       = 1000; // ms by which MSW_B must fire
  static constexpr unsigned long PAUSE_BEFORE_PULLBACK_MS  = 500;  // ms which actuator pauses after MSW_A fired before pullback
}

// ─── Private helper to set EM pin ───────────────────────────────────────────
static void setEMState(bool on) {
  emActOutputState = on;
  digitalWrite(PIN_EM_ACT, on ? HIGH : LOW);
}

// ─── Private helper to stop actuator and clear "ready" LED ─────────────────
static void resetToIdle() {
  ActuatorManager::run(ACT_STOP);
  setEMState(false);
  if (readyOutputState) {
    digitalWrite(PIN_READY, LOW);
    readyOutputState = false;
  }
  softwareArmTrigger   = false;
  softwareFireTrigger  = false;
  // Clear error bits as we re‐enter IDLE:
  errArmTimeout      = false;
  errPullbackTimeout = false;
  errRetainFail      = false;
}

namespace StateManager {

void init() {
  // Configure pins
  pinMode(PIN_MSW_POS_A, INPUT_PULLUP);
  pinMode(PIN_MSW_POS_B, INPUT_PULLUP);
  pinMode(PIN_EM_ACT,    OUTPUT);
  pinMode(PIN_READY,     OUTPUT);

  isInManualMode = false;
  currentState   = SystemState::STATE_IDLE;
  resetToIdle();
  Serial.println(F("StateManager: Initialized in AUTO mode."));
}

void enableManualMode() {
  if (!isInManualMode) {
    softwareArmTrigger  = false;
    softwareFireTrigger = false;
    isInManualMode = true;
    Serial.println(F("StateManager: Manual Mode ENABLED"));
    resetToIdle();
  }
}

void disableManualMode() {
  if (isInManualMode) {
    isInManualMode = false;
    Serial.println(F("StateManager: Manual Mode DISABLED"));
    currentState = SystemState::STATE_IDLE;
    resetToIdle();
  }
}

void enableHoldAfterFireMode() {
  holdAfterFireMode = true;
  Serial.println(F("StateManager: Hold-After-Fire Mode ENABLED"));
}

void disableHoldAfterFireMode() {
  holdAfterFireMode = false;
  Serial.println(F("StateManager: Hold-After-Fire Mode DISABLED"));
}

void requestArm() {
  if (isInManualMode) {
    Serial.println(F("StateManager: ARM ignored in Manual Mode."));
    return;
  }
  Serial.println(F("StateManager: ARM request"));
  if (currentState == SystemState::STATE_IDLE) {
    softwareArmTrigger = true;
  }
}

void requestDisarm() {
  Serial.println(F("StateManager: DISARM request"));
  currentState = SystemState::STATE_IDLE;
  resetToIdle();
}

void triggerSoftwareActuate() {
  if (isInManualMode) {
    Serial.println(F("StateManager: FIRE ignored in Manual Mode."));
    return;
  }
  //Serial.println(F("StateManager: FIRE trigger (Auto)."));
  if (currentState == SystemState::STATE_ARMED_READY) {
    softwareFireTrigger = true;
  }
}

void update() {
  // Read MSW pins (active LOW when pressed)
  bool a_low, b_low;
  msw_read_both_fast(a_low, b_low);

  // Cache it so the same values are seen in this cycle
  g_inputs.mswA_low = a_low;
  g_inputs.mswB_low = b_low;
  g_inputs.read_us  = micros();  // optional

  // Mirror outputs from the same snapshot
  digitalWrite(PIN_MSW_A_OUT, a_low ? HIGH : LOW);
  digitalWrite(PIN_MSW_B_OUT, b_low ? HIGH : LOW);
 
  // If manual mode, do nothing in the FSM (the EM/actuator pins are managed elsewhere)
  if (isInManualMode) {
    digitalWrite(PIN_EM_ACT, emActOutputState ? HIGH : LOW);
    if (readyOutputState) {
      digitalWrite(PIN_READY, LOW);
      readyOutputState = false;
    }
    return;
  }

  // Remember when we entered this state if it just changed
  static SystemState lastState = currentState;
  if (lastState != currentState) {
    stateStartMs = millis();
    lastState   = currentState;
  }

  switch (currentState) {
    // ───────────────────────────────────────────────────────────────────────────
    case SystemState::STATE_IDLE:
      // Pin outputs are already off from resetToIdle()
      if (softwareArmTrigger) {
        Serial.println(F("StateManager: IDLE → ARM_START_ENGAGE"));
        currentState = SystemState::STATE_ARM_START_ENGAGE;
        softwareArmTrigger = false;
        setEMState(true);  // energize EM immediately
        stateStartMs = millis();
        // Start driving forward toward MSW_A
        ActuatorManager::run(ACT_FWD);
      } else {
        resetToIdle();
      }
      break;

    // ───────────────────────────────────────────────────────────────────────────
    case SystemState::STATE_ARM_START_ENGAGE:
      // We have driven actuator forward with EM=ON; waiting for MSW_A
      //if (mswA_low) {
      //if (g_inputs.mswA_low) {
      if (true) {
        ActuatorManager::run(ACT_STOP);
        Serial.println(F("StateManager: MSW_A triggered → ARM_PAUSE_BEFORE_PULLBACK"));
        currentState = SystemState::STATE_ARM_PAUSE_BEFORE_PULLBACK;
        // Pause to give actuator time to come to rest before reversing direction toward MSW_B to "pull back"
        pauseBeforePullbackStartMs = millis();
      } else if ((millis() - stateStartMs) > ARM_TIMEOUT_MS) {
        // MSW_A never tripped within timeout: set bit 0
        if (errArmTimeout == false)
          Serial.println(F("StateManager: ERROR → ARM_TIMEOUT (bit0)"));
        errArmTimeout = true;
        // Still let the actuator continue until it hits MSW_A so we may recover
      }
      break;

    // ───────────────────────────────────────────────────────────────────────────
    case SystemState::STATE_ARM_PAUSE_BEFORE_PULLBACK:
      if ((millis() - pauseBeforePullbackStartMs) > PAUSE_BEFORE_PULLBACK_MS) {
        Serial.println(F("StateManager: ARM_PAUSE_BEFORE_PULLBACK → ARM_PULL_BACK"));
        currentState = SystemState::STATE_ARM_PULL_BACK;
        // Reverse direction toward MSW_B to "pull back"
        ActuatorManager::run(ACT_BWD);
        stateStartMs = millis();
      } 
      break;

    // ───────────────────────────────────────────────────────────────────────────
    case SystemState::STATE_ARM_PULL_BACK:
      // We are driving backward, waiting for MSW_B to confirm "fully open"
      //if (mswB_low) {
      //if (g_inputs.mswB_low) {
      if (true) {
        Serial.println(F("StateManager: MSW_B triggered → ARMED_READY"));
        currentState = SystemState::STATE_ARMED_READY;
        ActuatorManager::run(ACT_STOP);
        readyOutputState = true;
        digitalWrite(PIN_READY, HIGH);
        stateStartMs = millis();
      } else if ((millis() - stateStartMs) > PULLBACK_TIMEOUT_MS) {
        // MSW_B never tripped within timeout: set bit 1
        errPullbackTimeout = true;
        Serial.println(F("StateManager: ERROR → PULLBACK_TIMEOUT (bit1)"));
        // Still attempt to stop when MSW_B eventually trips
      }
      break;

    // ───────────────────────────────────────────────────────────────────────────
    case SystemState::STATE_ARMED_READY:
      // The EM is supposed to be holding the switch open (MSW_A_low == true)
      // (math: mswA_low==true means MSW_A is pressed → switch open & sitting on A)
       //if (!mswA_low) {
       //if (!g_inputs.mswA_low) {
       if (false) {
         // MSW_A is no longer held closed, yet EM is still ON: "retain" failed
         errRetainFail = true;
         Serial.println(F("StateManager: ERROR → RETAIN_FAIL (bit2)"));
       }

      // Remain in ARMED_READY until FIRE or IDLE
      if (softwareFireTrigger) {
        softwareFireTrigger = false;
        // Drop EM, then reset "ready" LED
        setEMState(false);
        digitalWrite(PIN_READY, LOW);
        readyOutputState = false;
        if (holdAfterFireMode) {
          Serial.println(F("StateManager: ARMED_READY → HOLD AFTER FIRE"));
          currentState = SystemState::STATE_HOLD_AFTER_FIRE;
          setEMState(false);
          //ActuatorManager::run(ACT_FWD);
        } else {
          Serial.println(F("StateManager: ARMED_READY → FIRE"));
          currentState = SystemState::STATE_FIRING;
        }
        stateStartMs = millis();
      }
      break;

    // ───────────────────────────────────────────────────────────────────────────
    case SystemState::STATE_FIRING:
      // We've already dropped EM and stopped the actuator in the transition
      // Back to IDLE
      Serial.println(F("StateManager: FIRING → IDLE"));
      currentState = SystemState::STATE_IDLE;
      resetToIdle();
      stateStartMs = millis();
      break;

    // ────────────────────────────────────────────────────────────────────────
    case SystemState::STATE_HOLD_AFTER_FIRE:
      // Actuator is holding after a fire event
        if (!g_inputs.mswA_low){
          holdAfterFireModeEMFireFlag = true;
          ActuatorManager::run(ACT_FWD);
         }

        if (g_inputs.mswA_low && holdAfterFireModeEMFireFlag == true) {
          ActuatorManager::run(ACT_STOP);
          Serial.println(F("StateManager: MSW_A triggered → IDLE"));
          currentState = SystemState::STATE_IDLE;
          resetToIdle();
          holdAfterFireModeEMFireFlag = false;
         }
      // if (softwareArmTrigger) {
      //   Serial.println(F("StateManager: HOLD_AFTER_FIRE → ARM_START_ENGAGE"));
      //   softwareArmTrigger = false;
      //   //setEMState(true);
      //   currentState = SystemState::STATE_ARM_START_ENGAGE;
      //   stateStartMs = millis();
      //   // continue driving forward toward MSW_A
       // }
       break;
   }
 }



uint8_t getErrorFlags() {
  uint8_t bits = 0;
  if (errArmTimeout)      bits |= (1 << 0);
  if (errPullbackTimeout) bits |= (1 << 1);
  if (errRetainFail)      bits |= (1 << 2);
  return bits;
}

// ─── The rest of your accessors ─────────────────────────────────────────────

bool isReady()               { return !isInManualMode && currentState == SystemState::STATE_ARMED_READY; }
bool isEmActActive()         { return emActOutputState; }
bool getActuateState()       { return !isInManualMode && currentState != SystemState::STATE_IDLE && currentState != SystemState::STATE_FIRING; }
const char* getCurrentStateName() {
  if (isInManualMode) return "MANUAL_MODE";
  switch (currentState) {
    case SystemState::STATE_IDLE:             return "IDLE";
    case SystemState::STATE_ARM_START_ENGAGE: return "ARM_START_ENGAGE";
    case SystemState::STATE_ARM_PAUSE_BEFORE_PULLBACK: return "ARM_PAUSE_BEFORE_PULLBACK";
    case SystemState::STATE_ARM_PULL_BACK:    return "ARM_PULL_BACK";
    case SystemState::STATE_ARMED_READY:      return "ARMED_READY";
    case SystemState::STATE_FIRING:           return "FIRING";
    case SystemState::STATE_HOLD_AFTER_FIRE:  return "HOLD_AFTER_FIRE";
    default:                                  return "UNKNOWN";
  }
}

StateManager::OperationalStatus getOperationalStatus() {
  if (isInManualMode) return OperationalStatus::STATUS_MANUAL_MODE;
  switch (currentState) {
    case SystemState::STATE_IDLE:             return OperationalStatus::STATUS_IDLE;
    case SystemState::STATE_ARM_START_ENGAGE: return OperationalStatus::STATUS_ENGAGING;
    case SystemState::STATE_ARM_PAUSE_BEFORE_PULLBACK: return OperationalStatus::STATUS_PAUSE_BEFORE_PULLBACK;
    case SystemState::STATE_ARM_PULL_BACK:    return OperationalStatus::STATUS_PULLING_BACK;
    case SystemState::STATE_ARMED_READY:      return OperationalStatus::STATUS_ARMED;
    case SystemState::STATE_FIRING:           return OperationalStatus::STATUS_FIRING;
    case SystemState::STATE_HOLD_AFTER_FIRE:  return OperationalStatus::STATUS_HOLDING;
    default:                                  return OperationalStatus::STATUS_UNKNOWN;
  }
}

bool isManualModeActive() { return isInManualMode; }
bool isHoldAfterFireModeActive() { return holdAfterFireMode; }

void manualEMEnable() {
  if (!isInManualMode) {
    Serial.println(F("StateManager: Manual EM enable IGNORED (not in Manual Mode)."));
    return;
  }
  setEMState(true);
  Serial.println(F("StateManager: Manual EM ENABLED"));
}

void manualEMDisable() {
  if (!isInManualMode) {
    Serial.println(F("StateManager: Manual EM disable IGNORED (not in Manual Mode)."));
    return;
  }
  setEMState(false);
  Serial.println(F("StateManager: Manual EM DISABLED"));
}

void manualActuatorControl(ActuatorMoveState cmd) { 

  auto in = getInputMswSnapshot();
  const bool atA = in.mswA_low;
  const bool atB = in.mswB_low; 

  if (!isInManualMode) {
    Serial.println(F("StateManager: Manual Actuator IGNORED (not in Manual Mode)."));
    return;
  }
  if (cmd == ACT_BWD && atB) {
    Serial.println(F("Manual BWD blocked: already at B endstop"));  //Bottom limit reached
    return;
  } 
  if (cmd == ACT_FWD && atA) {
    Serial.println(F("Manual FWD blocked: already at A endstop"));  //Top limit reached
    return;
  }
  Serial.print(F("StateManager (Manual): "));
  if(cmd == ACT_STOP){
    Serial.println(F("STOP"));
  }
  else{
    Serial.println(cmd == ACT_FWD ? F("ENGAGE") : F("DISENGAGE"));
  }
  ActuatorManager::run(cmd);
}


// MSW getter (declared outside namespace in header)
StateManager::InputMswSnapshot getInputMswSnapshot() {
  return g_inputs;
} 

} // namespace StateManager