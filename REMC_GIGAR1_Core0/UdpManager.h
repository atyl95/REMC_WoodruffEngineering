// UdpManager.h
#ifndef UDP_MANAGER_H
#define UDP_MANAGER_H

#include <Arduino.h> 
#include <EthernetUdp.h>

// Forward declare Sample struct from SharedRing
struct Sample;

namespace UdpManager {

  void init();
  void processIncoming();
  
  // Main interface - accepts samples and bundles them
  bool addSample(const Sample& sample);
  void flushSamples();  // Send current bundle
  
  // Command processing
  void update();
  
  // Diagnostics - current bundle status
  size_t getBufferUsage();
  size_t getBufferCapacity();
  
  // UDP object access
  EthernetUDP* getUdpObject();
  EthernetUDP* getNTPUdpObject();
  
  // Collected samples tagging
  void startSendingCollectedSamples();
  void stopSendingCollectedSamples();
  void sendBatchEndMarker();
  
  // Legacy functions (deprecated/unused)
  bool isPacketReady();
  void sendPacketIfReady(); 
  void snapshotTelemetry_ISR(); 
  void onSampleTick(uint32_t irq_us);
}

#endif // UDP_MANAGER_H