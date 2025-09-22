#include <Arduino.h>
#include <RPC.h>
#include "SharedRing.h"
#include "UdpManager.h"
#include "SampleCollector.h"
#include "StateManager.h"
#include "ActuatorManager.h"
#include "SDRAM.h"
#include "HardwareTimer.h"
#include "NTPClient.h"
#include "Config.h"

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

  Serial.println("[Serial Core] NTP beginning.");
  // Initialize the singleton with a shared UDP object
  NTPClient::initialize(UdpManager::getNTPUdpObject());
  Serial.println("[Serial Core] NTP initialized.");
  NTPClient::begin(Config::NTP_SERVER, Config::NTP_CLIENT_PORT);
  Serial.println("[Serial Core] NTP begun.");
  if(NTPClient::sync()) {
    Serial.println("[Serial Core] NTP synced.");
  }
  else Serial.println("[Serial Core] NTP FAILED TO SYNC.");


  // Initialize the shared timer (only call from M7)
  if (HardwareTimer::begin()) {
      Serial.println("[Serial Core] Hardware timer initialized successfully");
  } else {
      Serial.println("[Serial Core] Failed to initialize hardware timer");
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

  static uint64_t last_print = 0;
  uint64_t current_time = HardwareTimer::getMicros64();

  // Print every 1 second (1,000,000 microseconds)
  if (current_time - last_print >= 1000000) {
      char buf[32];
      snprintf(buf, sizeof(buf), "%" PRIu64, current_time);
      Serial.print("[Serial Core] Timer micros: ");
      Serial.println(buf);
    Serial.println("[Serial Core] Rollover count: " + String(HardwareTimer::getRolloverCount()));

    last_print = current_time;
  }
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