// Bluepad32.h falso: mesmos nomes de métodos usados no firmware (e no cupim).
#pragma once
#include "Arduino.h"

#define BP32_MAX_GAMEPADS 4

struct ControllerProperties { uint8_t btaddr[6] = {0xa8, 0x47, 0x4a, 0xbc, 0xab, 0xfa}; };

struct Controller {
  bool conectado = true, dados = true;
  int32_t ax = 0, ay = 0, rx = 0, ry = 0, gat_r2 = 0, gat_l2 = 0;
  bool select = false, start = false, l3 = false, r3 = false;
  uint8_t setas = 0;
  int cor_r = -1, cor_g = -1, cor_b = -1, trocas_cor = 0;
  ControllerProperties props;

  bool isConnected() { return conectado; }
  bool hasData() { return dados; }
  bool isGamepad() { return true; }
  bool miscSelect() { return select; }
  bool miscStart() { return start; }
  bool thumbL() { return l3; }
  bool thumbR() { return r3; }
  uint8_t dpad() { return setas; }
  int32_t axisX() { return ax; }
  int32_t axisY() { return ay; }
  int32_t axisRX() { return rx; }
  int32_t axisRY() { return ry; }
  int32_t throttle() { return gat_r2; }
  int32_t brake() { return gat_l2; }
  bool bt_a = false, bt_b = false, bt_x = false, bt_y = false, bt_l1 = false, bt_r1 = false;
  bool a() { return bt_a; }
  bool b() { return bt_b; }
  bool x() { return bt_x; }
  bool y() { return bt_y; }
  bool l1() { return bt_l1; }
  bool r1() { return bt_r1; }
  bool btn_r2 = false, btn_l2 = false;
  bool r2() { return btn_r2; }
  bool l2() { return btn_l2; }
  void setColorLED(uint8_t r, uint8_t g, uint8_t b) { cor_r = r; cor_g = g; cor_b = b; trocas_cor++; }
  ControllerProperties getProperties() { return props; }
  int leds = -1;
  void setPlayerLEDs(uint8_t m) { leds = m; }
};
typedef Controller *ControllerPtr;

typedef void (*CallbackControle)(ControllerPtr);
struct BP32Falso {
  CallbackControle conectou = nullptr, desconectou = nullptr;
  bool tem_dados = false;
  void setup(CallbackControle c, CallbackControle d) { conectou = c; desconectou = d; }
  void enableVirtualDevice(bool) {}
  void forgetBluetoothKeys() {}
  bool update() { bool r = tem_dados; return r; }
  const uint8_t *localBdAddress() { static uint8_t a[6] = {0xcc, 0x50, 0xe3, 0xaf, 0xe2, 0x96}; return a; }
};
inline BP32Falso BP32;
