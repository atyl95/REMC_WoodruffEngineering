// UdpManager.h
#ifndef UDP_MANAGER_H
#define UDP_MANAGER_H

#include <Arduino.h> 
#include <EthernetUdp.h>

namespace UdpManager {

  void init();


  void sendNeutrinoPacket();


  void processIncoming();

  void update(); 
  void snapshotTelemetry_ISR(); 
  void sendPacketIfReady();
  void onSampleTick(uint32_t irq_us);
}

#endif // UDP_MANAGER_H
