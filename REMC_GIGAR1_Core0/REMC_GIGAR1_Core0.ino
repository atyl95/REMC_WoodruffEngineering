#include <Arduino.h>
#include <RPC.h>
#include "SharedRing.h"
#include "Logger.h"
#include "UdpManager.h"
#include "SDRAM.h"

// ===== SAMPLE STORAGE SYSTEM =====
constexpr size_t STORAGE_CAPACITY = 300000;   // 10kHz sample rate 
static Sample* sampleStorage = nullptr;      // Will allocate in setup()
static volatile size_t storageIndex = 0;    // Current storage position
static volatile bool gatheringActive = false; // Gathering state
static volatile size_t totalSamplesStored = 0; // Total samples in storage

// Processing buffer for consuming from SharedRing
constexpr int MAX_FETCH = 1024;  // Large batches for efficiency when not sending UDP
static Sample sampleBuffer[MAX_FETCH];

// Rolling buffer for diagnostics
constexpr int WINDOW = 20;
static Sample ringBuf[WINDOW];
static int ringCount = 0;
static int ringIndex = 0;

void setup() { 
  Logger::init(0);
  Serial.println(F("\nREMC Switch Control Initializing (Dual Core)..."));

  // 1) Initialize SDRAM hardware
  if (!SDRAM.begin()) {
    Serial.println("[Serial Core] ERROR: SDRAM init failed!");
    while (1);
  }

  // Allocate sample storage in external SDRAM
  Serial.print("[Serial Core] Allocating storage for ");
  Serial.print(STORAGE_CAPACITY);
  Serial.print(" samples (");
  Serial.print((STORAGE_CAPACITY * sizeof(Sample)) / (1024.0*1024.0), 2);
  Serial.println(" MB)");
  
  sampleStorage = (Sample*) SDRAM.malloc(STORAGE_CAPACITY * sizeof(Sample));
  if (sampleStorage == nullptr) {
    Serial.println("[Serial Core] ERROR: Failed to allocate sample storage!");
    while(1); // Halt on allocation failure
  }
  Serial.println("[Serial Core] Sample storage allocated successfully");

  // Make sure our shared ring buffer is in the right spot
  Serial.print("[Serial Core] SharedRing Address ");
  Serial.println((uintptr_t)&g_ring, HEX);
  SharedRing_Init();

  // Initialize UDP Manager
  Serial.println(F("[Serial Core] UdpManager init"));
  UdpManager::init();

  // Starts Sampling Core (1)
  RPC.begin();
  
  Serial.println(F("[Serial Core] Ready - call startGathering() to begin"));

  startGathering();
}

void loop() {
  static uint32_t debug_counter = 0;
  debug_counter++;
  
  // Only show critical RPC messages occasionally
  if (debug_counter % 1000 == 0) {
    DEBUG_printRPCMessages();
  }
  
  // Always consume samples from SharedRing (Core1 never stops sampling)
  size_t count = SharedRing_Consume(sampleBuffer, MAX_FETCH);
  if (count > 0) {
    if (gatheringActive) {
      // GATHERING MODE: Store samples in storage buffer
      for (size_t i = 0; i < count; i++) {
        if (storageIndex < STORAGE_CAPACITY) {
          sampleStorage[storageIndex] = sampleBuffer[i];
          storageIndex++;
        } else {
          // Storage full - stop gathering automatically
          stopGathering();

          // TEMPORARY BEFORE RECEIVING COMMANDS
          sendAllSamples();

          break;
        }
      }
      totalSamplesStored = storageIndex;
    } else {
      // NOT GATHERING: Just discard samples (prevents ring buffer overflow)
      // This prevents ring buffer overflow when not actively gathering
    }
  }
  
  // Handle incoming UDP commands
  UdpManager::update();

  // Show status occasionally
  if (debug_counter % 2000 == 0) {
    Serial.print("OVR:");
    Serial.print(g_ring.overruns);
    Serial.print(" GATHERING:");
    Serial.print(gatheringActive ? "YES" : "NO");
    Serial.print(" STORED:");
    Serial.print(totalSamplesStored);
    Serial.print("/");
    Serial.println(STORAGE_CAPACITY);
  }
}


// ===== PUBLIC FUNCTIONS FOR EXTERNAL CONTROL =====

void startGathering() {
  Serial.println("[Serial Core] Starting sample gathering");
  
  // Reset storage
  storageIndex = 0;
  totalSamplesStored = 0;
  
  // Enable gathering
  gatheringActive = true;
  
  Serial.print("[Serial Core] Gathering started - storage capacity: ");
  Serial.print(STORAGE_CAPACITY);
  Serial.print(" samples (");
  Serial.print((float)STORAGE_CAPACITY / 10000.0, 1);  // Convert to seconds with 1 decimal
  Serial.println(" seconds)");
}

void stopGathering() {
  Serial.print("[Serial Core] Stopping sample gathering - collected ");
  Serial.print(totalSamplesStored);
  Serial.println(" samples");
  
  // Disable gathering but keep stored samples
  gatheringActive = false;
}

void sendAllSamples() {
  if (totalSamplesStored == 0) {
    Serial.println("[Serial Core] No samples to send");
    return;
  }
  
  Serial.print("[Serial Core] Sending ");
  Serial.print(totalSamplesStored);
  Serial.println(" samples over UDP...");
  
  uint32_t startTime = millis();
  size_t packetsSent = 0;
  size_t samplesSent = 0;
  
  // Send samples in optimal-sized batches (46 samples per UDP packet)
  constexpr size_t SAMPLES_PER_PACKET = 46;
  
  for (size_t i = 0; i < totalSamplesStored; i += SAMPLES_PER_PACKET) {
    size_t remainingSamples = totalSamplesStored - i;
    size_t batchSize = min(remainingSamples, SAMPLES_PER_PACKET);
    
    // Send this batch
    for (size_t j = 0; j < batchSize; j++) {
      UdpManager::addSample(sampleStorage[i + j]);
    }
    UdpManager::flushSamples();
    
    packetsSent++;
    samplesSent += batchSize;
    
    // Show progress every 1000 packets
    if (packetsSent % 1000 == 0) {
      Serial.print("[Serial Core] Sent ");
      Serial.print(samplesSent);
      Serial.print("/");
      Serial.print(totalSamplesStored);
      Serial.println(" samples");
    }
    
    // Small delay to prevent overwhelming the UDP stack
    if (packetsSent % 100 == 0) {
      delay(1); // 1ms pause every 100 packets
    }
  }
  
  uint32_t duration = millis() - startTime;
  
  Serial.print("[Serial Core] Transmission complete: ");
  Serial.print(packetsSent);
  Serial.print(" packets, ");
  Serial.print(samplesSent);
  Serial.print(" samples in ");
  Serial.print(duration);
  Serial.println("ms");
  
  // Calculate transmission rate
  if (duration > 0) {
    Serial.print("[Serial Core] Transmission rate: ");
    Serial.print((samplesSent * 1000) / duration);
    Serial.println(" samples/second");
  }
}

// ===== STATUS FUNCTIONS (OPTIONAL) =====

size_t getSamplesStored() {
  return totalSamplesStored;
}

bool isGathering() {
  return gatheringActive;
}

size_t getStorageCapacity() {
  return STORAGE_CAPACITY;
}

// ===== DEBUG FUNCTIONS =====

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

void DEBUG_printSampleDiagnostics(size_t count) {
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