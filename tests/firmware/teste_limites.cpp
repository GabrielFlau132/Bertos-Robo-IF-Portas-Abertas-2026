// Confere os limites de velocidade (valores padrão do parametros.h).
#include <string>
#include <cstdlib>
#include "Arduino.h"
#include "codigo_robo_controle_p3.ino"

static int falhas = 0;
Controller c;

int maiorPwmRodas() {
  int m = 0;
  for (int p : {PINO_1_MOTOR_ESQUERDO, PINO_2_MOTOR_ESQUERDO, PINO_1_MOTOR_DIREITO, PINO_2_MOTOR_DIREITO})
    m = std::max(m, g_pwm[p]);
  return m;
}
void controle() { BP32.tem_dados = true; loop(); BP32.tem_dados = false; }
void aperta(bool &b) { b = true; controle(); b = false; controle(); }
void gesto(int a, int d, int m) {
  for (int i = 0; i < 100; i++) {
    if (i % 33 == 0) g_udp_fila.push_back({0xAA, (uint8_t)(int8_t)a, (uint8_t)(int8_t)d, (uint8_t)(int8_t)m, 0});
    g_millis++; loop();
  }
}
void confere(const char *nome, int pwm, int pct) {
  int esperado = 255 * pct / 100;
  bool ok = std::abs(pwm - esperado) <= 2;
  std::printf("  [%s] %-34s PWM %3d (%3d%%)  esperado ~%3d (%d%%)\n", ok ? " ok " : "FALHOU", nome, pwm,
              pwm * 100 / 255, esperado, pct);
  if (!ok) falhas++;
}

int main() {
  g_millis = 1000;
  setup();
  BP32.conectou(&c);
  aperta(c.start);

  std::printf("MODO CONTROLE (frente %d%%, giro %d%%)\n", LIMITE_CONTROLE_FRENTE, LIMITE_CONTROLE_GIRO);
  c.btn_r2 = true; controle(); confere("R2 (frente)", maiorPwmRodas(), LIMITE_CONTROLE_FRENTE);
  c.btn_r2 = false; c.btn_l2 = true; controle(); confere("L2 (re)", maiorPwmRodas(), LIMITE_CONTROLE_FRENTE);
  c.btn_l2 = false; c.rx = 508; controle(); confere("analogico direita (gira)", maiorPwmRodas(), LIMITE_CONTROLE_GIRO);
  c.rx = -512; controle(); confere("analogico esquerda (gira)", maiorPwmRodas(), LIMITE_CONTROLE_GIRO);
  c.rx = 0; controle();
  // R2 + um pouco de direita: tem que virar pra DIREITA (esquerda mais rapida), nunca pro outro lado
  c.btn_r2 = true; c.rx = 40; controle();
  int esq = g_pwm[PINO_2_MOTOR_ESQUERDO], dir = g_pwm[PINO_2_MOTOR_DIREITO];
  bool okCurva = esq >= dir && esq > 0 && dir > 0;
  std::printf("  [%s] R2 + direita leve: esq %d / dir %d (vira pra direita)\n", okCurva ? " ok " : "FALHOU", esq, dir);
  if (!okCurva) falhas++;
  c.btn_r2 = false; c.rx = 0; controle();
  aperta(c.bt_b);
  bool armaCheia = g_pwm[PINO_2_ARMA1] == 255;
  std::printf("  [%s] arma pelo O continua 100%%\n", armaCheia ? " ok " : "FALHOU"); if (!armaCheia) falhas++;
  aperta(c.bt_y);

  std::printf("MODO GESTOS (frente %d%%, giro %d%%)\n", LIMITE_GESTOS_FRENTE, LIMITE_GESTOS_GIRO);
  aperta(c.bt_r1);
  gesto(100, 0, 0);  confere("acel +100 (frente)", maiorPwmRodas(), LIMITE_GESTOS_FRENTE);
  gesto(-100, 0, 0); confere("acel -100 (re)", maiorPwmRodas(), LIMITE_GESTOS_FRENTE);
  gesto(0, 100, 0);  confere("dire +100 (gira)", maiorPwmRodas(), LIMITE_GESTOS_GIRO);
  gesto(0, -100, 0); confere("dire -100 (gira)", maiorPwmRodas(), LIMITE_GESTOS_GIRO);
  gesto(50, 0, 0);   confere("acel +50", maiorPwmRodas(), LIMITE_GESTOS_FRENTE / 2);
  gesto(0, 0, 100);
  armaCheia = g_pwm[PINO_2_ARMA1] == 255;
  std::printf("  [%s] arma pelos gestos continua ate 100%%\n", armaCheia ? " ok " : "FALHOU"); if (!armaCheia) falhas++;
  gesto(0, 0, 0);
  std::printf("  [%s] comando zero para as rodas\n", maiorPwmRodas() == 0 ? " ok " : "FALHOU"); if (maiorPwmRodas()) falhas++;

  std::printf("\n%s (%d falha(s))\n", falhas ? "HOUVE FALHAS" : "TUDO OK", falhas);
  return falhas ? 1 : 0;
}
