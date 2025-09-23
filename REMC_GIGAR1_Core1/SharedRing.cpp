#include "SharedRing.h"
#include "Logger.h"
#include <Arduino.h>

// ===================== IMPORTANT =====================
// On STM32H747 (Arduino GIGA), the ONLY general-purpose RAM
// visible to BOTH cores is D3 SRAM4 at 0x3800_0000 (64 KB).
//
// We position the SharedRing at the *top* of SRAM4 because the
// Arduino RPC/OpenAMP transport uses the *bottom* of SRAM4 for its
// shared vrings and control blocks. Placing our buffer at the top
// (SRAM4_END - sizeof(SharedRing)) avoids overlapping those fixed
// OpenAMP structures.
// =====================================================

constexpr uintptr_t SRAM4_END = 0x38010000UL;   // end+1 of 64 KB
constexpr size_t    RING_BYTES = (sizeof(SharedRing) + 31u) & ~size_t(31);
constexpr uintptr_t RING_ADDR  = SRAM4_END - RING_BYTES;

SharedRing& g_ring = *reinterpret_cast<SharedRing*>(RING_ADDR);

static inline void ring_dmb() {
  __DMB();  // Data Memory Barrier
}

// // ---------------- Producer (Core1 / CM4) ----------------
void SharedRing_Init() {
  g_ring.capacity  = SHARED_RING_CAPACITY;
  g_ring.head      = 0;
  g_ring.tail      = 0;
  g_ring.overruns  = 0;
}

// ---------------- Producer ----------------
void SharedRing_Add(const REMCSample& sample) {
  const uint32_t cap     = g_ring.capacity;
  const uint32_t mask    = cap - 1u;

  uint32_t head = g_ring.head;
  uint32_t tail = g_ring.tail;
  // Logger::log(String("[Sampling Core] PRE SharedRing status: ")
  //           + " h=" + String(g_ring.head)
  //           + " t=" + String(g_ring.tail)
  //           + " ovw=" + String(g_ring.overruns));

  // Check full: number of items = head - tail
  if ((head - tail) >= cap) {
    // Ring full: drop oldest by advancing tail, count overrun
    g_ring.tail = tail + 1;
    g_ring.overruns++;
  }

  // Write sample at index = head & mask
  g_ring.samples[head & mask] = sample;

  // Ensure sample write is visible before publishing the new head
  ring_dmb();

  // Publish
  g_ring.head = head + 1;

  // Logger::log(String("[Sampling Core] POST SharedRing status: ")
  //         + " h=" + String(g_ring.head)
  //         + " t=" + String(g_ring.tail)
  //         + " ovw=" + String(g_ring.overruns));
}

// ---------------- Consumer ----------------
// Copies up to max_samples into out (if max_samples < 0, copy all available).
// Returns number of samples copied.
size_t SharedRing_Consume(REMCSample* out, int32_t max_samples) {
  if (!out) return 0;

  const uint32_t cap  = g_ring.capacity;
  const uint32_t mask = cap - 1u;
  // Logger::log(String("[Serial Core] PRE SharedRing status: ")
  //           + " h=" + String(g_ring.head)
  //           + " t=" + String(g_ring.tail)
  //           + " ovw=" + String(g_ring.overruns));
  // Snapshot producer head once
  uint32_t head_snapshot = g_ring.head;
  ring_dmb(); // make sure we see writes before using head

  uint32_t tail = g_ring.tail;
  uint32_t available = head_snapshot - tail;

  if (available == 0) return 0;

  uint32_t take = (max_samples < 0)
                    ? available
                    : (uint32_t)min<int32_t>(max_samples, (int32_t)available);

  // Copy out in two linear chunks if wrap splits the region
  uint32_t first_chunk = min<uint32_t>(take, cap - (tail & mask));
  for (uint32_t i = 0; i < first_chunk; ++i) {
    out[i] = g_ring.samples[(tail + i) & mask];
  }
  uint32_t remaining = take - first_chunk;
  for (uint32_t i = 0; i < remaining; ++i) {
    out[first_chunk + i] = g_ring.samples[(tail + first_chunk + i) & mask];
  }

  // Publish new tail
  ring_dmb();          // ensure reads complete before moving tail
  g_ring.tail = tail + take;
  // Logger::log(String("[Serial Core] POST SharedRing status: ")
  //           + " h=" + String(g_ring.head)
  //           + " t=" + String(g_ring.tail)
  //           + " ovw=" + String(g_ring.overruns));
  return take;
}

