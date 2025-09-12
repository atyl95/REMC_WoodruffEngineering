#include "SampleCollector.h"
#include "UdpManager.h"
#include "Logger.h"
#include "SDRAM.h"

// Static member definitions
Sample* SampleCollector::sampleStorage = nullptr;
volatile size_t SampleCollector::storageCapacity = 0;
volatile size_t SampleCollector::storageIndex = 0;
volatile bool SampleCollector::gatheringActive = false;
volatile size_t SampleCollector::totalSamplesStored = 0;

Sample SampleCollector::sampleBuffer[MAX_FETCH];
Sample SampleCollector::ringBuf[WINDOW];
int SampleCollector::ringCount = 0;
int SampleCollector::ringIndex = 0;

bool SampleCollector::init(size_t capacity) {
    storageCapacity = capacity;
    
    // Initialize SharedRing buffer
    Serial.print("[SampleCollector] SharedRing Address ");
    Serial.println((uintptr_t)&g_ring, HEX);
    SharedRing_Init();
    
    // Allocate sample storage in external SDRAM
    Serial.print("[SampleCollector] Allocating storage for ");
    Serial.print(storageCapacity);
    Serial.print(" samples (");
    Serial.print((storageCapacity * sizeof(Sample)) / (1024.0*1024.0), 2);
    Serial.println(" MB)");
    
    sampleStorage = (Sample*) SDRAM.malloc(storageCapacity * sizeof(Sample));
    if (sampleStorage == nullptr) {
        Serial.println("[SampleCollector] ERROR: Failed to allocate sample storage!");
        return false;
    }
    
    Serial.println("[SampleCollector] Sample storage allocated successfully");
    
    // Reset state
    storageIndex = 0;
    totalSamplesStored = 0;
    gatheringActive = false;
    ringCount = 0;
    ringIndex = 0;
    
    return true;
}

void SampleCollector::update() {
    
    // Always consume samples from SharedRing (Core1 never stops sampling)
    size_t count = SharedRing_Consume(sampleBuffer, MAX_FETCH);
    if (count > 0 && gatheringActive) {
        processSamples(count);
    }

    // Show status occasionally
    static uint32_t debug_counter = 0;
    debug_counter++;
    if (debug_counter % 20000 == 0) {
        Serial.print("Overruns:");
        Serial.print(g_ring.overruns);
        Serial.print(" GATHERING:");
        Serial.print(SampleCollector::isGathering() ? "YES" : "NO");
        Serial.print(" STORED:");
        Serial.print(SampleCollector::getSamplesStored());
        Serial.print("/");
        Serial.println(SampleCollector::getStorageCapacity());
    }

}

void SampleCollector::processSamples(size_t count) {
    // GATHERING MODE: Store samples in storage buffer
    for (size_t i = 0; i < count; i++) {
        storeSample(sampleBuffer[i]);
        if (storageIndex >= storageCapacity) {
            // Storage full - update count before stopping
            totalSamplesStored = storageIndex;
            stopGathering();
            
            // TEMPORARY BEFORE RECEIVING COMMANDS
            sendAllSamples();
            break;
        }
    }
    totalSamplesStored = storageIndex;
}

void SampleCollector::storeSample(const Sample& sample) {
    if (storageIndex < storageCapacity) {
        sampleStorage[storageIndex] = sample;
        storageIndex++;
    }
}

void SampleCollector::startGathering() {
    Serial.println("[SampleCollector] Starting sample gathering");
    
    // Reset storage
    storageIndex = 0;
    totalSamplesStored = 0;
    
    // Enable gathering
    gatheringActive = true;
    
    Serial.print("[SampleCollector] Gathering started - storage capacity: ");
    Serial.print(storageCapacity);
    Serial.print(" samples (");
    Serial.print((float)storageCapacity / 10000.0, 1);  // Convert to seconds with 1 decimal
    Serial.println(" seconds)");
}

void SampleCollector::stopGathering() {
    Serial.print("[SampleCollector] Stopping sample gathering - collected ");
    Serial.print(totalSamplesStored);
    Serial.println(" samples");
    
    // Disable gathering but keep stored samples
    gatheringActive = false;
}

void SampleCollector::sendAllSamples() {
    if (totalSamplesStored == 0) {
        Serial.println("[SampleCollector] No samples to send");
        return;
    }
    
    Serial.print("[SampleCollector] Sending ");
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
            Serial.print("[SampleCollector] Sent ");
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
    
    Serial.print("[SampleCollector] Transmission complete: ");
    Serial.print(packetsSent);
    Serial.print(" packets, ");
    Serial.print(samplesSent);
    Serial.print(" samples in ");
    Serial.print(duration);
    Serial.println("ms");
    
    // Calculate transmission rate
    if (duration > 0) {
        Serial.print("[SampleCollector] Transmission rate: ");
        Serial.print((samplesSent * 1000) / duration);
        Serial.println(" samples/second");
    }
}

size_t SampleCollector::getSamplesStored() {
    return totalSamplesStored;
}

bool SampleCollector::isGathering() {
    return gatheringActive;
}

size_t SampleCollector::getStorageCapacity() {
    return storageCapacity;
}

void SampleCollector::printSampleDiagnostics(size_t count) {
    Serial.print("[SampleCollector] Samples collected: ");
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

        Serial.print("[SampleCollector] Average spacing (us) over last ");
        Serial.print(WINDOW);
        Serial.print(" samples = ");
        Serial.println(avgDelta);
    }
}
