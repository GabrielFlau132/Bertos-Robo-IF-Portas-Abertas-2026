// Testa a integração controle + gestos do codigo_robo_controle_p3 no PC.
#include <string>
#include "Arduino.h"
#include "codigo_robo_controle_p3.ino"

static int falhas = 0;
#define CONFERE(cond, msg)                                                  \
  do {                                                                      \
    bool ok_ = (cond);                                                      \
    std::printf("  [%s] %s\n", ok_ ? " ok " : "FALHOU", msg);               \
    if (!ok_) falhas++;                                                     \
  } while (0)

Controller c;

// estado dos 4 pinos das rodas e da arma, como texto (pra comparar padrões)
std::string rodas() {
  char s[64];
  std::snprintf(s, sizeof s, "E1=%d E2=%d D1=%d D2=%d", g_pwm[PINO_1_MOTOR_ESQUERDO], g_pwm[PINO_2_MOTOR_ESQUERDO],
                g_pwm[PINO_1_MOTOR_DIREITO], g_pwm[PINO_2_MOTOR_DIREITO]);
  return s;
}
std::string arma() {
  char s[64];
  std::snprintf(s, sizeof s, "A1=%d/%d A2=%d/%d", g_pwm[PINO_1_ARMA1], g_pwm[PINO_2_ARMA1],
                g_pwm[PINO_1_ARMA2], g_pwm[PINO_2_ARMA2]);
  return s;
}
bool tudoParado() { return rodas() == "E1=0 E2=0 D1=0 D2=0" && arma() == "A1=0/0 A2=0/0"; }

// controle manda um relatório (o controle genérico só manda quando algo muda)
void controle() { BP32.tem_dados = true; loop(); BP32.tem_dados = false; }
void aperta(bool &botao) { botao = true; controle(); botao = false; controle(); }

// o PC manda 1 pacote de gestos
void pacote(int acel, int dire, int arm, int seq = 0) {
  g_udp_fila.push_back({0xAA, (uint8_t)(int8_t)acel, (uint8_t)(int8_t)dire, (uint8_t)(int8_t)arm, (uint8_t)seq});
}
// roda 'ms' milissegundos; se 'gestos' != nullptr, o PC manda pacote a cada 33 ms
struct Gesto { int a, d, m; };
void roda(uint32_t ms, const Gesto *g = nullptr) {
  for (uint32_t i = 0; i < ms; i++) {
    if (g && g_millis % 33 == 0) pacote(g->a, g->d, g->m);
    g_millis++;
    loop();
  }
}

// padrão de referência: o que o modo controle faz com R2 / analógico direito
std::string refControle(bool r2, bool l2, int32_t rx) {
  c.btn_r2 = r2; c.btn_l2 = l2; c.rx = rx; controle();
  std::string r = rodas();
  c.btn_r2 = c.btn_l2 = false; c.rx = 0; controle();
  return r;
}

int main() {
  g_millis = 1000;
  setup();
  CONFERE(WiFi.ssid && std::string(WiFi.ssid) == "RoboBatalha" && g_udp_porta == 4210, "Wi-Fi RoboBatalha + UDP 4210 no setup");
  CONFERE(WiFi.max_con == 1, "rede aceita so 1 aparelho");
  BP32.conectou(&c);

  std::printf("\n1) robo desligado: pacotes de gestos nao mexem nada\n");
  roda(200, new Gesto{100, 0, 100});
  CONFERE(tudoParado(), "tudo parado");

  std::printf("\n2) START, modo controle (padrao): referencias\n");
  aperta(c.start);
  CONFERE(roboLigado && !modoGestos, "ligado no modo controle");
  std::string frente = refControle(true, false, 0);
  std::string re = refControle(false, true, 0);
  std::string giraD = refControle(false, false, 508);
  std::string frenteD = refControle(true, false, 508);
  std::printf("     R2=%s | L2=%s | dir=%s | R2+dir=%s\n", frente.c_str(), re.c_str(), giraD.c_str(), frenteD.c_str());
  roda(100, new Gesto{100, 0, 0});
  CONFERE(tudoParado(), "no modo controle os gestos sao ignorados");

  std::printf("\n3) arma ligada pelo controle (O) e R1 -> modo gestos para tudo\n");
  aperta(c.bt_b);
  CONFERE(arma() == "A1=0/255 A2=255/0", "O liga a arma (padrao oficial)");
  roda(400);  // PC sem mandar nada (programa de gestos fechado)
  aperta(c.bt_r1);
  CONFERE(modoGestos, "R1 entra no modo gestos");
  roda(50);
  CONFERE(tudoParado(), "trocar de modo parou a arma e as rodas (sem gestos chegando)");
  aperta(c.bt_l1);

  std::printf("\n3b) R1 com o PC mandando 'frente': segue o gesto atual na hora\n");
  Gesto gf{100, 0, 0}; roda(100, &gf);
  aperta(c.bt_r1); roda(40, &gf);
  CONFERE(rodas() == frente, "entra seguindo o gesto (comportamento esperado)");

  std::printf("\n4) gestos: mesmos padroes de motor do controle\n");
  Gesto g1{100, 0, 0}; roda(100, &g1);
  CONFERE(rodas() == frente, "acel +100 = mesmo que R2");
  Gesto g2{-100, 0, 0}; roda(100, &g2);
  CONFERE(rodas() == re, "acel -100 = mesmo que L2");
  Gesto g3{0, 100, 0}; roda(100, &g3);
  CONFERE(rodas() == giraD, "dire +100 = mesmo que analogico pra direita");
  Gesto g4{100, 100, 0}; roda(100, &g4);
  // mistura propria dos gestos: esquerda 100% pra frente, direita parada (curva fechada)
  CONFERE(rodas() == "E1=0 E2=255 D1=0 D2=0", "acel +100 e dire +100 = curva fechada pra direita");
  Gesto g5{0, 0, 0}; roda(100, &g5);
  CONFERE(rodas() == "E1=0 E2=0 D1=0 D2=0", "comando zero = rodas paradas");

  std::printf("\n5) arma pelos gestos\n");
  Gesto g6{0, 0, 100}; roda(100, &g6);
  CONFERE(arma() == "A1=0/255 A2=255/0", "arma +100 = mesmo sentido e forca da BOLINHA");
  Gesto g7{0, 0, -50}; roda(100, &g7);
  CONFERE(arma() == "A1=127/0 A2=0/127", "arma -50 = sentido do QUADRADO a 50%");
  Gesto g8{0, 0, 0}; roda(100, &g8);
  CONFERE(arma() == "A1=0/0 A2=0/0", "arma 0 = parada");

  std::printf("\n6) botoes do controle nao interferem no modo gestos\n");
  Gesto g9{0, 0, 0};
  c.btn_r2 = true; controle(); roda(100, &g9);
  CONFERE(rodas() == "E1=0 E2=0 D1=0 D2=0", "R2 ignorado (vale o gesto)");
  c.btn_r2 = false; controle();
  aperta(c.bt_b); roda(100, &g9);
  CONFERE(arma() == "A1=0/0 A2=0/0", "O ignorado (vale o gesto)");

  std::printf("\n7) PC para de mandar (programa fechou / Wi-Fi caiu)\n");
  Gesto g10{100, 50, 100}; roda(200, &g10);
  CONFERE(!tudoParado(), "andando e arma girando");
  roda(250);
  CONFERE(!tudoParado(), "ainda dentro dos 300 ms");
  roda(100);
  CONFERE(tudoParado(), "passou 300 ms sem pacote: tudo parado");

  std::printf("\n8) LED azul no modo gestos\n");
  int trocas = 0, antes = g_digital[PINO_LED_INTERNO];
  for (int i = 0; i < 1000; i++) { roda(1); if (g_digital[PINO_LED_INTERNO] != antes) { trocas++; antes = g_digital[PINO_LED_INTERNO]; } }
  std::printf("     sem pacotes: %d trocas/s\n", trocas);
  CONFERE(trocas >= 1 && trocas <= 3, "pisca devagar sem pacotes");
  trocas = 0;
  for (int i = 0; i < 1000; i++) { roda(1, &g9); if (g_digital[PINO_LED_INTERNO] != antes) { trocas++; antes = g_digital[PINO_LED_INTERNO]; } }
  std::printf("     recebendo: %d trocas/s\n", trocas);
  CONFERE(trocas >= 8, "pisca rapido recebendo");

  std::printf("\n9) L1 volta pro controle\n");
  Gesto g11{100, 0, 100}; roda(100, &g11);
  aperta(c.bt_l1);
  CONFERE(!modoGestos, "modo controle");
  CONFERE(tudoParado(), "trocar de modo parou tudo");
  CONFERE(g_digital[PINO_LED_INTERNO] == configsTravadas, "LED volta a mostrar a trava");
  roda(100, &g11);
  CONFERE(tudoParado(), "gestos ignorados de novo");
  CONFERE(refControle(true, false, 0) == frente, "R2 volta a funcionar");

  std::printf("\n10) SELECT no modo gestos\n");
  aperta(c.bt_r1); roda(100, &g11);
  aperta(c.select);
  CONFERE(!roboLigado && !modoGestos && tudoParado(), "desliga, para e volta pro modo controle");
  roda(200, &g11);
  CONFERE(tudoParado(), "continua parado com gestos chegando");
  aperta(c.start);
  CONFERE(roboLigado && !modoGestos, "START religa no modo controle");

  std::printf("\n11) controle desconecta no modo gestos\n");
  aperta(c.bt_r1); roda(100, &g11);
  CONFERE(!tudoParado(), "gestos pilotando");
  BP32.desconectou(&c);
  roda(100, &g11);
  CONFERE(!roboLigado && tudoParado(), "failsafe: tudo parado mesmo com gestos chegando");

  std::printf("\n12) R1 com o robo desligado nao faz nada\n");
  BP32.conectou(&c);
  aperta(c.bt_r1);
  CONFERE(!modoGestos && !roboLigado, "continua desligado no modo controle");

  std::printf("\n%s (%d falha(s))\n", falhas ? "HOUVE FALHAS" : "TUDO OK", falhas);
  return falhas ? 1 : 0;
}
