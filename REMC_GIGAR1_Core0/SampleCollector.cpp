#include "SampleCollector.h"
#include "UdpManager.h"
#include "SDRAM.h"

// Static member definitions
Sample* SampleCollector::ringBuffer = nullptr;
volatile size_t SampleCollector::ringCapacity = 0;
volatile size_t SampleCollector::ringHead = 0;
volatile size_t SampleCollector::totalSamplesReceived = 0;

volatile bool SampleCollector::gatheringActive = false;
volatile int SampleCollector::gatheringStart = 0;
volatile int SampleCollector::gatheringStop = 0;
volatile size_t SampleCollector::samplesNeeded = 0;
volatile size_t SampleCollector::samplesCollected = 0;
volatile size_t SampleCollector::gatheringStartSampleCount = 0;

// Window storage variables
volatile int SampleCollector::windowStart = -50000;
volatile int SampleCollector::windowStop = 50000;

Sample SampleCollector::sampleBuffer[MAX_FETCH];
Sample SampleCollector::ringBuf[WINDOW];
int SampleCollector::ringCount = 0;
int SampleCollector::ringIndex = 0;

bool SampleCollector::init(size_t capacity) {
    ringCapacity = capacity;
    
    // Initialize SharedRing buffer
    Serial.print("[SampleCollector] SharedRing Address ");
    Serial.println((uintptr_t)&g_ring, HEX);
    SharedRing_Init();
    
    // Allocate ring buffer storage in external SDRAM
    Serial.print("[SampleCollector] Allocating ring buffer for ");
    Serial.print(ringCapacity);
    Serial.print(" samples (");
    Serial.print((ringCapacity * sizeof(Sample)) / (1024.0*1024.0), 2);
    Serial.println(" MB)");
    
    ringBuffer = (Sample*) SDRAM.malloc(ringCapacity * sizeof(Sample));
    if (ringBuffer == nullptr) {
        Serial.println("[SampleCollector] ERROR: Failed to allocate ring buffer storage!");
        return false;
    }
    
    Serial.println("[SampleCollector] Ring buffer storage allocated successfully");
    
    // Reset state
    ringHead = 0;
    totalSamplesReceived = 0;
    gatheringActive = false;
    gatheringStart = 0;
    gatheringStop = 0;
    samplesNeeded = 0;
    samplesCollected = 0;
    gatheringStartSampleCount = 0;
    ringCount = 0;
    ringIndex = 0;
    
    return true;
}

void SampleCollector::update() {
    
    // Always consume samples from SharedRing (Core1 never stops sampling)
    size_t count = SharedRing_Consume(sampleBuffer, MAX_FETCH);
    if (count > 0) {

        for (size_t i = 0; i < count; i++) {
            storeSampleInRing(sampleBuffer[i]);
        }
        
        // If gathering is active, check if we can send samples now
        if (gatheringActive && canSendNow()) {
            extractRequestedSamples();
        }
    }
}

void SampleCollector::storeSampleInRing(const Sample& sample) {
    // Store sample in ring buffer at current head position
    ringBuffer[ringHead] = sample;
    
    // Advance head pointer (wrap around at capacity)
    ringHead = (ringHead + 1) % ringCapacity;
    
    // Increment total samples received
    totalSamplesReceived++;
}

void SampleCollector::startGathering(int start, int stop) {
    Serial.print("[SampleCollector] Starting sample gathering - start: ");
    Serial.print(start);
    Serial.print(", stop: ");
    Serial.println(stop);
    
    // Validate parameters
    if (stop <= start) {
        Serial.println("[SampleCollector] ERROR: stop must be greater than start");
        return;
    }
    
    // Store gathering parameters
    gatheringStart = start;
    gatheringStop = stop;
    samplesNeeded = stop - start;
    samplesCollected = 0;
    gatheringStartSampleCount = totalSamplesReceived;
    
    // Enable gathering
    gatheringActive = true;
    
    Serial.print("[SampleCollector] Gathering configured for ");
    Serial.print(samplesNeeded);
    Serial.print(" samples (");
    Serial.print((float)samplesNeeded / 10000.0, 1);  // Convert to seconds with 1 decimal
    Serial.println(" seconds)");
    
    // If start is negative and we have enough samples in ring buffer, we can potentially send immediately
    if (start < 0 && totalSamplesReceived >= (size_t)(-start)) {
        Serial.println("[SampleCollector] Historical samples available");
    } else if (start < 0) {
        Serial.print("[SampleCollector] Need ");
        Serial.print((-start) - totalSamplesReceived);
        Serial.println(" more historical samples");
    }
}

void SampleCollector::setWindow(int start, int stop) {
    Serial.print("[SampleCollector] Setting window - start: ");
    Serial.print(start);
    Serial.print(", stop: ");
    Serial.println(stop);
    
    // Validate parameters
    if (stop <= start) {
        Serial.println("[SampleCollector] ERROR: stop must be greater than start");
        return;
    }
    
    windowStart = start;
    windowStop = stop;
    
    Serial.println("[SampleCollector] Window configuration updated");
}

void SampleCollector::startGathering() {
    // Use stored window parameters
    startGathering(windowStart, windowStop);
}

void SampleCollector::stopGathering() {
    Serial.print("[SampleCollector] Stopping sample gathering - collected ");
    Serial.print(samplesCollected);
    Serial.print("/");
    Serial.print(samplesNeeded);
    Serial.println(" samples");
    
    // Disable gathering
    gatheringActive = false;
}

void SampleCollector::sendAllSamples() {
    if (!gatheringActive) {
        Serial.println("[SampleCollector] No active gathering to send");
        return;
    }
    
    Serial.println("[SampleCollector] Force sending all samples from current gathering...");
    
    // Force extract samples even if not all future samples are ready
    extractRequestedSamples();
}

size_t SampleCollector::getSamplesStored() {
    return samplesCollected;
}

bool SampleCollector::isGathering() {
    return gatheringActive;
}

size_t SampleCollector::getStorageCapacity() {
    return ringCapacity;
}

bool SampleCollector::canSendNow() {
    // If stop is <= 0, all samples are historical, we can send immediately
    if (gatheringStop <= 0) {
        // Check if we have enough historical samples
        if (gatheringStart < 0) {
            size_t historicalNeeded = (size_t)(-gatheringStart);
            size_t availableHistorical = min(totalSamplesReceived, ringCapacity);
            return availableHistorical >= historicalNeeded;
        }
        return true; // All samples from 0 to stop are historical
    }
    
    // If stop > 0, we need future samples
    // Check if we've received enough samples since gathering started
    size_t samplesSinceStart = totalSamplesReceived - gatheringStartSampleCount;
    return samplesSinceStart >= (size_t)gatheringStop;
}

size_t SampleCollector::getRingIndex(int relativeIndex, size_t referenceSampleCount) {
    // Convert relative index to absolute ring buffer index
    // relativeIndex is relative to the reference point (when gathering started)
    // referenceSampleCount is the total samples received at reference point
    
    if (totalSamplesReceived == 0) {
        return 0; // No samples yet
    }
    
    // Calculate absolute sample number
    size_t absoluteSampleNumber = referenceSampleCount + relativeIndex;
    
    // If this sample hasn't been received yet, return invalid index
    if (absoluteSampleNumber >= totalSamplesReceived) {
        return ringCapacity; // Invalid index marker
    }
    
    // If this sample is too old (overwritten), return invalid index
    size_t oldestAvailable = totalSamplesReceived >= ringCapacity ? 
                            totalSamplesReceived - ringCapacity : 0;
    if (absoluteSampleNumber < oldestAvailable) {
        return ringCapacity; // Invalid index marker
    }
    
    // Convert to ring buffer index
    return absoluteSampleNumber % ringCapacity;
}

void SampleCollector::extractRequestedSamples() {
    Serial.println("[SampleCollector] Extracting requested samples...");
    
    // Tag all outgoing samples as collected samples
    UdpManager::startSendingCollectedSamples();
    
    // Reset samples collected counter
    samplesCollected = 0;
    
    // Extract samples from start to stop relative to when gathering started
    for (int i = gatheringStart; i < gatheringStop; i++) {
        size_t ringIndex = getRingIndex(i, gatheringStartSampleCount);
        
        // Check if this sample is valid
        if (ringIndex >= ringCapacity) {
            if (i < 0) {
                Serial.print("[SampleCollector] WARNING: Historical sample at index ");
                Serial.print(i);
                Serial.println(" is too old and has been overwritten");
            } else {
                Serial.print("[SampleCollector] WARNING: Future sample at index ");
                Serial.print(i);
                Serial.println(" not yet available");
                break; // Stop processing if we hit unavailable future samples
            }
            continue;
        }
        
        // Add sample to UDP manager for transmission
        UdpManager::addSample(ringBuffer[ringIndex]);
        samplesCollected++;
        
        // Send in batches to avoid overwhelming UDP stack
        if (samplesCollected % 46 == 0) {
            UdpManager::flushSamples();
        }
    }
    
    // Flush any remaining samples
    if (samplesCollected % 46 != 0) {
        UdpManager::flushSamples();
    }
    
    // Stop tagging samples as collected and send end marker
    UdpManager::stopSendingCollectedSamples();
    UdpManager::sendBatchEndMarker();
    
    Serial.print("[SampleCollector] Extracted and sent ");
    Serial.print(samplesCollected);
    Serial.print("/");
    Serial.print(samplesNeeded);
    Serial.println(" samples");
    
    // Stop gathering since we've sent the requested samples
    stopGathering();
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
