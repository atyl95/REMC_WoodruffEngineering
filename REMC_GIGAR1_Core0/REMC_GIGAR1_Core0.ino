#include <Arduino.h>
#include <RPC.h>
#include "SharedRing.h"
#include "UdpManager.h"
#include "SampleCollector.h"
#include "StateManager.h"
#include "ActuatorManager.h"
#include "SDRAM.h"
#include "HardwareTimer.h"

void setup() { 
  Serial.begin(115200);
  while (!Serial) { }    // wait (native-USB boards)
  Serial.println(F("\nREMC Switch Control Initializing (Dual Core)..."));

  // Initialize SDRAM hardware - important for storing 8MB worth of accumulated samples
  if (!SDRAM.begin()) {
    Serial.println("[Serial Core] ERROR: SDRAM init failed!");
    while (1);
  }
  // Initialize Sample Collector (includes SharedRing setup)
  if (!SampleCollector::init()) {
    Serial.println("[Serial Core] ERROR: SampleCollector init failed!");
    while(1); // Halt on initialization failure
  }

  // Initialize Actuator Manager
  Serial.println(F("[Serial Core] ActuatorManager init"));
  ActuatorManager::init();
  
  // Initialize State Manager
  Serial.println(F("[Serial Core] StateManager init"));
  StateManager::init();

  // Initialize UDP Manager
  Serial.println(F("[Serial Core] UdpManager init"));
  UdpManager::init();

    // Initialize the shared timer (only call from M7)
    if (HardwareTimer::begin()) {
        Serial.println("Hardware timer initialized successfully");
    } else {
        Serial.println("Failed to initialize hardware timer");
    }

  // Starts Sampling Core (1)
  RPC.begin();
  Serial.println(F("[Serial Core] Ready - call startGathering() to begin"));

}

void loop() {

  // Print RPC messages from M4 Core
  DEBUG_printRPCMessages();
  
  // Update State Manager (actuator control FSM)
  StateManager::update();
  
  // Process samples (main sample collection logic)
  SampleCollector::update();
  
  // Handle incoming UDP commands
  UdpManager::update();
}

// ===== DEBUG FUNCTIONS FROM M4 CORE =====
void DEBUG_printRPCMessages() {
  // Pump any RPC messages into Serial
  String buffer = "";
  while (RPC.available()) {
    buffer += (char)RPC.read();
  }
  if (buffer.length() > 0) {
    Serial.print(buffer);
  }
}