#include "NTPClient.h"

static const uint32_t NTP_UNIX_EPOCH_DIFF = 2208988800UL; // seconds between 1900 and 1970
static const int NTP_PACKET_SIZE = 48;

// Initialize singleton instance pointer
NTPClient* NTPClient::_instance = nullptr;

NTPClient::NTPClient(EthernetUDP* udp) {
  if (udp) {
    _udp = udp;
  } else {
    _udp = &_ownedUdp;
  }
}

// Singleton implementation
NTPClient& NTPClient::getInstance() {
  if (_instance == nullptr) {
    _instance = new NTPClient();
  }
  return *_instance;
}

// Static initialization method
void NTPClient::initialize(EthernetUDP* udp) {
  NTPClient& instance = getInstance();
  if (udp) {
    instance._udp = udp;
  }
}

// Static wrapper methods
uint64_t NTPClient::nowMicros() {
  return getInstance().nowMicrosInstance();
}

bool NTPClient::hasSynced() {
  return getInstance().hasSyncedInstance();
}

uint64_t NTPClient::lastSyncUnixUs() {
  return getInstance().lastSyncUnixUsInstance();
}

uint64_t NTPClient::baseOffsetUs() {
  return getInstance().baseOffsetUsInstance();
}

bool NTPClient::sync(uint16_t timeout_ms) {
  return getInstance().syncInstance(timeout_ms);
}

bool NTPClient::begin(const char* server, uint16_t ntpPort) {
  return getInstance().beginInstance(server, ntpPort);
}

bool NTPClient::beginInstance(const char* server, uint16_t ntpPort) {
  _serverPort = ntpPort;
  
  Serial.print("[NTP] Resolving server: ");
  Serial.println(server);
  _serverResolved = resolveServerIP(server);
  
  if (_serverResolved) {
    Serial.print("[NTP] Server resolved to: ");
    Serial.println(_serverIP);
  } else {
    Serial.println("[NTP] ERROR: Failed to resolve server address");
  }
  
  return _serverResolved;
}

bool NTPClient::setServer(const char* server) {
  _serverResolved = resolveServerIP(server);
  return _serverResolved;
}

bool NTPClient::resolveServerIP(const char* server) {
  // Try dotted-quad first
  IPAddress ip;
  if (ip.fromString(server)) {
    Serial.println("[NTP] Server is IP address - no DNS resolution needed");
    _serverIP = ip;
    return true;
  }
  
  Serial.println("[NTP] Server is hostname - attempting DNS resolution");
  
  // Fallback to DNS for hostnames
  DNSClient dns;
  IPAddress dnsServer = Ethernet.dnsServerIP();
  Serial.print("[NTP] DNS server from DHCP: ");
  Serial.println(dnsServer);
  
  if (dnsServer == IPAddress(0,0,0,0)) {
    // Try gateway as DNS if none configured
    dnsServer = Ethernet.gatewayIP();
    Serial.print("[NTP] Using gateway as DNS server: ");
    Serial.println(dnsServer);
  }
  
  if (dnsServer == IPAddress(0,0,0,0)) {
    Serial.println("[NTP] ERROR: No DNS server available");
    return false;
  }
  
  dns.begin(dnsServer);
  Serial.print("[NTP] Attempting DNS lookup for: ");
  Serial.println(server);

  int res = dns.getHostByName(server, _serverIP);
  if (res == 1) {
    Serial.print("[NTP] DNS resolution successful: ");
    Serial.println(_serverIP);
  } else {
    Serial.print("[NTP] DNS resolution failed with code: ");
    Serial.println(res);
  }
  
  return (res == 1);
}

bool NTPClient::sendRequest() {
  if (!_serverResolved) {
    Serial.println("[NTP] ERROR: Cannot send request - server not resolved");
    return false;
  }

  Serial.print("[NTP] Sending NTP request to ");
  Serial.print(_serverIP);
  Serial.print(":");
  Serial.println(_serverPort);

  uint8_t packet[NTP_PACKET_SIZE];
  memset(packet, 0, sizeof(packet));

  // LI = 0 (no warning), VN = 4, Mode = 3 (client)
  packet[0] = 0x23; // 0b0010_0011

  // Transmit Timestamp can be left 0; server will fill its own transmit time.

  if (_udp->beginPacket(_serverIP, _serverPort) != 1) {
    Serial.println("[NTP] ERROR: Failed to begin UDP packet");
    return false;
  }
  
  size_t written = _udp->write(packet, sizeof(packet));
  if (written != sizeof(packet)) {
    Serial.print("[NTP] ERROR: Incomplete packet write - wrote ");
    Serial.print(written);
    Serial.print(" of ");
    Serial.println(sizeof(packet));
    return false;
  }
  
  if (_udp->endPacket() != 1) {
    Serial.println("[NTP] ERROR: Failed to send UDP packet");
    return false;
  }
  
  Serial.println("[NTP] NTP request sent successfully");
  return true;
}

bool NTPClient::readResponse(uint32_t& secs, uint32_t& frac) {
  int size = _udp->parsePacket();
  if (size == 0) return false;  // No packet available
  
  if (size < NTP_PACKET_SIZE) {
    Serial.print("[NTP] WARNING: Received packet too small: ");
    Serial.print(size);
    Serial.print(" bytes (expected ");
    Serial.print(NTP_PACKET_SIZE);
    Serial.println(")");
    return false;
  }

  Serial.print("[NTP] Received NTP response (");
  Serial.print(size);
  Serial.println(" bytes)");

  uint8_t buf[NTP_PACKET_SIZE];
  int bytesRead = _udp->read(buf, NTP_PACKET_SIZE);
  
  if (bytesRead != NTP_PACKET_SIZE) {
    Serial.print("[NTP] ERROR: Read incomplete - got ");
    Serial.print(bytesRead);
    Serial.print(" of ");
    Serial.println(NTP_PACKET_SIZE);
    return false;
  }

  // Check if it's a valid NTP response
  uint8_t mode = buf[0] & 0x07;
  if (mode != 4) {  // Server mode
    Serial.print("[NTP] ERROR: Invalid mode in response: ");
    Serial.println(mode);
    return false;
  }

  // Transmit Timestamp starts at byte 40 (big-endian): seconds (4), fraction (4)
  secs = ((uint32_t)buf[40] << 24) |
         ((uint32_t)buf[41] << 16) |
         ((uint32_t)buf[42] << 8)  |
         ((uint32_t)buf[43]);

  frac = ((uint32_t)buf[44] << 24) |
         ((uint32_t)buf[45] << 16) |
         ((uint32_t)buf[46] << 8)  |
         ((uint32_t)buf[47]);

  Serial.print("[NTP] Transmit timestamp: ");
  Serial.print(secs);
  Serial.print(".");
  Serial.println(frac);

  // Basic sanity: NTP time should be after Jan 1, 2000 (946684800 Unix)
  uint32_t unixSecs = secs - NTP_UNIX_EPOCH_DIFF;
  if (unixSecs < 946684800UL) {
    Serial.print("[NTP] ERROR: Timestamp sanity check failed - Unix seconds: ");
    Serial.println(unixSecs);
    return false;
  }

  Serial.println("[NTP] Response validation successful");
  return true;
}

bool NTPClient::syncInstance(uint16_t timeout_ms) {
  if (!_serverResolved) {
    Serial.println("[NTP] ERROR: Cannot sync - server not resolved");
    return false;
  }

  Serial.print("[NTP] Starting sync with timeout: ");
  Serial.print(timeout_ms);
  Serial.println("ms");

  // Flush any stale packets
  int flushed = 0;
  while (_udp->parsePacket() > 0) {
    uint8_t dump[64];
    _udp->read(dump, sizeof(dump));
    flushed++;
  }
  if (flushed > 0) {
    Serial.print("[NTP] Flushed ");
    Serial.print(flushed);
    Serial.println(" stale packets");
  }

  if (!sendRequest()) {
    Serial.println("[NTP] ERROR: Failed to send request");
    return false;
  }

  Serial.println("[NTP] Waiting for response...");
  uint32_t start = millis();
  uint32_t lastCheck = start;
  
  while ((uint16_t)(millis() - start) < timeout_ms) {
    uint32_t secs = 0, frac = 0;
    if (readResponse(secs, frac)) {
      // Capture local clock as close as possible to packet read
      uint32_t localMicros = micros();

      uint64_t unixSecs = (uint64_t)(secs - NTP_UNIX_EPOCH_DIFF);
      uint64_t unixFracUs = ntpFracToMicros(frac);
      uint64_t unixUs = unixSecs * 1000000ULL + unixFracUs;

      _epochUsAtSync = unixUs;    // this corresponds roughly to localMicros moment
      _microsAtSync = localMicros;
      _synced = true;
      
      Serial.println("[NTP] Sync successful!");
      Serial.print("[NTP] NTP timestamp: ");
      Serial.print(secs);
      Serial.print(".");
      Serial.println(frac);

      Serial.print("[NTP] Unix time (s): ");
      Serial.println((unsigned long)(unixUs / 1000000ULL));
      
      Serial.print("[NTP] Unix time (us): ");
      Serial.print((unsigned long)(unixUs / 1000000ULL));
      Serial.print("_");
      Serial.println((unsigned long)(unixUs % 1000000ULL));

      char buf[32];
      sprintf(buf, "%llu", (unsigned long long)unixUs);
      Serial.println(buf);

      return true;
    }
    
    // Print progress every 1000ms
    if (millis() - lastCheck >= 1000) {
      Serial.print("[NTP] Still waiting... (");
      Serial.print(millis() - start);
      Serial.println("ms elapsed)");
      lastCheck = millis();
    }
    
    // brief yield; small delay helps some MCUs
    delay(10);  // Slightly longer delay for better responsiveness
  }
  
  Serial.print("[NTP] Sync timeout after ");
  Serial.print(timeout_ms);
  Serial.println("ms");
  return false;
}

uint64_t NTPClient::nowMicrosInstance() const {
  if (!_synced) return 0ULL;

  // unsigned arithmetic handles wrap of micros()
  uint32_t nowLocal = micros();
  uint32_t elapsed = nowLocal - _microsAtSync; // wraps safely
  return _epochUsAtSync + (uint64_t)elapsed;
}
