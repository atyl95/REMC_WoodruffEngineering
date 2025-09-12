#include <Arduino.h>
#include <RPC.h>
#include "SharedRing.h"
#include "UdpManager.h"
#include "SampleCollector.h"
#include "SDRAM.h"

void setup() { 
  Serial.begin(115200);
  while (!Serial) { }    // wait (native-USB boards)
  Serial.println(F("\nREMC Switch Control Initializing (Dual Core)..."));

  // 1) Initialize SDRAM hardware
  if (!SDRAM.begin()) {
    Serial.println("[Serial Core] ERROR: SDRAM init failed!");
    while (1);
  }

  // Initialize Sample Collector (includes SharedRing setup)
  if (!SampleCollector::init()) {
    Serial.println("[Serial Core] ERROR: SampleCollector init failed!");
    while(1); // Halt on initialization failure
  }

  // Initialize UDP Manager
  Serial.println(F("[Serial Core] UdpManager init"));
  UdpManager::init();

  // Starts Sampling Core (1)
  RPC.begin();
  
  Serial.println(F("[Serial Core] Ready - call startGathering() to begin"));
  SampleCollector::startGathering();
}

void loop() {

  // Print RPC messages from M4 Core
  DEBUG_printRPCMessages();
  
  // Process samples (main sample collection logic)
  SampleCollector::update();
  
  // Handle incoming UDP commands
  UdpManager::update();}

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