// ActuatorManager.cpp (Extended with Move State Tracking)
#include "ActuatorManager.h"
#include "PinConfig.h"

namespace {
  ActuatorMoveState currentMove = ACT_STOP;
}

namespace ActuatorManager {

  void init() {
    pinMode(PIN_LIN_ACT_A, OUTPUT);
    pinMode(PIN_LIN_ACT_B, OUTPUT);
    digitalWrite(PIN_LIN_ACT_A, LOW);
    digitalWrite(PIN_LIN_ACT_B, LOW);
    currentMove = ACT_STOP;
  }

  void run(ActuatorMoveState moveState) {
    currentMove = moveState;
    switch (moveState) {
      case ACT_STOP:
        digitalWrite(PIN_LIN_ACT_A, LOW);
        digitalWrite(PIN_LIN_ACT_B, LOW);
        break;
      case ACT_FWD:
        digitalWrite(PIN_LIN_ACT_A, HIGH);
        digitalWrite(PIN_LIN_ACT_B, LOW);
        break;
      case ACT_BWD:
        digitalWrite(PIN_LIN_ACT_A, LOW);
        digitalWrite(PIN_LIN_ACT_B, HIGH);
        break;
    }
  }

  ActuatorMoveState getCurrentMove() {
    return currentMove;
  }

} // namespace ActuatorManager
