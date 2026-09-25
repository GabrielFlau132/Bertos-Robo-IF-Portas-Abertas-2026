// Imprime os PWMs das rodas e da arma para uma lista de situações.
// Compilado duas vezes: -DOFICIAL (código original, comandos nos analógicos) e sem
// (versão nova, R2/L2 + analógico direito). As duas saídas têm que ser idênticas.
#include "Arduino.h"
#include "codigo_robo_controle_p3.ino"

Controller c;

void envia() { processControllers(); }

void zera() {
  c.ax = c.ay = c.rx = c.ry = 0;
  c.gat_r2 = c.gat_l2 = 0;
  c.btn_r2 = c.btn_l2 = false;
  c.bt_b = c.bt_x = c.bt_y = false;
  c.setas = 0;
}

// frente: 0..1 | lado: -1..1 (direita +)
void comando(float frente, float lado) {
#ifdef OFICIAL
  c.ry = (int32_t)(frente >= 0 ? -512 * frente : -508 * frente);  // cima = negativo
  c.ax = (int32_t)(lado >= 0 ? 508 * lado : 512 * lado);
#else
  if (frente >= 0) c.gat_r2 = (int32_t)(1023 * frente); else c.gat_l2 = (int32_t)(-1023 * frente);
  c.rx = (int32_t)(lado >= 0 ? 508 * lado : 512 * lado);
#endif
}

void mostra(const char *nome) {
  std::printf("%-34s E1=%3d E2=%3d D1=%3d D2=%3d | A1=%3d/%3d A2=%3d/%3d\n", nome,
              g_pwm[PINO_1_MOTOR_ESQUERDO], g_pwm[PINO_2_MOTOR_ESQUERDO],
              g_pwm[PINO_1_MOTOR_DIREITO], g_pwm[PINO_2_MOTOR_DIREITO],
              g_pwm[PINO_1_ARMA1], g_pwm[PINO_2_ARMA1], g_pwm[PINO_1_ARMA2], g_pwm[PINO_2_ARMA2]);
}

int main() {
  setup();
  myControllers[0] = &c;
  c.start = true; envia(); c.start = false;
#ifdef OFICIAL
  // o oficial liga com outro sentido padrão; a seta pra baixo deixa igual ao padrão novo
  c.setas = 0x02; envia(); c.setas = 0;
#endif

  struct { const char *nome; float f, l; } casos[] = {
    {"parado", 0, 0},          {"frente 100%", 1, 0},     {"frente 50%", 0.5f, 0},
    {"tras 100%", -1, 0},      {"tras 50%", -0.5f, 0},    {"gira direita 100%", 0, 1},
    {"gira esquerda 100%", 0, -1}, {"frente 100% + direita 100%", 1, 1},
    {"frente 100% + esquerda 50%", 1, -0.5f}, {"tras 100% + direita 100%", -1, 1},
    {"tras 50% + esquerda 100%", -0.5f, -1},
  };
  for (auto &k : casos) {
    zera(); comando(k.f, k.l); envia(); mostra(k.nome);
  }
  zera(); c.bt_b = true; envia(); mostra("arma: bolinha");
  zera(); c.bt_x = true; envia(); mostra("arma: quadrado");
  zera(); c.bt_y = true; envia(); mostra("arma: triangulo");
  zera(); c.setas = 0x02; envia(); comando(1, 0); envia(); mostra("seta baixo + frente");
  zera(); c.select = true; envia(); c.select = false; comando(1, 0); envia(); mostra("SELECT + frente (desligado)");
  return 0;
}
