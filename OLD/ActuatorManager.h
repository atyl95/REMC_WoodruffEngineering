// ActuatorManager.h (Extended with Move State Accessor)
#ifndef ACTUATOR_MANAGER_H
#define ACTUATOR_MANAGER_H

#include <Arduino.h>

enum ActuatorMoveState {
  ACT_STOP = 0,
  ACT_FWD,
  ACT_BWD
};

namespace ActuatorManager {
  void init();
  void run(ActuatorMoveState moveState);
  ActuatorMoveState getCurrentMove();
}

#endif