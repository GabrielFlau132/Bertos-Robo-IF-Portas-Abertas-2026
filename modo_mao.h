// modo_mao.h — recebe comandos do controle_mao.py via UDP (ESP32 em softAP)
#pragma once
#include <WiFi.h>
#include <WiFiUdp.h>

#define MAO_SSID       "RoboBatalha"
#define MAO_SENHA      "12345678"   // mínimo 8 caracteres
#define MAO_PORTA      4210
#define MAO_TIMEOUT_MS 300          // sem pacote nesse tempo = comando inválido (para)

struct ComandoMao {
  int8_t  aceleracao;  // -100..100
  int8_t  direcao;     // -100..100
  int8_t  arma;        // -100..100 (sinal = sentido de rotação)
};

static WiFiUDP    udpMao;
static ComandoMao cmdMao = {0, 0, 0};
static uint32_t   ultimoPacoteMao = 0;

void iniciarModoMao() {
  WiFi.softAP(MAO_SSID, MAO_SENHA);  // IP do robô: 192.168.4.1
  udpMao.begin(MAO_PORTA);
}

// Lê todos os pacotes pendentes e fica com o mais recente.
// Retorna true se o último comando válido tem menos de MAO_TIMEOUT_MS.
bool lerModoMao() {
  int tam;
  while ((tam = udpMao.parsePacket()) > 0) {
    uint8_t buf[5];
    if (tam == 5 && udpMao.read(buf, 5) == 5 && buf[0] == 0xAA) {
      cmdMao.aceleracao = (int8_t)buf[1];
      cmdMao.direcao    = (int8_t)buf[2];
      cmdMao.arma       = (int8_t)buf[3];
      ultimoPacoteMao   = millis();
    }
  }
  if (millis() - ultimoPacoteMao >= MAO_TIMEOUT_MS) {
    cmdMao = {0, 0, 0};
    return false;
  }
  return true;
}
