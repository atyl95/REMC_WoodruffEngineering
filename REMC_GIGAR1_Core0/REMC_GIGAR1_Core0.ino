#include <Arduino.h>
#include <RPC.h>
#include "SharedRing.h"
#include "Logger.h"

constexpr int MAX_FETCH = 1024;
static Sample sampleBuffer[MAX_FETCH];   // lives in .bss, allocated once

// Rolling buffer to check time between samples
constexpr int WINDOW = 20;
static Sample ringBuf[WINDOW];
static int ringCount = 0;
static int ringIndex = 0;

void setup() { 
  Logger::init(0);
  Serial.println(F("\nREMC Switch Control Initializing (Dual Core)..."));

  // Make sure our shared ring buffer is in the right spot
  Serial.print("[Serial Core] SharedRing Address ");
  Serial.println((uintptr_t)&g_ring, HEX);
  SharedRing_Init();

  // Starts Sampling Core (1)
  RPC.begin();
}

void loop() {
  DEBUG_printRPCMessages();
  size_t count = SharedRing_Consume(sampleBuffer, MAX_FETCH);
  DEBUG_printSampleDiagnostics(count);
}


// DEBUG FUNCTIONS - They don't need to stick around
void DEBUG_printRPCMessages()
{
  // Pump any RPC messages into Serial
  String buffer = "";
  while (RPC.available()) {
    buffer += (char)RPC.read();
  }
  if (buffer.length() > 0) {
    Serial.print(buffer);
  }
}
void DEBUG_printSampleDiagnostics(size_t count)
{
  Serial.print("[Serial Core] Samples collected: ");
  Serial.println(count);

  if (count > 0) {
    for (size_t i = 0; i < count; i++) {
      // Add sample to rolling buffer
      ringBuf[ringIndex] = sampleBuffer[i];
      ringIndex = (ringIndex + 1) % WINDOW;
      if (ringCount < WINDOW) ringCount++;
    }
  }

  // Only compute when we have a full window
  if (ringCount == WINDOW) {
    uint64_t totalDelta = 0;
    for (int i = 1; i < WINDOW; i++) {
      int prev = (ringIndex + i - WINDOW - 1 + WINDOW) % WINDOW;
      int curr = (prev + 1) % WINDOW;
      totalDelta += (uint32_t)(ringBuf[curr].t_us - ringBuf[prev].t_us);
    }
    uint32_t avgDelta = totalDelta / (WINDOW - 1);

    Serial.print("[Serial Core] Average spacing (us) over last ");
    Serial.print(WINDOW);
    Serial.print(" samples = ");
    Serial.println(avgDelta);
  }
}