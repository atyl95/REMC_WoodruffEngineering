#include "UdpManager.h"
#include "TimerManager.h"  // For analog sensor data
#include "StateManager.h"  // For system status and commands
#include "PinConfig.h"     // For PIN_MSW_POS_A, PIN_MSW_POS_B
#include "Config.h"
#include <Arduino.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include "MD5.h"      // For schema hashing
#include <TimeLib.h>  // For timekeeping (needs external time source)


// --- Network Configuration ---
static EthernetUDP cmdUdp;  // Multicast listener for commands
static EthernetUDP udp;
static const IPAddress PC_MCAST = Config::TELEMETRY_IP;  // Destination IP for telemetry
static const uint16_t UDP_PORT = Config::TELEMETRY_PORT;

static const IPAddress CMD_MCAST = Config::COMMAND_MCAST_IP;
static const uint16_t CMD_PORT = Config::COMMAND_PORT;

// Neutrino header constants
static const uint32_t MSG_ID = 1;  // Atomic
static const uint32_t FLAGS = 0;
static const size_t FRAG_LEN = 16;
static const size_t HEADER_SIZE = 64;

// Telemetry schema
static const char* schema =
  "node_name REMC \n"
  "c telem_period 100000\n"  // 100µs (in nanoseconds)
  "v switch_voltage f32 u:kV\n"
  "v switch_current f32 u:kA\n"
  "v output_voltage_a f32 u:kV\n"
  "v output_voltage_b f32 u:kV\n"
  "v temperature_1 f32 u:degC\n"
  "v armed_status u8\n"
  "v em_status u8\n"
  "v msw_a_status u8\n"
  "v msw_b_status u8\n"
  "v manual_mode_status u8\n"
  "v hold_mode_status u8\n"

  "\n\n\n\n\n\n\n\n\n\n";  // Pad to multiple of 16 bytes

// Data sizes

// Batched telemetry definitions (per-sample micros timestamp supported)
static const uint8_t N_SAMPLES = (uint8_t)Config::TELEMETRY_BATCH_SAMPLES;
static const size_t DATA_SIZE_MICROS = (5 * sizeof(float)) + sizeof(uint32_t) + 6 * sizeof(uint8_t);  // 30 bytes
static const size_t PACKET_SIZE_BATCH = HEADER_SIZE + (DATA_SIZE_MICROS * N_SAMPLES);

struct TelemetrySample {
  float sv, sc, ova, ovb, tm1;
  uint32_t us;  // micros() at capture time
  uint8_t ready, em, a, b, manual, hold;
};


namespace {
uint8_t schemaHash[16];
uint32_t schemaNumFrags = 0;
uint32_t currentSchemaFrag = 0;

static TelemetrySample s_buf[Config::TELEMETRY_BATCH_SAMPLES];
static volatile uint8_t s_wr_idx = 0;    // where ISR writes next sample
static volatile uint8_t s_tick_div = 0;  // count 10k ticks → 0..N-1
static volatile bool packet_ready = false;

void calcSchemaHash() {
  char* buf = (char*)malloc(strlen(schema) + 1);
  if (buf != NULL) {
    strcpy(buf, schema);
    unsigned char* digest = MD5::make_hash(buf);
    free(buf);
    if (digest) {
      memcpy(schemaHash, digest, 16);
      free(digest);
    } else {
      Serial.println(F("[UDP][ERR] Schema hash error"));
      memset(schemaHash, 0, 16);
    }
  } else {
    Serial.println(F("[UDP][ERR] Memory allocation error"));
    memset(schemaHash, 0, 16);
  }
  schemaNumFrags = (strlen(schema) + FRAG_LEN - 1) / FRAG_LEN;
  currentSchemaFrag = 0;
}

uint32_t htonl_custom(uint32_t h) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return __builtin_bswap32(h);
#else
  return h;
#endif
}

uint64_t htobe64_custom(uint64_t v) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return ((uint64_t)htonl_custom(v & 0xffffffff) << 32) | htonl_custom(v >> 32);
#else
  return v;
#endif
}


uint64_t getUnixTimeNanos() {
  // Get integer seconds since epoch
  time_t epochSecs = now();
  // Get microseconds since boot; take only the fractional part within the current second
  unsigned long us = micros() % 1000000UL;
  // Combine into nanoseconds
  return (uint64_t)epochSecs * 1000000000ULL + (uint64_t)us * 1000ULL;
}

}

namespace UdpManager {

void init() {
  Serial.println(F("UdpManager: Ethernet.begin..."));
  Ethernet.begin((byte*)Config::MAC_ADDRESS,
                 Config::LOCAL_IP,
                 Config::GATEWAY_IP,
                 Config::GATEWAY_IP,
                 Config::SUBNET_MASK);

  Serial.print(F("UdpManager: Binding UDP on port "));
  Serial.println(UDP_PORT);
  if (udp.begin(UDP_PORT) != 1) {
    Serial.println(F("UdpManager: UDP bind failed"));
  }

  // --- JOIN MULTICAST GROUP FOR TELEMETRY ---
  Serial.println(F("Joining telemtry multicast "));
  if (udp.beginMulticast(PC_MCAST, UDP_PORT) != 1) {
    Serial.println(F("UdpManager: Telemetry multicast join failed"));
  } else {
    Serial.print(F("UdpManager: Joined telemetry multicast "));
    Serial.print(PC_MCAST);
    Serial.print(F(":"));
    Serial.println(UDP_PORT);
  }

  // --- JOIN MULTICAST GROUP FOR COMMAND ---
  Serial.println(F("Joining command multicast "));
  if (cmdUdp.beginMulticast(CMD_MCAST, CMD_PORT) != 1) {
    Serial.println(F("UdpManager: Command multicast join failed"));
  } else {
    Serial.print(F("UdpManager: Joined command multicast "));
    Serial.print(CMD_MCAST);
    Serial.print(F(":"));
    Serial.println(CMD_PORT);
  }

  calcSchemaHash();

  // Initialize time (replace with actual time sync)
  // This is a placeholder!  You'll need a proper time synchronization mechanism.
  setTime(0, 0, 0, 1, 1, 2024);  // Dummy time - replace with actual time sync
  Serial.println(F("[UDP] init complete."));
}

void onSampleTick(uint32_t irq_us) {
  // Snapshot the latest scaled values & states into ring
  TelemetrySample& s = s_buf[s_wr_idx];
  s.sv = TimerManager::getSwitchVoltageKV();
  s.sc = TimerManager::getSwitchCurrentA();
  s.ova = TimerManager::getOutputVoltageAKV();
  s.ovb = TimerManager::getOutputVoltageBKV();
  s.tm1 = TimerManager::getTemp1DegC();
  s.us = irq_us;   // use ISR-captured timestamp

  s.ready = StateManager::isReady() ? 1 : 0;
  s.em = StateManager::isEmActActive() ? 1 : 0;
  s.a = (digitalRead(PIN_MSW_POS_A) == LOW) ? 0 : 1;
  s.b = (digitalRead(PIN_MSW_POS_B) == LOW) ? 0 : 1;
  s.manual = StateManager::isManualModeActive() ? 1 : 0;
  s.hold = StateManager::isHoldAfterFireModeActive() ? 1 : 0;

  // advance index and mark ready every N samples
  if (++s_wr_idx >= N_SAMPLES) s_wr_idx = 0;
  // every N ticks, tell the main loop to send one packet
  if (++s_tick_div >= N_SAMPLES) {
    s_tick_div = 0;
    packet_ready = true;
  }
}

void processIncoming() {
  int size = cmdUdp.parsePacket();
  if (size > 0) {
    const int BUF_SIZE = 128;
    uint8_t buf[BUF_SIZE];
    int len = cmdUdp.read(buf, min(size, BUF_SIZE));
    if (len > 64) {
      uint8_t cmd = buf[64];
      switch (cmd) {
        case 0x01: StateManager::requestArm(); break;
        case 0x02: StateManager::triggerSoftwareActuate(); break;
        case 0x03: StateManager::requestDisarm(); break;
        case 0x11: StateManager::manualActuatorControl(ACT_FWD); break;
        case 0x12: StateManager::manualActuatorControl(ACT_STOP); break;
        case 0x13: StateManager::manualActuatorControl(ACT_BWD); break;
        case 0x15: StateManager::manualEMEnable(); break;
        case 0x16: StateManager::manualEMDisable(); break;
        case 0x1F: StateManager::enableManualMode(); break;
        case 0x1E: StateManager::disableManualMode(); break;
        case 0x20: StateManager::enableHoldAfterFireMode(); break;
        case 0x21: StateManager::disableHoldAfterFireMode(); break;
        default: break;
      }
    }
  }
}

static void processIncomingOccasionally() {
  static uint32_t last = 0;
  uint32_t nowu = micros();
  if ((uint32_t)(nowu - last) < 2000) return;  // ~500 Hz
  last = nowu;
  processIncoming();
}

void update() {
  if (!packet_ready) return;
  packet_ready = false;
  processIncomingOccasionally();  //cheaper than 10 kHz polling
  sendNeutrinoPacket();           // now sends N samples
}

void sendNeutrinoPacket() {
  // Build one UDP datagram that contains N_SAMPLES consecutive samples
  uint8_t packet[PACKET_SIZE_BATCH];
  uint32_t* h = reinterpret_cast<uint32_t*>(packet);
  h[0] = htonl_custom(MSG_ID);
  h[1] = htonl_custom(FLAGS);
  h[2] = htonl_custom(schemaNumFrags);
  h[3] = htonl_custom(1);  // NUM_ATOMIC_FRAGS
  memcpy(packet + 16, schemaHash, 16);

  // Schema fragment cycles each packet
  memset(packet + 32, 0, FRAG_LEN);
  size_t schemaLen = strlen(schema);
  size_t offset = currentSchemaFrag * FRAG_LEN;
  if (offset < schemaLen) {
    size_t copyLen = min(FRAG_LEN, schemaLen - offset);
    memcpy(packet + 32, schema + offset, copyLen);
  }
  h[12] = htonl_custom(currentSchemaFrag);
  h[13] = htonl_custom(0);  // ATOMIC_IDX
  currentSchemaFrag = (currentSchemaFrag + 1) % schemaNumFrags;

  uint64_t t = htobe64_custom(getUnixTimeNanos());
  memcpy(packet + 56, &t, sizeof(uint64_t));

  // Payload (batched): copy N_SAMPLES from ring buffer, oldest first
  uint8_t* d = packet + HEADER_SIZE;

  /*
  Take a short critical section while copying the ring.
  Keep it as tight as possible: only the read/copy of s_buf.
  Re-enable interrupts BEFORE any UDP/W5500 calls.
  */
  noInterrupts();

  // Determine the oldest sample (write index points to next slot)
  uint8_t idx = s_wr_idx;  // oldest sample (write index points to next slot)
  for (uint8_t i = 0; i < N_SAMPLES; ++i) {
    const TelemetrySample& s = s_buf[idx];

    memcpy(d, &s.sv, sizeof(s.sv));
    d += sizeof(s.sv);
    memcpy(d, &s.sc, sizeof(s.sc));
    d += sizeof(s.sc);
    memcpy(d, &s.ova, sizeof(s.ova));
    d += sizeof(s.ova);
    memcpy(d, &s.ovb, sizeof(s.ovb));
    d += sizeof(s.ovb);
    memcpy(d, &s.tm1, sizeof(s.tm1));
    d += sizeof(s.tm1);
    memcpy(d, &s.us, sizeof(s.us));
    d += sizeof(s.us);  // per-sample micros()

    *d++ = s.ready;
    *d++ = s.em;
    *d++ = s.a;
    *d++ = s.b;
    *d++ = s.manual;
    *d++ = s.hold;

    // advance ring index
    if (++idx >= N_SAMPLES) idx = 0;
  }

  interrupts();

//  Send Packet
  if (udp.beginPacket(PC_MCAST, UDP_PORT) != 1) {
    Serial.println(F("UdpManager: Failed send, aborting"));
    return;
  }

  if (udp.write(packet, PACKET_SIZE_BATCH) == 0 ) {
    Serial.println(F("UdpManager: Failed write, aborting"));
    return;
  }

  udp.endPacket();
}

}  // namespace UdpManager