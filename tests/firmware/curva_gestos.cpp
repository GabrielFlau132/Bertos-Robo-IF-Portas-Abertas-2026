// Curva de resposta: modo gestos, acel fixo, dire de -100 a 100. Mostra a velocidade de
// cada roda (positivo = pra frente, com o sentido padrão = seta pra baixo).
#include <string>
#include "Arduino.h"
#include "codigo_robo_controle_p3.ino"

Controller c;
void controle() { BP32.tem_dados = true; loop(); BP32.tem_dados = false; }
void aperta(bool &b) { b = true; controle(); b = false; controle(); }
void gesto(int a, int d) {
  for (int i = 0; i < 70; i++) {
    if (i % 33 == 0) g_udp_fila.push_back({0xAA, (uint8_t)(int8_t)a, (uint8_t)(int8_t)d, 0, 0});
    g_millis++; loop();
  }
}
int main(int argc, char **argv) {
  int acel = argc > 1 ? atoi(argv[1]) : 100;
  g_millis = 1000; setup(); BP32.conectou(&c); aperta(c.start); aperta(c.bt_r1);
  std::printf("acel=%d\n dire  roda_esq  roda_dir\n", acel);
  for (int d : {-100, -60, -30, -15, -8, -4, -2, 0, 2, 4, 8, 15, 30, 60, 100}) {
    gesto(acel, d);
    // com seta pra baixo: frente = pino 2 nas duas rodas
    int esq = g_pwm[PINO_2_MOTOR_ESQUERDO] - g_pwm[PINO_1_MOTOR_ESQUERDO];
    int dir = g_pwm[PINO_2_MOTOR_DIREITO] - g_pwm[PINO_1_MOTOR_DIREITO];
    std::printf(" %+4d   %+5d     %+5d   %s\n", d, esq, dir, esq > dir ? "-> vira DIREITA" : esq < dir ? "-> vira ESQUERDA" : "reto");
  }
}
