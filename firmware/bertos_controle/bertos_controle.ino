// bertos_controle.ino — robô Bertos pilotado pelo controle de PS4 (Bluepad32)
//
// *** OBSOLETO — NÃO GRAVAR. O firmware em uso é ../codigo_robo_controle_p3 (código
// oficial). A pinagem daqui reflete uma ligação errada dos motores já corrigida.
// Mantido só como referência da rampa/tempo morto. Ver ../README.md ***
//
// Base: nrc-cupim/cupim_start_inatel (robo_inicativa_feiras), mesma placa; os motores
// do Bertos estão em saídas diferentes (mapa em parametros.h).
// Mudanças em relação ao original:
//   - R2 acelera e L2 dá ré (proporcional); analógico esquerdo faz a curva
//   - arma no analógico direito (cima = um sentido, baixo = outro, proporcional)
//   - rampa (soft-start) e tempo morto na inversão em todos os motores
//   - L3 + R3 alterna a trava uma vez por toque (antes alternava a cada leitura
//     enquanto os botões ficavam apertados, e a trava parava num estado aleatório)
//   - só o primeiro controle conectado pilota; os outros são ignorados
//   - log no serial limitado a 5x/s (imprimir a cada leitura atrasava o loop)
//
// Comandos:
//   OPTIONS (START) = liga o robô         SHARE (SELECT) = desliga (para tudo na hora)
//   R2 = acelera | L2 = ré                analógico esquerdo (X) = curva
//   analógico direito (Y) = arma
//   setas = corrigem o sentido dos motores de locomoção, se algum rodar ao contrário:
//     cima = normal | baixo = inverte o direito | direita = inverte o esquerdo | esquerda = inverte os dois
//   L3 + R3 (apertar os dois analógicos) = trava/destrava as setas
//   LED azul da placa: aceso enquanto a ESP32 recebe comando de movimento ou de arma
//
// Luz do controle: PS4 vermelha = robô desligado, verde = ligado.
//                  PS3 só o LED 1 = desligado, os 4 LEDs = ligado.
// Controle usado no evento (PS3 paralelo, MAC 98:B6:37:E7:3E:F7): pareia pelo
// Bluetooth comum, é só colocar em modo de pareamento com a ESP32 ligada.
// O robô sempre começa desligado e desliga sozinho se o controle desconectar.

#include <Bluepad32.h>
#include "parametros.h"

struct Motor {
  uint8_t pino1, pino2;
  int pwmMax;
  uint16_t rampaMs;
  uint16_t tempoMortoMs;
  bool invertido;
  float atual;            // -1..1 aplicado agora
  int8_t ultimoSentido;   // sentido do último giro: -1, 0 ou 1
  uint32_t girandoAte;    // último instante (ms) em que estava girando
};

Motor motorEsquerdo = { PINO_1_MOTOR_ESQUERDO, PINO_2_MOTOR_ESQUERDO, MAX_PWM_LOCOMOCAO,
                        RAMPA_LOCOMOCAO_MS, TEMPO_MORTO_LOCOMOCAO_MS, false, 0, 0, 0 };
Motor motorDireito = { PINO_1_MOTOR_DIREITO, PINO_2_MOTOR_DIREITO, MAX_PWM_LOCOMOCAO,
                       RAMPA_LOCOMOCAO_MS, TEMPO_MORTO_LOCOMOCAO_MS, false, 0, 0, 0 };
Motor arma1 = { PINO_1_ARMA1, PINO_2_ARMA1, MAX_PWM_ARMA,
                RAMPA_ARMA_MS, TEMPO_MORTO_ARMA_MS, false, 0, 0, 0 };
Motor arma2 = { PINO_1_ARMA2, PINO_2_ARMA2, MAX_PWM_ARMA,
                RAMPA_ARMA_MS, TEMPO_MORTO_ARMA_MS, ARMA2_INVERTIDA, 0, 0, 0 };

ControllerPtr controle = nullptr;
bool roboLigado = false, configsTravadas = false, travaAntes = false;
bool atualizarCorControle = false;
float alvoEsquerdo = 0, alvoDireito = 0, alvoArma = 0;  // -1..1
uint32_t ultimoLoop = 0, ultimoLog = 0;

/* ------------------------------ Motores ------------------------------ */

// Converte leitura do controle em -1..1 com zona morta (sem salto na borda dela).
float normaliza(int32_t valor, int32_t zonaMorta, int32_t maximo) {
  int32_t modulo = abs(valor);
  if (modulo <= zonaMorta) return 0;
  float x = (float)(modulo - zonaMorta) / (maximo - zonaMorta);
  if (x > 1) x = 1;
  return valor > 0 ? x : -x;
}

// Leva o motor em direção ao alvo respeitando rampa e tempo morto.
void atualizaMotor(Motor &m, float alvo, uint32_t agora, uint32_t dt) {
  // Pediu o sentido contrário: primeiro para (roda livre) e só depois inverte.
  if (m.atual * alvo < 0) alvo = 0;

  if (fabsf(alvo) <= fabsf(m.atual)) {
    m.atual = alvo;  // reduzir é imediato
  } else {
    int8_t sentido = alvo > 0 ? 1 : -1;
    bool esperandoInverter = m.atual == 0 && sentido != m.ultimoSentido
                             && agora - m.girandoAte < m.tempoMortoMs;
    if (!esperandoInverter) {
      float passo = (float)dt / m.rampaMs;
      m.atual += constrain(alvo - m.atual, -passo, passo);
    }
  }

  if (m.atual != 0) {
    m.ultimoSentido = m.atual > 0 ? 1 : -1;
    m.girandoAte = agora;
  }
}

// DRV8833: um pino com PWM e o outro em 0 (roda livre quando o PWM cai).
void aplicaMotor(const Motor &m) {
  float v = m.invertido ? -m.atual : m.atual;
  int pwm = (int)(fabsf(v) * m.pwmMax + 0.5f);
  if (v > 0) {
    analogWrite(m.pino1, pwm);
    analogWrite(m.pino2, 0);
  } else if (v < 0) {
    analogWrite(m.pino1, 0);
    analogWrite(m.pino2, pwm);
  } else {
    analogWrite(m.pino1, 0);
    analogWrite(m.pino2, 0);
  }
}

// Troca o sentido de um motor de locomoção pelas setas. Se mudou, corta o motor e
// força o tempo morto antes de ele voltar a girar (senão inverteria em movimento).
void defineInvertido(Motor &m, bool invertido) {
  if (m.invertido == invertido) return;
  Serial.printf("[%lu] Setas: motor %s agora %s\n", (unsigned long)millis(),
                &m == &motorEsquerdo ? "esquerdo" : "direito", invertido ? "INVERTIDO" : "normal");
  m.invertido = invertido;
  m.atual = 0;
  m.ultimoSentido = 2;  // valor que nunca bate com um sentido: obriga o tempo morto
  m.girandoAte = millis();
}

// Corte imediato de tudo (sem rampa).
void pararTudo() {
  alvoEsquerdo = alvoDireito = alvoArma = 0;
  Motor *motores[] = { &motorEsquerdo, &motorDireito, &arma1, &arma2 };
  for (Motor *m : motores) {
    m->atual = 0;
    aplicaMotor(*m);
  }
}

void desligaRobo() {
  if (roboLigado) atualizarCorControle = true;
  roboLigado = false;
  pararTudo();
  digitalWrite(PINO_LED_INTERNO, LOW);
}

/* ------------------------------ Controle ------------------------------ */

void onConnectedController(ControllerPtr ctl) {
  if (controle == nullptr) {
    controle = ctl;
    atualizarCorControle = true;
    const uint8_t *mac = ctl->getProperties().btaddr;
    Serial.printf("AVISO: controle conectado. MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  } else {
    Serial.println("AVISO: outro controle conectou e sera ignorado (so o primeiro pilota).");
  }
}

void onDisconnectedController(ControllerPtr ctl) {
  if (ctl == controle) {
    Serial.println("AVISO: controle desconectado. Robo desligado.");
    controle = nullptr;
    travaAntes = false;
    desligaRobo();
  }
}

void processaControle() {
  if (!(controle && controle->isConnected() && controle->hasData() && controle->isGamepad()))
    return;

  /* ----------------------- Liga / desliga ----------------------- */

  if (controle->miscSelect()) {  // SHARE
    if (roboLigado) Serial.println("Robo desligado.");
    desligaRobo();
  } else if (controle->miscStart() && !roboLigado) {  // OPTIONS
    roboLigado = true;
    atualizarCorControle = true;
    Serial.println("Robo ligado.");
  }

  if (atualizarCorControle && USAR_LUZ_CONTROLE) {
    // PS4: barra de luz | PS3: LEDs de jogador (1 aceso = desligado, 4 acesos = ligado)
    if (roboLigado) {
      controle->setColorLED(0, 255, 0);
      controle->setPlayerLEDs(0x0F);
    } else {
      controle->setColorLED(255, 0, 0);
      controle->setPlayerLEDs(0x01);
    }
    atualizarCorControle = false;
  }

  /* -------------- Trava das configurações (L3 + R3) -------------- */

  bool trava = controle->thumbL() && controle->thumbR();
  if (trava && !travaAntes) {  // só na hora que aperta, não enquanto segura
    configsTravadas = !configsTravadas;
    Serial.println(configsTravadas ? "Setas travadas." : "Setas destravadas.");
  }
  travaAntes = trava;

  /* ------------ Correção do sentido da locomoção (setas) ------------ */

  if (!configsTravadas) {
    switch (controle->dpad()) {
      case 0x01:  // cima
        defineInvertido(motorEsquerdo, false);
        defineInvertido(motorDireito, false);
        break;
      case 0x02:  // baixo
        defineInvertido(motorEsquerdo, false);
        defineInvertido(motorDireito, true);
        break;
      case 0x04:  // direita
        defineInvertido(motorEsquerdo, true);
        defineInvertido(motorDireito, false);
        break;
      case 0x08:  // esquerda
        defineInvertido(motorEsquerdo, true);
        defineInvertido(motorDireito, true);
        break;
    }
  }

  if (!roboLigado) return;

  /* ------------------------- Locomoção ------------------------- */

  // Gatilho analógico; se o controle só mandar o botão (controle genérico com
  // throttle/brake sempre 0), o botão apertado vale 100%.
  float r2 = normaliza(controle->throttle(), ZONA_MORTA_GATILHO, MAX_GATILHO);
  float l2 = normaliza(controle->brake(), ZONA_MORTA_GATILHO, MAX_GATILHO);
  if (r2 == 0 && controle->r2()) r2 = 1;
  if (l2 == 0 && controle->l2()) l2 = 1;
  float acel = r2 - l2;
  float curva = normaliza(controle->axisX(), ZONA_MORTA_JOYSTICK, MAX_JOYSTICK);

  // Mistura tipo tanque; sem acelerar, a curva gira o robô no lugar.
  float esq = acel + curva;
  float dir = acel - curva;
  float maior = fmaxf(fabsf(esq), fabsf(dir));
  if (maior > 1) {  // mantém a proporção entre os lados em vez de cortar um deles
    esq /= maior;
    dir /= maior;
  }
  alvoEsquerdo = esq;
  alvoDireito = dir;

  /* ---------------------------- Arma ---------------------------- */

  // Y do analógico direito: cima é negativo no Bluepad32, então inverte o sinal.
  alvoArma = -normaliza(controle->axisRY(), ZONA_MORTA_JOYSTICK, MAX_JOYSTICK);

  // LED da placa acende no mesmo instante em que o comando chega: comparando com a
  // hora em que o motor reage dá pra separar atraso do rádio de problema de energia.
  digitalWrite(PINO_LED_INTERNO, alvoEsquerdo != 0 || alvoDireito != 0 || alvoArma != 0);
}

/* ------------------------------ Setup / loop ------------------------------ */

void setup() {
  Serial.begin(115200);

  pinMode(PINO_LED_INTERNO, OUTPUT);
  digitalWrite(PINO_LED_INTERNO, LOW);

  Motor *motores[] = { &motorEsquerdo, &motorDireito, &arma1, &arma2 };
  for (Motor *m : motores) {
    pinMode(m->pino1, OUTPUT);
    pinMode(m->pino2, OUTPUT);
  }
  desligaRobo();

  // Inicia o Bluetooth que permite a conexão com o controle.
  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.enableVirtualDevice(false);

  // Esquece pareamentos antigos a cada boot (igual ao descobrir_parametros_controle
  // do cupim, que conectou esse controle). Sem isso, chaves velhas salvas na ESP32
  // podem impedir o controle de parear de novo.
  BP32.forgetBluetoothKeys();

  ultimoLoop = millis();
}

void loop() {
  if (BP32.update())
    processaControle();

  uint32_t agora = millis();
  uint32_t dt = agora - ultimoLoop;
  ultimoLoop = agora;

  if (roboLigado) {
    atualizaMotor(motorEsquerdo, alvoEsquerdo, agora, dt);
    atualizaMotor(motorDireito, alvoDireito, agora, dt);
    atualizaMotor(arma1, alvoArma, agora, dt);
    atualizaMotor(arma2, ARMA2_HABILITADA ? alvoArma : 0, agora, dt);
    aplicaMotor(motorEsquerdo);
    aplicaMotor(motorDireito);
    aplicaMotor(arma1);
    aplicaMotor(arma2);
  }

  // Sem controle: avisa no serial a cada 2 s (ajuda a saber se a placa está viva).
  static uint32_t ultimoAviso = 0;
  if (controle == nullptr && agora - ultimoAviso >= 2000) {
    ultimoAviso = agora;
    Serial.println("Aguardando controle (coloque o controle em modo de pareamento).");
  }

#if DEBUG_SERIAL
  if (agora - ultimoLog >= 200) {
    ultimoLog = agora;
    Serial.printf("[%lu] ligado=%d trava=%d | alvo E=%+.2f D=%+.2f A=%+.2f | pwm E=%+4d D=%+4d A=%+4d | inv E=%d D=%d",
                  (unsigned long)agora, roboLigado, configsTravadas, alvoEsquerdo, alvoDireito, alvoArma,
                  (int)(motorEsquerdo.atual * motorEsquerdo.pwmMax),
                  (int)(motorDireito.atual * motorDireito.pwmMax),
                  (int)(arma1.atual * arma1.pwmMax),
                  motorEsquerdo.invertido, motorDireito.invertido);
    if (controle)
      Serial.printf(" | setas=0x%02x botoes=0x%04x R2=%ld L2=%ld X=%ld RY=%ld",
                    controle->dpad(), controle->buttons(), (long)controle->throttle(),
                    (long)controle->brake(), (long)controle->axisX(), (long)controle->axisRY());
    Serial.println();
  }
#endif

  // Cede tempo pras tarefas do sistema (evita disparar o watchdog).
  delay(1);
}
