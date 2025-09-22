#include "TimeMapper.h"

// Initialize singleton instance pointer
TimeMapper* TimeMapper::_instance = nullptr;

TimeMapper& TimeMapper::getInstance() {
    if (_instance == nullptr) {
        _instance = new TimeMapper();
    }
    return *_instance;
}

// Static convenience methods
uint64_t TimeMapper::hardwareToNTP(uint64_t hardwareMicros) {
    return getInstance().hardwareToNTPInstance(hardwareMicros);
}

uint64_t TimeMapper::ntpToHardware(uint64_t ntpMicros) {
    return getInstance().ntpToHardwareInstance(ntpMicros);
}

bool TimeMapper::isReady() {
    return getInstance().isReadyInstance();
}

bool TimeMapper::syncNTP(uint16_t timeout_ms) {
    return getInstance().syncNTPInstance(timeout_ms);
}

void TimeMapper::update() {
    getInstance().updateInstance();
}

uint64_t TimeMapper::sampleToNTP(uint32_t t_us, uint32_t rollover_count) {
    // Compose 64-bit hardware timestamp from Sample fields
    uint64_t hardwareTime = ((uint64_t)rollover_count << 32) | t_us;
    return hardwareToNTP(hardwareTime);
}

void TimeMapper::ntpToSample(uint64_t ntpMicros, uint32_t& t_us, uint32_t& rollover_count) {
    // Convert NTP time to hardware time
    uint64_t hardwareTime = ntpToHardware(ntpMicros);
    
    // Split into Sample fields
    t_us = (uint32_t)(hardwareTime & 0xFFFFFFFFULL);
    rollover_count = (uint32_t)(hardwareTime >> 32);
}

bool TimeMapper::begin() {
    if (_initialized) {
        return true;
    }
    
    Serial.println("[TimeMapper] Initializing...");
    
    // Check if HardwareTimer is ready
    if (!HardwareTimer::isInitialized()) {
        Serial.println("[TimeMapper] ERROR: HardwareTimer not initialized");
        return false;
    }
    
    // Check if NTP client is available
    if (!NTPClient::hasSynced()) {
        Serial.println("[TimeMapper] WARNING: NTP not yet synced, will sync on first update");
    }
    
    _initialized = true;
    _lastAutoSyncMillis = millis();
    
    // Try initial sync if NTP is available
    if (NTPClient::hasSynced()) {
        updateMapping();
        Serial.println("[TimeMapper] Initialized with existing NTP sync");
    } else {
        Serial.println("[TimeMapper] Initialized, waiting for NTP sync");
    }
    
    return true;
}

bool TimeMapper::syncNTPInstance(uint16_t timeout_ms) {
    if (!_initialized) {
        Serial.println("[TimeMapper] ERROR: Not initialized");
        return false;
    }
    
    Serial.print("[TimeMapper] Syncing NTP with timeout: ");
    Serial.print(timeout_ms);
    Serial.println("ms");
    
    bool success = NTPClient::sync(timeout_ms);
    if (success) {
        updateMapping();
        _syncCount++;
        _lastAutoSyncMillis = millis();
        Serial.println("[TimeMapper] NTP sync successful, mapping updated");
    } else {
        Serial.println("[TimeMapper] NTP sync failed");
    }
    
    return success;
}

void TimeMapper::updateMapping() {
    if (!NTPClient::hasSynced()) {
        Serial.println("[TimeMapper] WARNING: Cannot update mapping - NTP not synced");
        return;
    }
    
    // Capture both timestamps as close together as possible
    // Order matters: get hardware time first since it's faster/more deterministic
    _hardwareTimeAtSync = HardwareTimer::getMicros64();
    _ntpTimeAtSync = NTPClient::nowMicros();
    _lastSyncNTPTime = _ntpTimeAtSync;
    _lastSyncMillis = millis();
    
    _hasMappingData = true;
    
    Serial.print("[TimeMapper] Mapping updated - HW: ");
    Serial.print((unsigned long)(_hardwareTimeAtSync / 1000000ULL));
    Serial.print(".");
    Serial.print((unsigned long)(_hardwareTimeAtSync % 1000000ULL));
    Serial.print("s, NTP: ");
    Serial.print((unsigned long)(_ntpTimeAtSync / 1000000ULL));
    Serial.print(".");
    Serial.print((unsigned long)(_ntpTimeAtSync % 1000000ULL));
    Serial.println("s");
}

uint64_t TimeMapper::hardwareToNTPInstance(uint64_t hardwareMicros) const {
    if (!_hasMappingData) {
        Serial.println("[TimeMapper] WARNING: No mapping data available");
        return 0;
    }
    
    // Calculate the time difference in hardware time
    int64_t hardwareDelta = (int64_t)(hardwareMicros - _hardwareTimeAtSync);
    
    // Apply the same delta to NTP time
    // This assumes the clocks run at the same rate (which they should over short periods)
    uint64_t ntpTime = _ntpTimeAtSync + hardwareDelta;
    
    return ntpTime;
}

uint64_t TimeMapper::ntpToHardwareInstance(uint64_t ntpMicros) const {
    if (!_hasMappingData) {
        Serial.println("[TimeMapper] WARNING: No mapping data available");
        return 0;
    }
    
    // Calculate the time difference in NTP time
    int64_t ntpDelta = (int64_t)(ntpMicros - _ntpTimeAtSync);
    
    // Apply the same delta to hardware time
    uint64_t hardwareTime = _hardwareTimeAtSync + ntpDelta;
    
    return hardwareTime;
}

bool TimeMapper::isReadyInstance() const {
    return _initialized && _hasMappingData;
}

void TimeMapper::updateInstance() {
    if (!_initialized) {
        return;
    }
    
    // Check if we need to auto-sync
    uint32_t currentMillis = millis();
    if (currentMillis - _lastAutoSyncMillis >= AUTO_SYNC_INTERVAL_MS) {
        Serial.println("[TimeMapper] Auto-sync triggered");
        syncNTPInstance();
    }
}

uint64_t TimeMapper::getTimeSinceLastSync() const {
    if (!_hasMappingData) {
        return 0;
    }
    
    uint32_t currentMillis = millis();
    return (uint64_t)(currentMillis - _lastSyncMillis) * 1000ULL; // Convert to microseconds
}
