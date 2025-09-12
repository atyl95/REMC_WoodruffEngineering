// REMC_SwitchControl.ino

#include <Arduino.h>
#include <SPI.h>            // Required for Ethernet library
//#define SPI_ETHERNET_SETTINGS SPISettings(42000000, MSBFIRST, SPI_MODE0)  //SPI max speed
#include <Ethernet.h>       // For Networking
#include <EthernetUdp.h>    // For UDP communication

#include "PinConfig.h"       // Pin definitions (MSW pins, outputs pins)
#include "TimerManager.h"    // ADC sampling, PWM outputs
#include "ActuatorManager.h" // ACT_FWD, ACT_BWD, ACT_STOP
#include "StateManager.h"    // Auto/manual FSM
#include "UdpManager.h"      // Telemetry (and multicast command receive)

void setup() { 
  Serial.begin(115200); 
  while (!Serial) { /* wait a moment for USB serial */ }
  Serial.println(F("\nREMC Switch Control Initializing..."));

  Serial.println(F("→ ActuatorManager init"));
  ActuatorManager::init();

  Serial.println(F("→ StateManager init"));
  StateManager::init();  

  Serial.println(F("→ UdpManager init"));
  UdpManager::init(); 

  delay(1000);

  Serial.println(F("→ TimerManager init"));
  TimerManager::init();
  Serial.println(F("Setup complete. Running..."));
}

void loop() {
  // Run the auto/manual state machine
  StateManager::update();

  // Handle telemetry send (when updated Telemtry received)/receive 
  UdpManager::update();


  TimerManager::update();


  // Mirror the MSW inputs to digital outputs D5/D6
  {
    bool a_trig = (digitalRead(PIN_MSW_POS_A) == LOW);
    bool b_trig = (digitalRead(PIN_MSW_POS_B) == LOW);
    digitalWrite(PIN_MSW_A_OUT, a_trig ? HIGH : LOW);
    digitalWrite(PIN_MSW_B_OUT, b_trig ? HIGH : LOW);
  }
}
