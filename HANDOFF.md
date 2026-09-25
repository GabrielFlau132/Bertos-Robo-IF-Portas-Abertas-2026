# Handoff — Robô Bertos (IF Portas Abertas 2026)

Estado em **2026-09-25** (véspera da apresentação). Histórico e decisões detalhadas. O guia operacional rápido para a IA é o **`CLAUDE.md`**, e o roteiro do dia é o **`APRESENTACAO.md`**.

## Objetivo
Robô de batalha (categoria cupim) pilotado de **dois jeitos, um de cada vez**, escolhidos no controle:
1. **Controle de PS3** (Bluepad32 na ESP32) — **L1**.
2. **Gestos dos braços** por webcam (Python + MediaPipe, via Wi-Fi/UDP) — **R1**.

## Resumo do estado
| Parte | Estado |
|---|---|
| Firmware do robô (`firmware/codigo_robo_controle_p3`) | ✅ gravado e funcionando no robô |
| Motores / placa | ✅ motores religados nos bornes certos |
| Gestos (Python) | ✅ pilotando o robô de verdade pelo Wi-Fi (`ENVIAR_UDP = True`) |
| Integração controle + gestos (R1/L1) | ✅ **testada no robô pelo usuário em 2026-09-24: "funcionando lindamente"** |
| Wi-Fi + Bluetooth juntos na ESP32 | ✅ sem problema (controle responsivo com a rede ligada) |
| Limite de velocidade | ✅ testado e aprovado pelo usuário (valores em `parametros.h`) |
| Direção por gestos (difícil andar reto) | ✅ Python (zona morta + curva suave) aprovado · ⚠️ **correção do firmware (mistura própria) ainda NÃO gravada na ESP32** — gravar com `scripts\gravar_firmware.ps1` |
| Barra de calibração simples (no lugar do anel animado) | ✅ aprovada ("tá incrível") |
| Paleta roxo + verde (no lugar de vermelho/amarelo) | ✅ aprovada ("perfeito") |
| Preparação pra apresentação | ✅ `CLAUDE.md`, `APRESENTACAO.md`, scripts (`scripts/`, `rodar_gestos.bat`), testes no repositório (`tests/`), `requirements-lock.txt`, `CAMERA` configurável e aviso se a câmera não abrir |

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

- LED azul da ESP32 (GPIO 2) = trava das setas ligada (modo controle) / pisca no modo gestos.
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
Código oficial com estas mudanças (listadas no topo do `.ino`):
1. **R2 = frente, L2 = trás, analógico direito (horizontal) = direção.** Os gatilhos são convertidos pra mesma escala do analógico vertical, então a lógica oficial de mistura dos motores ficou intacta. Se o controle só mandar o botão do gatilho, apertado vale 100%. A troca de analógicos do oficial (L1/R1) foi removida.
2. **Sentido padrão dos motores ao ligar = o da seta pra baixo** (testado no robô: R2 anda reto pra frente).
3. **Modo gestos** (R1 entra, L1 volta; só com o robô ligado):
   - Trocar de modo **para todos os motores** (inclusive a arma ligada pelo O).
   - No modo gestos, as rodas e a arma obedecem aos pacotes UDP do `controle_mao.py` (aplicados a cada 30 ms). O controle continua valendo pra START/SELECT, L1, setas e trava; R2/L2, analógico e O/□/△ são ignorados.
   - Sem pacote por **300 ms** → tudo parado. Controle desconectado → robô desliga (failsafe oficial), mesmo no modo gestos. SELECT/desconexão voltam pro modo controle.
   - Arma pelos gestos: sinal = sentido (positivo = sentido da BOLINHA), módulo = velocidade (0–100%).
   - **LED azul no modo gestos:** pisca devagar (1 Hz) = sem pacotes do PC; rápido (5 Hz) = recebendo. No modo controle continua sendo a trava.
4. A mistura oficial dos motores foi movida para `aplicaMovimento(V, H)`, usada **só no modo controle**. Única adição nela: o limite de velocidade reduz o PWM final.
   - **Descoberta importante:** a mistura oficial só se comporta bem com o comando **no fim do curso** (o analógico/gatilho no máximo). Com comando parcial ela vira pro **lado errado** em curvas leves e "salta" perto do centro (ex.: frente a 60% + mão um pouco pra direita → roda direita de 153 pra 202 → robô vira pra esquerda). Por isso:
     - no modo controle o limite é aplicado **no PWM final** (a mistura recebe o comando cheio, igual ao oficial). Uma primeira versão reduzia o comando antes da mistura e causava esse defeito — corrigido;
     - o modo gestos usa **mistura própria, contínua** (`movimentoGestos`): roda esquerda = frente + curva, direita = frente − curva, com os mesmos pinos/sentidos da oficial (as setas continuam valendo).
5. `modo_mao.h` fica dentro da pasta do sketch; rede aceita **1 aparelho** só.
6. **Limite de velocidade da locomoção por modo** (`LIMITE_*` em `parametros.h`, em % do máximo; aplicado no comando antes de `aplicaMovimento`, então a lógica oficial continua igual). A arma não é limitada.

   | | Frente/ré | Giro (curva e girar no lugar) |
   |---|---|---|
   | Modo controle | `LIMITE_CONTROLE_FRENTE = 80` | `LIMITE_CONTROLE_GIRO = 60` |
   | Modo gestos | `LIMITE_GESTOS_FRENTE = 60` | `LIMITE_GESTOS_GIRO = 40` |

   No controle: andando vale `FRENTE`, girando no lugar vale `GIRO`. Nos gestos: `FRENTE` e `GIRO` são os pesos da aceleração e da curva na mistura (frente + curva cheias podem somar 100% numa roda).
   Conferido no PC: R2 → PWM 204 (80%), R2 + direita leve → vira pra direita (esq 204 / dir 196), girar pelo controle → 153 (60%), gestos frente → 153 (60%), gestos giro → 102 (40%); curva dos gestos contínua e sempre pro lado certo. Com os limites do controle em 100 o modo controle é idêntico ao oficial. O simulador do Python **não** mostra esse limite (ele desenha o comando cheio).

| Comando | Função |
|---|---|
| START / SELECT | liga / desliga o robô |
| **L1 / R1** | **modo controle / modo gestos** |
| R2 / L2 | frente / trás (100%, o controle não é proporcional) — só modo controle |
| Analógico direito ↔ | direção (sem gatilho, gira no lugar) — só modo controle |
| O / □ | arma num sentido / no outro (100% na hora) — só modo controle |
| △ | desliga a arma — só modo controle |
| Setas | mudam o sentido das rodas (padrão = seta ↓) — vale nos dois modos |
| L3 + R3 | trava/destrava as setas (LED azul) |

Tamanho com Wi-Fi + Bluetooth: 1,12 MB (85% da partição de app padrão).

Testes no PC (bibliotecas Arduino/Bluepad32/WiFi falsas, hoje em `tests/firmware/`; rodar tudo com `tests\rodar_testes.ps1`): 35 checagens da integração (troca de modo, gestos = mesmos padrões de motor do controle, arma, timeout de 300 ms, LED, SELECT, desconexão) + comparação com o oficial no modo controle (idêntico, fora 1 ponto de PWM de arredondamento). `controle_mao.py` rodado com câmera falsa mandando pra um receptor local: pacotes de 5 bytes corretos, zero durante a calibração, valores iguais aos do HUD.

Comportamentos do código oficial que continuam (não foram "consertados" de propósito, pra manter fidelidade):
- A arma vai de 0 a 100% instantaneamente (sem partida suave). Histórico do usuário: partidas bruscas já derrubaram a alimentação e resetaram o rádio numa versão anterior.
- A trava L3+R3 alterna a cada leitura enquanto os botões ficam apertados — às vezes precisa apertar de novo.
- Imprime no serial a cada leitura do controle.
- Pendência aberta com o usuário: ele pediu "acelerar" a arma; ela já está em 100% — perguntar se quer partida suave ou outra coisa.
- A lógica oficial imprime o PWM no serial a cada aplicação (no modo gestos, ~33x/s) — normal.

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
- **Calibração: tecla `c`** → contagem de 5 s com o robô parado → salva a posição neutra; se faltar mão no fim, espera mais 2 s e diz o que faltou. (A calibração por gesto de X foi removida — não funcionava.) Mostrada numa **barra de progresso simples** (cartão escuro na parte de baixo, barra verde; roxa pulsando se faltar mão) — o usuário pediu pra trocar o anel animado "tech" por ela.
- Profundidade = tamanho da palma ÷ largura dos ombros; curva = x do pulso direito − ombro direito, em larguras de ombro. EMA `SUAVIZACAO = 0.4`.
- **Direção:** `CURVA_DZ = 0.25`, `CURVA_ALCANCE = 0.8`, `CURVA_EXPO = 2.0` (curva ao quadrado: suave perto do centro). Deslocamento → curva: 0,3 → 0 · 0,4 → 7 · 0,5 → 20 · 0,6 → 40 · 0,7 → 66 · 0,8 → 100. Antes (0.15/0.7 linear) 0,3 já dava 27 — difícil andar reto (junto com o defeito da mistura oficial no firmware).
- **Paleta roxo + verde** (pedido do usuário, no lugar do vermelho/dourado): roxo = acentos, painel e arma; verde = destaque, locomoção e barra de calibração. Cores só nas constantes `COR_*` no topo do `controle_mao.py` (BGR); o `sim_robo.py` usa o mesmo roxo pra arma.
- Visual estilo HUD "Tony Stark" (pedido do usuário): sem esqueleto do corpo nem da mão; repulsor brilhando na palma (apagado = mão fechada), marcas nas pontas dos dedos, rótulos com os valores, brilho neon, painel com canto cortado. O usuário pediu pra **tirar os anéis em volta das mãos** e o **anel animado da calibração** — não recolocar.
- Teclas: `c` calibra, `r` reseta o simulador, `q` sai.
- Protocolo UDP: 5 bytes `struct.pack("<BbbbB", 0xAA, acel, dire, arma, seq)`, valores −100..100, ~30x/s, pra `192.168.4.1:4210`. `ENVIAR_UDP = True`. Se o envio falhar (sem rede), avisa no terminal, mostra "ROBO SEM REDE" em roxo claro no rodapé e continua rodando. Ao sair, manda um pacote de parada.

Problemas conhecidos (não resolvidos):
- **Ré quase inalcançável e aceleração máxima difícil:** o tamanho da palma varia com 1/distância; a ~2 m da câmera, 100% de ré exigiria recuar a mão ~1,6 m. Sugestão: usar `log(r/cal)` e diminuir `PROF_DZ`/`PROF_ALCANCE`.
- Aberta/fechada sem histerese (pode piscar e dar trancos).
- ~~`cap.isOpened()` não é checado~~ — resolvido: `CAMERA` configurável no topo e mensagem clara se a câmera não abrir ou cair.
- Mandar pra `192.168.4.1` estando em outra rede Wi-Fi normalmente **não dá erro** (o pacote só se perde): "ROBO ON" no rodapé não garante que o robô está recebendo — o LED azul da ESP32 piscando rápido é que confirma.

## `modo_mao.h` (lado ESP32, em `firmware/codigo_robo_controle_p3/`)
softAP `RoboBatalha` / `12345678`, canal 1, **no máximo 1 aparelho conectado**, UDP 4210, pacote de 5 bytes começando com `0xAA`, zera o comando se ficar 300 ms sem pacote (`lerModoMao()` retorna `false`). A senha ainda é fraca — trocar antes de evento aberto (e no notebook).

Diagnóstico pelo serial (bancada, USB, 115200): no boot imprime `Modo gestos: rede RoboBatalha OK, IP 192.168.4.1, UDP 4210 OK` e, a cada 3 s, `[wifi] rede ... | aparelhos conectados: N | ultimo pacote ha X ms | modo CONTROLE/GESTOS`. O boot costuma sair antes de dar tempo de abrir o monitor; a linha de 3 em 3 s é a que confirma.

Obs.: o Windows às vezes demora a mostrar a rede nova na lista de Wi-Fi (a lista fica desatualizada); abrir a lista de redes de novo / esperar uns segundos.

---

## Próximos passos
Integração testada e aprovada no robô (2026-09-24). Pendências:
1. **Gravar o firmware com a mistura dos gestos corrigida** e testar se ficou fácil andar reto; se a curva ficar fraca/forte demais, ajustar `CURVA_*` no `controle_mao.py` (não precisa regravar) ou `LIMITE_GESTOS_GIRO` no firmware.
2. Partida suave da arma ("acelerar" a arma) — perguntar o que o usuário quer; hoje liga a 100% na hora.
3. Escala de profundidade dos gestos (ré difícil de alcançar) — ver "Problemas conhecidos".
4. Trocar a senha da rede `RoboBatalha` antes de evento aberto.
5. `firmware/bertos_controle` obsoleto — perguntar se pode apagar.

Como pilotar por gestos: notebook na rede `RoboBatalha` (senha `12345678`; fica sem internet) → `controle_mao.py` → calibrar (`c`) → START no controle → **R1** (LED azul pisca rápido) → gestos. **L1** volta pro controle.

## Regras de trabalho do usuário
- Mudanças incrementais; preservar o que funciona; fiel ao repositório oficial.
- Respostas diretas e técnicas, com passos acionáveis.
- Não inventar comportamento de lib ou hardware; se não souber, dizer (e medir: `descobrir_parametros_controle`, testes no PC).
- O usuário testa (webcam, robô) e reporta; **o usuário faz os commits** (Claude sugere a mensagem).
