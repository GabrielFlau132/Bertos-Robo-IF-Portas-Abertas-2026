
#ifndef parametros_h
#define parametros_h

#include <Arduino.h>

#define PINO_LED_INTERNO 2

#define PINO_1_ARMA2 13
#define PINO_2_ARMA2 12

#define PINO_1_MOTOR_ESQUERDO 14
#define PINO_2_MOTOR_ESQUERDO 27

#define PINO_1_MOTOR_DIREITO 25
#define PINO_2_MOTOR_DIREITO 26

#define PINO_1_ARMA1 33
#define PINO_2_ARMA1 32

// Comportamento natural do controle em ambos os analógicos
// Cima - Baixo +
// Direita + Esquerda -

// Ambos os analógicos (E e D) tem o mesmo comportamento, retornam os mesmos valores para os mesmos sentidos / direções
// Ambas as direções retornam a mesma faixa de valores (-512 a 508, tanto na vertical quanto horizontal)
// Os valores retornados para cada um dos sentidos em uma mesma direção (esquerda/direita ou cima/baixo) não são simétricos

const int32_t MIN_JOYSTICK_Y = -512, PARADO_JOYSTICK_Y = 0, MAX_JOYSTICK_Y = 508;
const int32_t MIN_JOYSTICK_X = -512, PARADO_JOYSTICK_X = 0, MAX_JOYSTICK_X = 508;

const int32_t MIN_GATILHO = 0, MAX_GATILHO = 1023;  // faixa dos gatilhos L2/R2 na Bluepad32

const int32_t TOLERANCIA_JOYSTICK = 5;  // zona morta do controle

const int MIN_PWM = 0, MAX_PWM = 255;  // valores limite para o PWM da ESP32

const uint32_t PERIODO_GESTOS_MS = 30;  // de quanto em quanto tempo aplica o comando dos gestos

// Limite de velocidade da locomoção, em % do máximo (100 = sem limite, igual ao oficial).
// Controle: "FRENTE" = andando (reduz o PWM final), "GIRO" = girando no lugar.
// Gestos: "FRENTE" = peso da aceleração, "GIRO" = peso da curva na mistura.
// A arma não é limitada.
#ifndef LIMITE_CONTROLE_FRENTE
#define LIMITE_CONTROLE_FRENTE 80
#endif
#ifndef LIMITE_CONTROLE_GIRO
#define LIMITE_CONTROLE_GIRO 60
#endif
#ifndef LIMITE_GESTOS_FRENTE
#define LIMITE_GESTOS_FRENTE 60
#endif
#ifndef LIMITE_GESTOS_GIRO
#define LIMITE_GESTOS_GIRO 40
#endif

#endif