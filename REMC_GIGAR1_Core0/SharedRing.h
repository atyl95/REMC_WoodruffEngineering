#pragma once
#include <stdint.h>
#include <stddef.h>

// Force a consistent layout: 4-byte aligned, no packing shenanigans
struct __attribute__((aligned(4))) Sample {
  uint32_t t_us;      // put 32-bit first (keeps struct naturally aligned)
  uint32_t rollover_count;      // put 32-bit first (keeps struct naturally aligned)
  uint16_t swI, swV, outA, outB, t1; // 5 * 2 = 10 bytes
  uint32_t t_us_end;      
  uint32_t rollover_count_end;
  uint16_t _pad;      // explicit pad so sizeof(Sample) is 16 bytes everywhere
};

static_assert(sizeof(Sample) == 28, "Sample must be 28 bytes");
static_assert(alignof(Sample) == 4, "Sample align must be 4");

// Choose a power-of-two capacity for fast masking
#ifndef SHARED_RING_CAPACITY
#define SHARED_RING_CAPACITY 1024u
#endif

struct __attribute__((aligned(32))) SharedRing {
  uint32_t capacity;         // frames capacity (power of two)
  uint32_t head;    // producer writes frames, then increments head
  uint32_t tail;    // consumer reads frames, then increments tail
  uint32_t overruns;// producer increments when overwriting
  Sample samples[SHARED_RING_CAPACITY];
};


extern SharedRing& g_ring;
void SharedRing_Init();

// Add a sample to the ring buffer
void  SharedRing_Add(const Sample& sample);

// Copies up to max_samples into out (if max_samples < 0, copy all available).
// Returns the number of samples copied.
size_t  SharedRing_Consume(Sample* out, int32_t max_samples);
