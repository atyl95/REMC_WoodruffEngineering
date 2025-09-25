#pragma once
#include <Arduino.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <Dns.h>

class NTPClient {
public:
  // Singleton access
  static NTPClient& getInstance();
  
  // Static initialization method for setting UDP object
  static void initialize(EthernetUDP* udp = nullptr);
  
  // Static convenience methods for global access
  static uint64_t nowMicros();
  static bool hasSynced();
  static uint64_t lastSyncUnixUs();
  static uint64_t baseOffsetUs();
  static bool sync(uint16_t timeout_ms = 1000);
  static bool begin(const char* server, uint16_t ntpPort = 123);

  // You may pass a shared UDP instance if you want; otherwise default-construct one.
  explicit NTPClient(EthernetUDP* udp = nullptr);

  // Begin with server (IP string or hostname), remote NTP port (123 or 12300), and local UDP port to bind.
  // Returns true if UDP is ready and server was parsed/resolved.
  bool beginInstance(const char* server, uint16_t ntpPort = 123);

  // Force a sync: send an NTP request and set internal epoch on success.
  // timeout_ms: how long to wait for a reply (default 1000ms).
  // Returns true if a valid reply was received and time synced.
  bool syncInstance(uint16_t timeout_ms = 1000);

  // Returns Unix time in microseconds (UTC) based on last successful sync.
  // If never synced, returns 0.
  uint64_t nowMicrosInstance() const;

  // Returns the offset (us) between device's micros() clock and Unix epoch at the last sync.
  // I.e., epoch_us_at_sync - micros_at_sync.
  uint64_t baseOffsetUsInstance() const { return _epochUsAtSync - _microsAtSync; }

  // When was the last successful sync (Unix us)? 0 if never.
  uint64_t lastSyncUnixUsInstance() const { return _epochUsAtSync; }

  // Was there at least one successful sync?
  bool hasSyncedInstance() const { return _synced; }

  // Optionally change remote port (e.g., switch between 123 and 12300)
  void setServerPort(uint16_t port) { _serverPort = port; }

  // Optionally change server (string can be "x.y.z.w" or hostname)
  bool setServer(const char* server);

private:
  // Private constructor for singleton (use getInstance())
  // Note: public constructor still available for direct instantiation if needed
  
  bool resolveServerIP(const char* server);
  bool sendRequest();
  bool readResponse(uint32_t& secs, uint32_t& frac);

  static uint64_t ntpFracToMicros(uint32_t frac) {
    // Convert 32-bit NTP fractional seconds to microseconds: frac * 1e6 / 2^32
    return ((uint64_t)frac * 1000000ULL) >> 32;
  }

  // Singleton instance
  static NTPClient* _instance;

  EthernetUDP _ownedUdp;        // used if user didn't supply one
  EthernetUDP* _udp = nullptr;  // active UDP instance

  IPAddress _serverIP;
  uint16_t _serverPort = 123;
  uint16_t _localPort = 0;

  bool _serverResolved = false;

  // Sync anchors
  bool _synced = false;
  uint64_t _epochUsAtSync = 0;  // Unix epoch microseconds at the moment we captured micros()
  uint64_t _microsAtSync = 0;   // micros() snapshot at sync
  uint64_t _requestSentMicros = 0; // timestamp when NTP request was sent
};
