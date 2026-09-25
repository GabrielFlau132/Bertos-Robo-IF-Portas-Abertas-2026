// WiFiUdp.h falso: pacotes entram por g_udp_fila
#pragma once
#include <deque>
#include <vector>
#include <cstring>
#include "Arduino.h"

inline std::deque<std::vector<uint8_t>> g_udp_fila;
inline int g_udp_porta = -1;

class WiFiUDP {
  std::vector<uint8_t> atual;
 public:
  uint8_t begin(int porta) { g_udp_porta = porta; return 1; }
  int parsePacket() {
    if (g_udp_fila.empty()) return 0;
    atual = g_udp_fila.front();
    g_udp_fila.pop_front();
    return (int)atual.size();
  }
  int read(uint8_t *buf, int n) {
    int k = std::min<int>(n, (int)atual.size());
    std::memcpy(buf, atual.data(), k);
    return k;
  }
};
