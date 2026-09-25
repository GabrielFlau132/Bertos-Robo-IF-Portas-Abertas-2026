// teste_motores.ino — descobre, sem o PC, o que cada comando mexe no robô
//
// Grave, coloque a ESP32 no robô, ligue a bateria e pareie o controle.
// Robô SUSPENSO (rodas sem tocar no chão) e, se der, arma desacoplada.
//
// Segure uma seta: o canal correspondente gira devagar enquanto ela estiver apertada.
//   cima     = pinos 14/27         baixo    = pinos 25/26
//   direita  = pinos 33/32         esquerda = pinos 13/12 (força reduzida)
//   segurando qualquer um de X / O / quadrado / triângulo junto = gira ao contrário
// Pinos de cada borne da placa: ESQUERDO 14/27 (cima), DIREITO 25/26 (baixo),
// ARMA1 33/32 (direita), ARMA2 13/12 (esquerda).
// Usado em 2026-09-24 pra achar uma roda ligada no borne ARMA1 com o ESQUERDO vazio
// (ligação corrigida depois).
//
// Gatilhos: R2 aciona os canais 14/27 e 25/26 pra um lado, L2 pro outro,
// proporcional ao quanto aperta (se entrar sempre na mesma força, o controle só
// manda o botão, não o valor analógico).
//
// LED azul da placa: piscando = esperando controle | aceso = controle conectado.
//
// Nomes dos pinos seguem o robo_inicativa_feiras do cupim (não o Bertos): serve
// justamente pra descobrir o que está ligado em cada canal.

#include <Bluepad32.h>

#define PINO_LED_INTERNO 2

#define PINO_1_MOTOR_ESQUERDO 14
#define PINO_2_MOTOR_ESQUERDO 27
#define PINO_1_MOTOR_DIREITO 25
#define PINO_2_MOTOR_DIREITO 26
#define PINO_1_ARMA1 33
#define PINO_2_ARMA1 32
#define PINO_1_ARMA2 13
#define PINO_2_ARMA2 12

const int PWM_TESTE = 160;       // locomoção nas setas (de 255)
const int PWM_TESTE_ARMA = 110;  // arma mais fraca, por segurança

const uint8_t PINOS[] = { PINO_1_MOTOR_ESQUERDO, PINO_2_MOTOR_ESQUERDO, PINO_1_MOTOR_DIREITO,
                          PINO_2_MOTOR_DIREITO, PINO_1_ARMA1, PINO_2_ARMA1, PINO_1_ARMA2, PINO_2_ARMA2 };

ControllerPtr controle = nullptr;

void pararTudo() {
  for (uint8_t p : PINOS) analogWrite(p, 0);
}

// velocidade com sinal: + = pino 1, - = pino 2
void gira(uint8_t pino1, uint8_t pino2, int velocidade) {
  analogWrite(pino1, velocidade > 0 ? velocidade : 0);
  analogWrite(pino2, velocidade < 0 ? -velocidade : 0);
}

void onConnectedController(ControllerPtr ctl) {
  if (controle == nullptr) {
    controle = ctl;
    Serial.println("Controle conectado.");
  }
}

void onDisconnectedController(ControllerPtr ctl) {
  if (ctl == controle) {
    controle = nullptr;
    pararTudo();
    Serial.println("Controle desconectado.");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PINO_LED_INTERNO, OUTPUT);
  for (uint8_t p : PINOS) pinMode(p, OUTPUT);
  pararTudo();

  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.enableVirtualDevice(false);
  BP32.forgetBluetoothKeys();
}

void loop() {
  BP32.update();
  digitalWrite(PINO_LED_INTERNO, controle ? HIGH : (millis() / 300) % 2);

  if (controle && controle->isConnected()) {
    int sentido = (controle->a() || controle->b() || controle->x() || controle->y()) ? -1 : 1;
    uint8_t setas = controle->dpad();

    int esq = 0, dir = 0, a1 = 0, a2 = 0;
    if (setas & 0x01) esq = PWM_TESTE * sentido;       // cima
    if (setas & 0x02) dir = PWM_TESTE * sentido;       // baixo
    if (setas & 0x04) a1 = PWM_TESTE_ARMA * sentido;   // direita
    if (setas & 0x08) a2 = PWM_TESTE_ARMA * sentido;   // esquerda

    // gatilhos: analógico se o controle mandar, senão o botão vale PWM_TESTE
    int r2 = map(controle->throttle(), 0, 1023, 0, PWM_TESTE);
    int l2 = map(controle->brake(), 0, 1023, 0, PWM_TESTE);
    if (r2 < 10 && controle->r2()) r2 = PWM_TESTE;
    if (l2 < 10 && controle->l2()) l2 = PWM_TESTE;
    if (r2 >= 10 || l2 >= 10) esq = dir = r2 - l2;

    gira(PINO_1_MOTOR_ESQUERDO, PINO_2_MOTOR_ESQUERDO, esq);
    gira(PINO_1_MOTOR_DIREITO, PINO_2_MOTOR_DIREITO, dir);
    gira(PINO_1_ARMA1, PINO_2_ARMA1, a1);
    gira(PINO_1_ARMA2, PINO_2_ARMA2, a2);

    static uint32_t ultimoLog = 0;
    if (millis() - ultimoLog > 300) {
      ultimoLog = millis();
      Serial.printf("setas=0x%02x botoes=0x%04x R2=%d L2=%d r2btn=%d l2btn=%d | E=%d D=%d A1=%d A2=%d\n",
                    setas, controle->buttons(), controle->throttle(), controle->brake(),
                    controle->r2(), controle->l2(), esq, dir, a1, a2);
    }
  }
  delay(1);
}
