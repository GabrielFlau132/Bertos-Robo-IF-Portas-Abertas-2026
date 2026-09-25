// Arduino.h falso: só o necessário pra rodar a lógica do firmware no PC.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstdarg>
#include <map>

#define OUTPUT 1
#define LOW 0
#define HIGH 1
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

inline uint32_t g_millis = 0;
inline std::map<int, int> g_pwm;     // pino -> último analogWrite
inline std::map<int, int> g_digital; // pino -> último digitalWrite
inline bool g_serial_mudo = true;

inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;  // igual ao Arduino
}
inline uint32_t millis() { return g_millis; }
inline void delay(uint32_t ms) { g_millis += ms; }
inline void pinMode(int, int) {}
inline void analogWrite(int p, int v) { g_pwm[p] = v; }
inline void digitalWrite(int p, int v) { g_digital[p] = v; }

struct SerialFalso {
  void begin(int) {}
  void println(const char *s) { if (!g_serial_mudo) std::printf("    [serial] %s\n", s); }
  void println() {}
  void println(long) {}
  void print(const char *) {}
  void print(long) {}
  void printf(const char *fmt, ...) {
    if (g_serial_mudo) return;
    va_list a; va_start(a, fmt); std::printf("    [serial] "); std::vprintf(fmt, a); va_end(a);
  }
};
inline SerialFalso Serial;
