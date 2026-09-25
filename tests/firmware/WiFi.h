// WiFi.h falso
#pragma once
#include <string>
#include "Arduino.h"
struct TextoFalso {
  std::string s;
  const char *c_str() const { return s.c_str(); }
};
struct IPFalso {
  TextoFalso toString() const { return {"192.168.4.1"}; }
};
struct WiFiFalso {
  const char *ssid = nullptr;
  int max_con = -1;
  bool softAP(const char *s, const char *, int = 1, int = 0, int m = 4) { ssid = s; max_con = m; return true; }
  IPFalso softAPIP() { return {}; }
  TextoFalso softAPSSID() { return {ssid ? ssid : ""}; }
  int softAPgetStationNum() { return 0; }
};
inline WiFiFalso WiFi;
