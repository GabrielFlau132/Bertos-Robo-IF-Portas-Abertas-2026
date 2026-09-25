// parametros.h — pinagem e ajustes do robô Bertos
//
// Placa ROBO_INICIATIVA_V2_DRV (ESP32 DevKit + 2x DRV8833) do repositório
// nrc-cupim/cupim_start_inatel (pcbs/2_motores_drv). Os pinos da placa são os mesmos,
// mas no Bertos os motores estão ligados em saídas diferentes das do
// robo_inicativa_feiras. Mapa levantado com o firmware/teste_motores no robô:
//   25/26 (DRV U15 IN3/IN4) = roda esquerda
//   33/32 (DRV U15 IN2/IN1) = roda direita
//   13/12 (DRV U12 IN4/IN3) = arma
//   14/27 (DRV U12 IN2/IN1) = nada ligado (segundo motor de arma, se um dia tiver)
// Os pinos EEP (sleep) e ULT (fault) dos DRV ficam soltos: o módulo já vem habilitado.

#ifndef parametros_h
#define parametros_h

#include <Arduino.h>

#define PINO_LED_INTERNO 2  // aceso = a ESP32 está recebendo comando de movimento/arma

// Manda luz/LEDs pro controle (barra do PS4, LEDs de jogador do PS3). Desligado:
// o controle atual é genérico e pode não aceitar esse comando.
const bool USAR_LUZ_CONTROLE = false;

#define PINO_1_MOTOR_ESQUERDO 25
#define PINO_2_MOTOR_ESQUERDO 26

#define PINO_1_MOTOR_DIREITO 33
#define PINO_2_MOTOR_DIREITO 32

#define PINO_1_ARMA1 13
#define PINO_2_ARMA1 12  // GPIO12 é pino de boot: se a ESP32 não ligar com a placa, desconfie dele

#define PINO_1_ARMA2 14  // canal sem motor hoje (ver ARMA2_HABILITADA)
#define PINO_2_ARMA2 27

// Faixas do Bluepad32 (controle de PS4)
// Analógicos: cima = negativo no Y, direita = positivo no X. A faixa medida no
// cupim é assimétrica (-512..508), então 508 conta como 100% nos dois sentidos.
// Gatilhos L2/R2: 0..1023
const int32_t MAX_JOYSTICK = 508;
const int32_t MAX_GATILHO = 1023;

// Zona morta: analógico de PS4 usado costuma não voltar exatamente pro 0
const int32_t ZONA_MORTA_JOYSTICK = 40;  // ~8%
const int32_t ZONA_MORTA_GATILHO = 40;

// PWM máximo (0..255). Baixe pra limitar a velocidade, ex.: visitante pilotando.
// DIAGNÓSTICO (2026-09-24): em 255 com as duas rodas juntas a roda esquerda não
// girou e os comandos atrasaram 3-5 s; no teste_motores, 160 numa roda por vez
// funcionou. Limitado a 160 pra ver se o problema é a alimentação.
const int MAX_PWM_LOCOMOCAO = 160;
const int MAX_PWM_ARMA = 160;

// Rampa (soft-start): tempo pra ir de parado a 100%. Reduzir é sempre imediato
// (motor fica em roda livre). Partida brusca já derrubou a alimentação e resetou
// o rádio numa versão anterior do robô, por isso a arma sobe bem mais devagar.
const uint16_t RAMPA_LOCOMOCAO_MS = 300;
const uint16_t RAMPA_ARMA_MS = 800;

// Tempo parado antes de girar no sentido contrário (o motor ainda está girando
// por inércia; inverter na hora puxa um pico de corrente enorme).
// A arma tem muito mais inércia: aumente se ela ainda estiver girando forte ao inverter.
const uint16_t TEMPO_MORTO_LOCOMOCAO_MS = 60;
const uint16_t TEMPO_MORTO_ARMA_MS = 500;

// Segundo motor de arma no canal 14/27. Desligado porque hoje não há nada ligado
// nele. Se instalar um segundo motor acoplado à arma, habilite e teste com a arma
// desacoplada: com ARMA2_INVERTIDA = true ele recebe o sinal invertido em relação
// à ARMA1. Se um motor frear o outro, troque ARMA2_INVERTIDA.
const bool ARMA2_HABILITADA = false;
const bool ARMA2_INVERTIDA = true;

#define DEBUG_SERIAL 1  // 1 = imprime o estado no monitor serial 5x por segundo

#endif
