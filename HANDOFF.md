# Handoff — Robô Bertos (IF Portas Abertas 2026)

Estado em **2026-09-24**. Leia inteiro antes de mexer em qualquer coisa.

## Objetivo
Robô de batalha (categoria cupim) pilotado de **dois jeitos, um de cada vez**:
1. **Controle de PS3** (Bluepad32 na ESP32) — **funcionando**.
2. **Gestos dos braços** por webcam (Python + MediaPipe) — reconhecimento funcionando **só no simulador**, ainda não conectado ao robô.

A escolha do modo será feita **pelo controle, com R1 e L1** (pedido do usuário). Ainda não implementado — ver "Próxima fase".

## Resumo do estado
| Parte | Estado |
|---|---|
| Firmware do robô com controle | ✅ funcionando no robô (`firmware/codigo_robo_controle_p3`) |
| Motores / placa | ✅ motores religados nos bornes certos |
| Gestos (Python) | ✅ no simulador · ❌ não envia nada pro robô (`ENVIAR_UDP = False`) |
| `modo_mao.h` (UDP na ESP32) | ❌ não integrado ao firmware |
| Integração controle + gestos | ⏳ **aguardando confirmação do usuário pra começar** |

---

## Base oficial (muito importante)
Tudo do robô vem de **https://github.com/nrc-cupim/start-automacao-eletrica** (kit "start_Engenharia Automação e Elétrica", Inatel), commit `a52bbf3`.
- **Não usar** `nrc-cupim/cupim_start_inatel` — foi mandado por engano no começo e gerou confusão.
- O usuário quer o firmware **o mais fiel possível ao código oficial**: mudanças mínimas, sempre explicadas no topo do arquivo.
- Documentação oficial útil: `Apostilas/apostila_completa_firmware.pdf`, `Apostilas/apostila_completa_hardware.pdf`, `Codigos/funcoes-controle.png` (mapa dos botões oficial).
- Procedimento oficial: gravar `filtro_mac_controle` (opcional, trava a ESP32 num controle) → gravar o código principal.

## Hardware
- Placa **START_AUTOMACAO_ELETRICA**: ESP32 WROOM-32 **DevKit v1** + 2× **DRV8833** + regulador 7805 (bateria → 5 V → VIN da ESP32; bateria direto no VCC dos DRV).
- Bornes serigrafados na placa (conferido no esquemático do repositório oficial):

  | Borne | Pinos ESP32 | No código |
  |---|---|---|
  | ESQUERDO | 27 / 14 | `PINO_x_MOTOR_ESQUERDO` |
  | ARMA2 | 12 / 13 | `PINO_x_ARMA2` |
  | ARMA1 | 32 / 33 | `PINO_x_ARMA1` |
  | DIREITO | 25 / 26 | `PINO_x_MOTOR_DIREITO` |

- LED azul da ESP32 (GPIO 2) = trava das setas ligada.
- Histórico: numa tentativa o robô não andava porque uma roda estava no borne ARMA1 e o ESQUERDO estava vazio. O usuário religou; hoje está certo.
- **A ESP32 não fica ligada no PC e no robô ao mesmo tempo.** Grava no PC (USB) → leva pro robô → testa. Não há monitor serial com o robô rodando; diagnóstico no robô é pelo que o usuário observa.
- Windows precisa do driver **CP210x** (sem ele: erro 28 no Gerenciador de Dispositivos). Nesta máquina a ESP32 aparece na **COM11**.

## Controle
- **PS3 paralelo**, MAC `98:B6:37:E7:3E:F7`. O Bluepad32 o reconhece como **controle HID genérico** (não como DS3): pareia pelo Bluetooth comum, **não precisa de SixaxisPairTool**. Ligado no PC por USB ele aparece como "Xbox 360" — irrelevante.
- Leituras medidas com `descobrir_parametros_controle`:

  | Comando | Leitura na Bluepad32 |
  |---|---|
  | R2 | `throttle()` 0 ou 1020 — **só liga/desliga**, sem valor intermediário (bit 0x0080) |
  | L2 | `brake()` 0 ou 1020 — só liga/desliga (bit 0x0040) |
  | Analógico direito → direita / esquerda | `axisRX()` +508 / −512 |
  | Analógico direito → cima / baixo | `axisRY()` −512 / +508 |

- **Pareamento:** gravar `descobrir_parametros_controle` (ele faz `forgetBluetoothKeys()`), colocar o controle em modo de pareamento, esperar "controle conectado" no serial, depois gravar o código principal. O código principal não apaga as chaves, então o controle reconecta sozinho depois.

## Firmware em uso: `firmware/codigo_robo_controle_p3`
Código oficial com **duas** mudanças (listadas no topo do `.ino`):
1. **R2 = frente, L2 = trás, analógico direito (horizontal) = direção.** Os gatilhos são convertidos pra mesma escala do analógico vertical, então a lógica oficial de mistura dos motores ficou intacta. Se o controle só mandar o botão do gatilho, apertado vale 100%. L1/R1 e o analógico esquerdo ficaram **livres** (a troca de analógicos do oficial foi removida).
2. **Sentido padrão dos motores ao ligar = o da seta pra baixo** (testado no robô: R2 anda reto pra frente).

| Comando | Função |
|---|---|
| START / SELECT | liga / desliga o robô |
| R2 / L2 | frente / trás (100%, o controle não é proporcional) |
| Analógico direito ↔ | direção (sem gatilho, gira no lugar) |
| O / □ | arma num sentido / no outro (100% na hora) |
| △ | desliga a arma |
| Setas | mudam o sentido das rodas (padrão = seta ↓) |
| L3 + R3 | trava/destrava as setas (LED azul) |
| L1, R1, analógico esquerdo | sem função (L1/R1 reservados pra escolha de modo) |

Comportamentos do código oficial que continuam (não foram "consertados" de propósito, pra manter fidelidade):
- A arma vai de 0 a 100% instantaneamente (sem partida suave). Histórico do usuário: partidas bruscas já derrubaram a alimentação e resetaram o rádio numa versão anterior.
- A trava L3+R3 alterna a cada leitura enquanto os botões ficam apertados — às vezes precisa apertar de novo.
- Imprime no serial a cada leitura do controle.
- Pendência aberta com o usuário: ele pediu "acelerar" a arma; ela já está em 100% — perguntar se quer partida suave ou outra coisa.

Comparação feita no PC (bibliotecas falsas): com os mesmos comandos, os PWMs das rodas saem iguais aos do código oficial (diferença de 1 ponto de PWM em ré a 50%, arredondamento).

### Gravar
```bash
arduino-cli compile --fqbn esp32-bluepad32:esp32:esp32 firmware/codigo_robo_controle_p3
arduino-cli upload -p COM11 --fqbn esp32-bluepad32:esp32:esp32 firmware/codigo_robo_controle_p3
```
Core: `esp32-bluepad32:esp32` 4.1.0 (URLs de placa no `README.md`). Se travar em `Connecting...`, segurar o botão BOOT.

### Outras pastas em `firmware/`
Ver `firmware/README.md`. Resumo: 3 utilitários oficiais (idênticos ao repositório), `teste_motores` (diagnóstico, feito aqui) e `bertos_controle` (**obsoleto**: reescrita com rampa/tempo morto, substituída pelo oficial a pedido do usuário; pinagem dele reflete a ligação errada antiga — não gravar).

---

## Gestos (Python) — `controle_mao.py` + `sim_robo.py`
Ambiente: `venv` com **Python 3.12** e `requirements.txt` (`mediapipe==0.10.14` — versões novas não têm `mp.solutions`, **não atualizar**). O Python global desta máquina (3.14) não roda o projeto.

Mapeamento de gestos (definido pelo usuário — não mudar):
- **Braço direito (locomoção):** mão aberta = anda, fechada/fora = para; mão pra frente (em direção à câmera) = acelera, pra trás = ré; mão pra direita/esquerda do ombro = curva.
- **Braço esquerdo (arma):** mão aberta = ativa, fechada = para; frente/trás = sentido; ao sair da zona morta já entra em `ARMA_MIN = 70%`.

Como está implementado:
- Frame espelhado. **Cada mão é atribuída ao pulso do Pose mais próximo** (não usa o rótulo Left/Right do Hands, que às vezes rotula as duas iguais). Mãos a mais de `MAO_DIST_MAX = 0.8` larguras de ombro de qualquer pulso são ignoradas (gente atrás do operador) e aparecem cinza.
- **Calibração: tecla `c`** → contagem de 5 s com o robô parado → salva a posição neutra; se faltar mão no fim, espera mais 2 s e diz o que faltou. (A calibração por gesto de X foi removida — não funcionava.)
- Profundidade = tamanho da palma ÷ largura dos ombros; curva = x do pulso direito − ombro direito, em larguras de ombro. EMA `SUAVIZACAO = 0.4`.
- Visual estilo HUD "Tony Stark" (pedido do usuário): sem esqueleto do corpo nem da mão; repulsor brilhando na palma (apagado = mão fechada), marcas nas pontas dos dedos, rótulos com os valores, anel grande de contagem na calibração, brilho neon, painel com canto cortado. O usuário pediu pra **tirar os anéis em volta das mãos** — não recolocar.
- Teclas: `c` calibra, `r` reseta o simulador, `q` sai.
- Protocolo UDP (quando ligado): 5 bytes `struct.pack("<BbbbB", 0xAA, acel, dire, arma, seq)`, valores −100..100, 30x/s, pra `192.168.4.1:4210`.

Problemas conhecidos (não resolvidos):
- **Ré quase inalcançável e aceleração máxima difícil:** o tamanho da palma varia com 1/distância; a ~2 m da câmera, 100% de ré exigiria recuar a mão ~1,6 m. Sugestão: usar `log(r/cal)` e diminuir `PROF_DZ`/`PROF_ALCANCE`.
- Aberta/fechada sem histerese (pode piscar e dar trancos).
- `sock.sendto` sem `try/except` (queda de Wi-Fi derruba o programa); `cap.isOpened()` não é checado.

## `modo_mao.h` (lado ESP32, não integrado)
softAP `RoboBatalha` / `12345678`, UDP 4210, pacote de 5 bytes começando com `0xAA`, zera o comando se ficar 300 ms sem pacote (`lerModoMao()` retorna `false`). Atenção: senha fraca e até 4 conexões — em evento aberto, qualquer celular na rede poderia mandar pacotes; considerar `WiFi.softAP(ssid, senha, 1, 0, 1)` e senha melhor.

---

## Próxima fase: integração (NÃO começar sem confirmação do usuário)
Requisito do usuário: o robô é pilotado **pelo controle OU pelos gestos, um de cada vez**, e **o modo é escolhido no controle com R1 e L1** (qual botão é qual modo ainda não foi definido — perguntar).

Pontos a resolver/decidir:
1. **Controle continua sendo o "dono":** START/SELECT e o failsafe de desconexão valem nos dois modos; se o controle desconectar, o robô para mesmo no modo gestos.
2. **Wi-Fi (softAP) + Bluetooth no mesmo rádio da ESP32:** testar primeiro se o controle continua responsivo com o Wi-Fi ligado. Não se sabe ainda.
3. **No modo gestos:** converter `acel`/`dire` (−100..100) para `valorAnalogicoV`/`valorAnalogicoH` e reaproveitar a mistura oficial (frente = V negativo). Sem pacote UDP por 300 ms → motores parados.
4. **Arma no modo gestos:** o gesto dá velocidade e sentido (−100..100); no controle é liga/desliga 100%. Decidir se o gesto controla a arma e com que limite; considerar partida suave (histórico de quedas de alimentação).
5. **Python:** `ENVIAR_UDP = True`; o notebook precisa entrar na rede `RoboBatalha` (fica sem internet).
6. Mudar o firmware oficial o mínimo possível e registrar as mudanças no topo do `.ino`.
7. Testar sempre com o robô suspenso antes do chão.

## Regras de trabalho do usuário
- Mudanças incrementais; preservar o que funciona; fiel ao repositório oficial.
- Respostas diretas e técnicas, com passos acionáveis.
- Não inventar comportamento de lib ou hardware; se não souber, dizer (e medir: `descobrir_parametros_controle`, testes no PC).
- O usuário testa (webcam, robô) e reporta; **o usuário faz os commits** (Claude sugere a mensagem).
