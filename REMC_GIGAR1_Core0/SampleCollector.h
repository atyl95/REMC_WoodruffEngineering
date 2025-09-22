#ifndef SAMPLE_COLLECTOR_H
#define SAMPLE_COLLECTOR_H

#include <Arduino.h>
#include <stddef.h>
#include "SharedRing.h"

class SampleCollector {
public:
    // Initialize the sample collector (call once in setup)
    static bool init(size_t storageCapacity = 30000);
    
    // Main processing function (call in main loop)
    static void update();
    
    // Control functions
    static void startGathering();
    static void stopGathering();
    static void sendAllSamples();
    
    // Status functions
    static size_t getSamplesStored();
    static bool isGathering();
    static size_t getStorageCapacity();
    
    // Debug functions
    static void printSampleDiagnostics(size_t count);
    
private:
    // Storage system
    static Sample* sampleStorage;
    static volatile size_t storageCapacity;
    static volatile size_t storageIndex;
    static volatile bool gatheringActive;
    static volatile size_t totalSamplesStored;
    
    // Processing buffer
    static constexpr int MAX_FETCH = 1024;
    static Sample sampleBuffer[MAX_FETCH];
    
    // Rolling buffer for diagnostics
    static constexpr int WINDOW = 20;
    static Sample ringBuf[WINDOW];
    static int ringCount;
    static int ringIndex;
    
    // Helper functions
    static void processSamples(size_t count);
    static void storeSample(const Sample& sample);
};

#endif // SAMPLE_COLLECTOR_H
