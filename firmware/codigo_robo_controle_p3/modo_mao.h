// modo_mao.h — recebe comandos do controle_mao.py via UDP (ESP32 em softAP)
#pragma once
#include <WiFi.h>
#include <WiFiUdp.h>

#define MAO_SSID       "RoboBatalha"
#define MAO_SENHA      "12345678"   // mínimo 8 caracteres
#define MAO_PORTA      4210
#define MAO_TIMEOUT_MS 300          // sem pacote nesse tempo = comando inválido (para)
#define MAO_MAX_CONEXOES 1          // só o notebook dos gestos entra na rede

struct ComandoMao {
  int8_t  aceleracao;  // -100..100 (positivo = frente)
  int8_t  direcao;     // -100..100 (positivo = direita)
  int8_t  arma;        // -100..100 (sinal = sentido de rotação)
};

static WiFiUDP    udpMao;
static ComandoMao cmdMao = {0, 0, 0};
static uint32_t   ultimoPacoteMao = 0;

void iniciarModoMao() {
  // IP do robô: 192.168.4.1 | canal 1, rede visível, no máximo MAO_MAX_CONEXOES aparelho
  bool apOk = WiFi.softAP(MAO_SSID, MAO_SENHA, 1, 0, MAO_MAX_CONEXOES);
  bool udpOk = udpMao.begin(MAO_PORTA);
  Serial.printf("Modo gestos: rede %s %s, IP %s, UDP %d %s\n", MAO_SSID, apOk ? "OK" : "FALHOU",
                WiFi.softAPIP().toString().c_str(), MAO_PORTA, udpOk ? "OK" : "FALHOU");
}

// Imprime no serial, a cada 3 s, o estado da rede (ajuda a testar na bancada, com USB).
void statusModoMao(bool modoGestos) {
  static uint32_t ultimo = 0;
  if (millis() - ultimo < 3000) return;
  ultimo = millis();
  Serial.printf("[wifi] rede %s IP %s | aparelhos conectados: %d | ultimo pacote ha %lu ms | modo %s\n",
                WiFi.softAPSSID().c_str(), WiFi.softAPIP().toString().c_str(), WiFi.softAPgetStationNum(),
                (unsigned long)(millis() - ultimoPacoteMao), modoGestos ? "GESTOS" : "CONTROLE");
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
