/* Código oficial nrc-cupim/start-automacao-eletrica (Codigos/codigo_robo_controle_p3,
   commit a52bbf3) com estas mudanças, pedidas pro robô Bertos:
     - R2 = frente, L2 = trás, analógico direito na horizontal = direção
       (antes: analógicos, com L1/R1 trocando qual fazia o quê)
     - sentido padrão dos motores ao ligar = o da seta pra baixo (no setup)
     - MODO GESTOS: com o robô ligado, R1 = pilotar por gestos (controle_mao.py via
       Wi-Fi/UDP, ver modo_mao.h), L1 = voltar pro controle. Um modo de cada vez; trocar
       de modo para todos os motores. No modo gestos o controle continua valendo pra
       START/SELECT, L1 e setas/trava, e se ele desconectar o robô para (failsafe oficial).
       Sem pacote do PC por 300 ms, os motores param.
       LED azul no modo gestos: pisca devagar = sem pacotes do PC, rápido = recebendo.
     - a lógica oficial de mistura dos motores foi movida, sem alteração, para a função
       aplicaMovimento(), usada pelos dois modos.
     - limite de velocidade da locomoção por modo (frente/ré e giro separados), aplicado
       no comando antes de aplicaMovimento(): ver LIMITE_* em parametros.h.
   Arma pelo controle, setas, trava L3+R3 e START/SELECT continuam iguais. */

#include <Bluepad32.h>
#include "parametros.h"
#include "modo_mao.h"

ControllerPtr myControllers[BP32_MAX_GAMEPADS];
bool roboLigado, configsTravadas;
bool modoGestos = false;  // false = controle (L1), true = gestos (R1)

/* Foi necessário utilizar essas variáveis para permitir a
   inversão do sentido de giro de cada motor de locomoção */

int sentidoMotorEsquerdo, velocidadeMotorEsquerdo;
int sentidoMotorDireito, velocidadeMotorDireito;

void desligaRobo() {
  analogWrite(PINO_1_ARMA1, 0);
  analogWrite(PINO_2_ARMA1, 0);

  analogWrite(PINO_1_ARMA2, 0);
  analogWrite(PINO_2_ARMA2, 0);

  analogWrite(sentidoMotorDireito, 0);
  analogWrite(velocidadeMotorDireito, 0);

  analogWrite(sentidoMotorEsquerdo, 0);
  analogWrite(velocidadeMotorEsquerdo, 0);

  roboLigado = false;

  // Robô desligado sempre volta pro modo controle (e o LED volta a mostrar a trava).
  if (modoGestos) {
    modoGestos = false;
    digitalWrite(PINO_LED_INTERNO, configsTravadas);
  }
}

// Para todos os motores sem desligar o robô (usado ao trocar de modo).
void paraMotores() {
  analogWrite(PINO_1_ARMA1, 0);
  analogWrite(PINO_2_ARMA1, 0);
  analogWrite(PINO_1_ARMA2, 0);
  analogWrite(PINO_2_ARMA2, 0);
  analogWrite(PINO_1_MOTOR_DIREITO, 0);
  analogWrite(PINO_2_MOTOR_DIREITO, 0);
  analogWrite(PINO_1_MOTOR_ESQUERDO, 0);
  analogWrite(PINO_2_MOTOR_ESQUERDO, 0);
}

void onConnectedController(ControllerPtr ctl) {

  bool foundEmptySlot = false;

  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == nullptr) {
      Serial.println("AVISO: controle conectado.");
      myControllers[i] = ctl;
      foundEmptySlot = true;

      /* Caso deseje realizar alguma tarefa assim que a conexão
         com o contole for estabelecidada, coloque o código aqui. */

      break;
    }
  }

  if (!foundEmptySlot) {
    Serial.println("AVISO: Nao foi possivel conectar o controle.");
    Serial.println("AVISO: Reinicie a ESP32 e tente novamente.");
  }
}

void onDisconnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == ctl) {
      Serial.printf("AVISO: controle desconectado");
      myControllers[i] = nullptr;
      desligaRobo();

      /* Caso deseje realizar alguma tarefa assim que o
         controle for desconectado, coloque o código aqui */

      break;
    }
  }
}

/* ----------------- Lógica de funcionamento da movimentação ----------------- */
// Trecho oficial, sem alteração: recebe o "analógico" vertical (frente = negativo)
// e horizontal (direita = positivo) na escala -512..508 e aciona os motores.
void aplicaMovimento(int32_t valorAnalogicoV, int32_t valorAnalogicoH) {

        int pwmMotorDireito1, pwmMotorDireito2, pwmMotorEsquerdo1, pwmMotorEsquerdo2;

        // Analógico Y movimentado para trás
        if (valorAnalogicoV > (PARADO_JOYSTICK_Y + TOLERANCIA_JOYSTICK)) {
          pwmMotorDireito1 = MIN_PWM;
          pwmMotorEsquerdo2 = MIN_PWM;

          // Analógico X movimentado para direita
          if (valorAnalogicoH > (PARADO_JOYSTICK_X + TOLERANCIA_JOYSTICK)) {
            pwmMotorDireito2 = map(valorAnalogicoV - valorAnalogicoH,
                                   PARADO_JOYSTICK_Y - MAX_JOYSTICK_X,
                                   MAX_JOYSTICK_Y - PARADO_JOYSTICK_X,
                                   MIN_PWM, MAX_PWM);
            pwmMotorEsquerdo1 = map(valorAnalogicoV, PARADO_JOYSTICK_Y, MAX_JOYSTICK_Y, MIN_PWM, MAX_PWM);
          }

          // Analógico X movimentado para esquerda
          else if (valorAnalogicoH < (PARADO_JOYSTICK_X - TOLERANCIA_JOYSTICK)) {
            pwmMotorDireito2 = map(valorAnalogicoV, PARADO_JOYSTICK_Y, MAX_JOYSTICK_Y, MIN_PWM, MAX_PWM);
            pwmMotorEsquerdo1 = map(valorAnalogicoV + valorAnalogicoH,
                                    PARADO_JOYSTICK_Y + MIN_JOYSTICK_X,
                                    MAX_JOYSTICK_Y + PARADO_JOYSTICK_X,
                                    MIN_PWM, MAX_PWM);
          }

          // Analógico X não movimentado
          else {
            pwmMotorDireito2 = map(valorAnalogicoV, PARADO_JOYSTICK_Y, MAX_JOYSTICK_Y, MIN_PWM, MAX_PWM);
            pwmMotorEsquerdo1 = map(valorAnalogicoV, PARADO_JOYSTICK_Y, MAX_JOYSTICK_Y, MIN_PWM, MAX_PWM);
          }
        }

        // Analógico Y movimentado para frente
        else if (valorAnalogicoV < (PARADO_JOYSTICK_Y - TOLERANCIA_JOYSTICK)) {
          pwmMotorDireito2 = MIN_PWM;
          pwmMotorEsquerdo1 = MIN_PWM;

          // Analógico X movimentado para direita
          if (valorAnalogicoH > (PARADO_JOYSTICK_X + TOLERANCIA_JOYSTICK)) {
            pwmMotorDireito1 = map(valorAnalogicoV + valorAnalogicoH,
                                   MIN_JOYSTICK_Y + PARADO_JOYSTICK_X,
                                   PARADO_JOYSTICK_Y + MAX_JOYSTICK_X,
                                   MAX_PWM, MIN_PWM);
            pwmMotorEsquerdo2 = map(valorAnalogicoV, MIN_JOYSTICK_Y, PARADO_JOYSTICK_Y, MAX_PWM, MIN_PWM);
          }

          // Analógico X movimentado para esquerda
          else if (valorAnalogicoH < (PARADO_JOYSTICK_X - TOLERANCIA_JOYSTICK)) {
            pwmMotorDireito1 = map(valorAnalogicoV, MIN_JOYSTICK_Y, PARADO_JOYSTICK_Y, MAX_PWM, MIN_PWM);
            pwmMotorEsquerdo2 = map(valorAnalogicoV - valorAnalogicoH,
                                    MIN_JOYSTICK_Y - PARADO_JOYSTICK_X,
                                    PARADO_JOYSTICK_Y - MIN_JOYSTICK_X,
                                    MIN_PWM, MAX_PWM);
          }

          // Analógico X não movimentado
          else {
            pwmMotorDireito1 = map(valorAnalogicoV, MIN_JOYSTICK_Y, PARADO_JOYSTICK_Y, MAX_PWM, MIN_PWM);
            pwmMotorEsquerdo2 = map(valorAnalogicoV, MIN_JOYSTICK_Y, PARADO_JOYSTICK_Y, MAX_PWM, MIN_PWM);
          }
        }

        // Analógico Y não movimentado
        else {

          // Analógico X movimentado para direita
          if (valorAnalogicoH > (PARADO_JOYSTICK_X + TOLERANCIA_JOYSTICK)) {
            pwmMotorDireito1 = MIN_PWM;
            pwmMotorEsquerdo1 = MIN_PWM;
            pwmMotorDireito2 = map(valorAnalogicoH, PARADO_JOYSTICK_X, MAX_JOYSTICK_X, MIN_PWM, MAX_PWM);
            pwmMotorEsquerdo2 = map(valorAnalogicoH, PARADO_JOYSTICK_X, MAX_JOYSTICK_X, MIN_PWM, MAX_PWM);
          }

          // Analógico X movimentado para esquerda
          else if (valorAnalogicoH < (PARADO_JOYSTICK_X - TOLERANCIA_JOYSTICK)) {
            pwmMotorDireito1 = map(valorAnalogicoH, MIN_JOYSTICK_X, PARADO_JOYSTICK_X, MAX_PWM, MIN_PWM);
            pwmMotorEsquerdo1 = map(valorAnalogicoH, MIN_JOYSTICK_X, PARADO_JOYSTICK_X, MAX_PWM, MIN_PWM);
            pwmMotorDireito2 = MIN_PWM;
            pwmMotorEsquerdo2 = MIN_PWM;
          }

          // Analógico X não movimentado
          else {
            pwmMotorDireito1 = MIN_PWM;
            pwmMotorEsquerdo1 = MIN_PWM;
            pwmMotorDireito2 = MIN_PWM;
            pwmMotorEsquerdo2 = MIN_PWM;
          }
        }

        Serial.print("PWM Direito 1: ");
        Serial.println(pwmMotorDireito1);
        Serial.print("PWM Direito 2: ");
        Serial.println(pwmMotorDireito2);
        analogWrite(sentidoMotorDireito, pwmMotorDireito1);
        analogWrite(velocidadeMotorDireito, pwmMotorDireito2);

        Serial.println();

        Serial.print("PWM Esquerdo 1: ");
        Serial.println(pwmMotorEsquerdo1);
        Serial.print("PWM Esquerdo 2: ");
        Serial.println(pwmMotorEsquerdo2);
        analogWrite(sentidoMotorEsquerdo, pwmMotorEsquerdo1);
        analogWrite(velocidadeMotorEsquerdo, pwmMotorEsquerdo2);
}

/* ----------------------------- Modo gestos ----------------------------- */

// Arma pelos gestos: sinal = sentido (positivo = mesmo sentido da BOLINHA),
// módulo = velocidade (0..100%). Mesmos pares de pinos da lógica oficial da arma.
void armaGestos(int8_t arma) {
  int pwm = map(constrain(abs(arma), 0, 100), 0, 100, MIN_PWM, MAX_PWM);

  if (arma > 0) {
    analogWrite(PINO_1_ARMA1, 0);
    analogWrite(PINO_2_ARMA1, pwm);
    analogWrite(PINO_1_ARMA2, pwm);
    analogWrite(PINO_2_ARMA2, 0);
  } else if (arma < 0) {
    analogWrite(PINO_1_ARMA1, pwm);
    analogWrite(PINO_2_ARMA1, 0);
    analogWrite(PINO_1_ARMA2, 0);
    analogWrite(PINO_2_ARMA2, pwm);
  } else {
    analogWrite(PINO_1_ARMA1, 0);
    analogWrite(PINO_2_ARMA1, 0);
    analogWrite(PINO_1_ARMA2, 0);
    analogWrite(PINO_2_ARMA2, 0);
  }
}

// Converte o último comando dos gestos (-100..100) pra escala do analógico e aplica.
// Sem pacote recente, lerModoMao() já zerou o comando: tudo para.
void aplicaGestos() {
  int32_t valorAnalogicoV = map(-constrain(cmdMao.aceleracao, -100, 100), -100, 100,
                                MIN_JOYSTICK_Y, MAX_JOYSTICK_Y);  // frente = negativo
  int32_t valorAnalogicoH = map(constrain(cmdMao.direcao, -100, 100), -100, 100,
                                MIN_JOYSTICK_X, MAX_JOYSTICK_X);  // direita = positivo
  aplicaMovimento(valorAnalogicoV * LIMITE_GESTOS_FRENTE / 100,
                  valorAnalogicoH * LIMITE_GESTOS_GIRO / 100);
  armaGestos(cmdMao.arma);
}

void processControllers() {
  for (auto myController : myControllers) {

    if (myController && myController->isConnected()
        && myController->hasData() && myController->isGamepad()) {

      /* A partir daqui inicia-se a lógica de funcionamento do robô.
         Qualquer alteração / nova implementação deve ser feita aqui. */

      // Se SELECT for presionado, desliga robô.
      if (myController->miscSelect()) {
        roboLigado = false;
        Serial.println("Robo desligado.");
      }

      // Se START for presionado, liga robô.
      else if (myController->miscStart()) {
        roboLigado = true;
        Serial.println("Robo ligado.");
      }

      if (roboLigado) {

        /* ---------------------- Escolha do modo: R1 gestos, L1 controle ---------------------- */

        if (myController->r1() && !modoGestos) {
          modoGestos = true;
          paraMotores();
          Serial.println("Modo GESTOS (R1).");
        } else if (myController->l1() && modoGestos) {
          modoGestos = false;
          paraMotores();
          digitalWrite(PINO_LED_INTERNO, configsTravadas);
          Serial.println("Modo CONTROLE (L1).");
        }

        /* --------------------- Lógica de funcionamento da arma --------------------- */
        // No modo gestos a arma é comandada pelos gestos, não pelos botões.

        // Se BOLINHA for pressionado, roda arma para um lado.
        if (!modoGestos && myController->b()) {
          Serial.print("Arma Sentido 1\n");
          analogWrite(PINO_1_ARMA1, 0);
          analogWrite(PINO_2_ARMA1, MAX_PWM);
          analogWrite(PINO_1_ARMA2, MAX_PWM);
          analogWrite(PINO_2_ARMA2, 0);
        }

        // Se QUADRADO for pressionado, roda arma para o outro lado.
        if (!modoGestos && myController->x()) {
          Serial.print("Arma Sentido 2\n");
          analogWrite(PINO_1_ARMA1, MAX_PWM);
          analogWrite(PINO_2_ARMA1, 0);
          analogWrite(PINO_1_ARMA2, 0);
          analogWrite(PINO_2_ARMA2, MAX_PWM);
        }

        // Se TRIÂNGULO for presionado, desliga motores da arma.
        if (!modoGestos && myController->y()) {
          Serial.print("Arma desligada\n");
          analogWrite(PINO_1_ARMA1, 0);
          analogWrite(PINO_2_ARMA1, 0);
          analogWrite(PINO_1_ARMA2, 0);
          analogWrite(PINO_2_ARMA2, 0);
        }

        /* ----------------- Lógica de trava das configurações ----------------- */

        if (myController->thumbL() && myController->thumbR()) {
          configsTravadas = !configsTravadas;
          digitalWrite(PINO_LED_INTERNO, configsTravadas);
          Serial.println(configsTravadas);
        }

        // Trava pra evitar de alterar as configurações do controle durante a partida
        if (!configsTravadas) {

          /* ----------------- Lógica de inversão de giro da movimentação ----------------- */

          uint8_t leituraSetinhas = myController->dpad();

          // Cada SETINHA representa uma configuração de pinos para os motores
          switch (leituraSetinhas) {
            case 0x01:  // cima
              sentidoMotorEsquerdo = PINO_1_MOTOR_ESQUERDO, velocidadeMotorEsquerdo = PINO_2_MOTOR_ESQUERDO;
              sentidoMotorDireito = PINO_1_MOTOR_DIREITO, velocidadeMotorDireito = PINO_2_MOTOR_DIREITO;
              break;
            case 0x02:  // baixo
              sentidoMotorEsquerdo = PINO_1_MOTOR_ESQUERDO, velocidadeMotorEsquerdo = PINO_2_MOTOR_ESQUERDO;
              sentidoMotorDireito = PINO_2_MOTOR_DIREITO, velocidadeMotorDireito = PINO_1_MOTOR_DIREITO;
              break;
            case 0x04:  // direita
              sentidoMotorEsquerdo = PINO_2_MOTOR_ESQUERDO, velocidadeMotorEsquerdo = PINO_1_MOTOR_ESQUERDO;
              sentidoMotorDireito = PINO_1_MOTOR_DIREITO, velocidadeMotorDireito = PINO_2_MOTOR_DIREITO;
              break;
            case 0x08:  // esquerda
              sentidoMotorEsquerdo = PINO_2_MOTOR_ESQUERDO, velocidadeMotorEsquerdo = PINO_1_MOTOR_ESQUERDO;
              sentidoMotorDireito = PINO_2_MOTOR_DIREITO, velocidadeMotorDireito = PINO_1_MOTOR_DIREITO;
              break;
          }
        }

        /* ----------------- Movimentação pelo controle (só no modo controle) ----------------- */

        if (!modoGestos) {
          // R2 = frente, L2 = trás. Os gatilhos vão de 0 a 1023 e são convertidos pra mesma
          // escala do analógico vertical (frente = negativo), assim o resto da lógica fica igual.
          int32_t gatilhoFrente = myController->throttle();  // R2
          int32_t gatilhoTras = myController->brake();       // L2

          // Se o controle só mandar o botão do gatilho (sem o valor analógico), apertado vale 100%.
          if (gatilhoFrente == 0 && myController->r2()) gatilhoFrente = MAX_GATILHO;
          if (gatilhoTras == 0 && myController->l2()) gatilhoTras = MAX_GATILHO;

          int32_t valorAnalogicoV = map(gatilhoTras - gatilhoFrente,
                                        MIN_GATILHO - MAX_GATILHO, MAX_GATILHO - MIN_GATILHO,
                                        MIN_JOYSTICK_Y, MAX_JOYSTICK_Y);

          // Lê valor em X do analógico direito (R-right) = direção.
          int32_t valorAnalogicoH = myController->axisRX();

          aplicaMovimento(valorAnalogicoV * LIMITE_CONTROLE_FRENTE / 100,
                          valorAnalogicoH * LIMITE_CONTROLE_GIRO / 100);
        }
      }

      else
        desligaRobo();
    }
  }
}

void setup() {

  Serial.begin(115200);

  // Inicia comunicação Bluetooth que permite a conexão com controle.
  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.enableVirtualDevice(false);

  // Desparea os controles que haviam sido conectados anteriormente.
  // BP32.forgetBluetoothKeys();

  // Rede Wi-Fi do modo gestos (RoboBatalha, 192.168.4.1, UDP 4210).
  iniciarModoMao();

  pinMode(PINO_LED_INTERNO, OUTPUT);

  // Configura pinos da ESP32 para controle dos motores de arma.
  pinMode(PINO_1_ARMA1, OUTPUT);
  pinMode(PINO_2_ARMA1, OUTPUT);

  pinMode(PINO_1_ARMA2, OUTPUT);
  pinMode(PINO_2_ARMA2, OUTPUT);

  // Configura pinos da ESP32 para controle dos motores de locomoção.
  pinMode(PINO_1_MOTOR_ESQUERDO, OUTPUT);
  pinMode(PINO_2_MOTOR_ESQUERDO, OUTPUT);

  pinMode(PINO_1_MOTOR_DIREITO, OUTPUT);
  pinMode(PINO_2_MOTOR_DIREITO, OUTPUT);

  // Sentido padrão = mesmo da SETA PARA BAIXO (testado no Bertos: R2 anda reto pra frente).
  sentidoMotorEsquerdo = PINO_1_MOTOR_ESQUERDO, velocidadeMotorEsquerdo = PINO_2_MOTOR_ESQUERDO;
  sentidoMotorDireito = PINO_2_MOTOR_DIREITO, velocidadeMotorDireito = PINO_1_MOTOR_DIREITO;

  configsTravadas = false;

  // Desliga movimentação e arma do robô.
  desligaRobo();

  // Desliga LED de indicação de trava
  digitalWrite(PINO_LED_INTERNO, LOW);
}

void loop() {
  // Checa se houve atualização nos dados do controle
  bool dataUpdated = BP32.update();

  // Se sim, chama a função processControllers() para processar os dados
  if (dataUpdated)
    processControllers();

  // Lê os pacotes dos gestos sempre (não deixa acumular), mas só usa no modo gestos.
  bool recebendoGestos = lerModoMao();
  statusModoMao(modoGestos);

  if (roboLigado && modoGestos) {
    // O PC manda ~30 pacotes/s; aplicar a cada 30 ms evita encher o serial com os
    // prints da lógica oficial a cada volta do loop.
    static uint32_t ultimoGesto = 0;
    if (millis() - ultimoGesto >= PERIODO_GESTOS_MS) {
      ultimoGesto = millis();
      aplicaGestos();
    }
    digitalWrite(PINO_LED_INTERNO, (millis() / (recebendoGestos ? 100 : 500)) % 2);
  }
}
