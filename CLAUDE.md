# CLAUDE.md — Robô Bertos (IF Portas Abertas 2026)

Guia operacional para a IA. Leia inteiro antes de mexer. O histórico detalhado e as decisões estão em `HANDOFF.md`; o roteiro do dia da apresentação e o diagnóstico rápido estão em `APRESENTACAO.md`.

## O que é
Robô de batalha (categoria cupim, ESP32 + 2× DRV8833) pilotado de dois jeitos, **um de cada vez**, escolhidos no controle de PS3:
- **L1 = modo controle** (R2 frente, L2 ré, analógico direito direção, O/□/△ arma);
- **R1 = modo gestos**: webcam → `controle_mao.py` (MediaPipe) → Wi-Fi/UDP → ESP32.

**Estado (2026-09-25): tudo funcionando e aprovado pelo usuário no robô**, incluindo velocidade, direção, barra de calibração e paleta roxo/verde. Apresentação em 2026-09-26. Não mude o que funciona sem pedido.

> ⚠️ **PENDENTE:** a última versão do firmware (mistura própria dos gestos, `movimentoGestos`, que corrige a direção) **compila e passa nos testes, mas ainda não foi gravada na ESP32**: ela não estava no USB. A ESP32 está com a versão anterior, que tem os limites de velocidade mas usa a mistura oficial nos gestos. Grave assim que a ESP32 estiver no PC: `scripts\gravar_firmware.ps1`. Quando gravar, atualize esta nota e o `HANDOFF.md`.

## Mapa de arquivos
| Caminho | O quê |
|---|---|
| `controle_mao.py` | Gestos: webcam → MediaPipe Hands+Pose → `acel`/`dire`/`arma` (−100..100) → HUD + simulador + UDP. **Todos os ajustes ficam em constantes no topo** |
| `sim_robo.py` | Robô virtual desenhado ao lado da câmera (não mostra os limites de velocidade do firmware) |
| `firmware/codigo_robo_controle_p3/` | **Firmware do robô**: `.ino`, `parametros.h` (pinos, limites) e `modo_mao.h` (Wi-Fi softAP + UDP). Base = código oficial + mudanças listadas no topo do `.ino` |
| `firmware/descobrir_parametros_controle/` | Oficial: pareia o controle e imprime no serial o MAC e o valor de todos os botões/eixos |
| `firmware/filtro_mac_controle/`, `filtro_com_descobrir_parametros/` | Oficiais, não usados (lista de controles permitidos) |
| `firmware/teste_motores/` | Diagnóstico: cada seta liga um borne de motor devagar (descobrir ligação sem PC) |
| `firmware/bertos_controle/` | **Obsoleto, não gravar** (reescrita abandonada) |
| `scripts/preparar_ambiente.ps1` | Cria/confere o venv (Python 3.12 + libs) e, com `-Arduino`, o arduino-cli/core/driver |
| `scripts/gravar_firmware.ps1` | Compila e grava (acha a porta sozinho); `-SoCompilar`, `-Sketch`, `-Porta` |
| `rodar_gestos.bat` | Duplo clique: abre o controle por gestos com o venv |
| `tests/rodar_testes.ps1` | Todos os testes sem robô/webcam (firmware no PC com libs falsas + Python ponta a ponta) |
| `tests/python/render_hud.py` | Gera PNGs do HUD em `tests/saida/` (conferir mudança visual sem webcam) |
| `requirements.txt` / `requirements-lock.txt` | Dependências diretas / versões exatas de tudo (o script usa o lock) |

## Ambiente (o mais importante)
- **Python 3.12 obrigatório, sempre pelo venv** (`venv\Scripts\python.exe`). `mediapipe==0.10.14` não existe pra Python mais novo, e mediapipe novo não tem `mp.solutions`: **nunca atualizar mediapipe; numpy tem que ser 1.x**.
  - Nesta máquina (usuário Gabriel132): o `python` global é **3.14** (não serve) e o launcher `py` **não lista o 3.12**; o 3.12 está em `%LOCALAPPDATA%\Programs\Python\Python312\python.exe`. O script acha sozinho.
  - Preparar/consertar: `powershell -ExecutionPolicy Bypass -File scripts\preparar_ambiente.ps1` (`-RecriarVenv` apaga e recria). Sem Python 3.12 no PC: pedir ao usuário `winget install -e --id Python.Python.3.12`.
- **Firmware:** `arduino-cli` (em `C:\Program Files\Arduino CLI`), core **`esp32-bluepad32:esp32` 4.1.0**, FQBN `esp32-bluepad32:esp32:esp32` (ESP32 Dev Module). URLs de placas no `README.md`. O script instala com `-Arduino`.
- **ESP32 no USB:** chip CP2102 → precisa do **driver CP210x** (sem ele: "CP2102 USB to UART Bridge" com erro 28, nenhuma porta COM). Nesta máquina aparece na **COM11**.
- **Testes do firmware no PC:** `g++` do MSYS2 em `C:\msys64\ucrt64\bin` (o script põe no PATH).
- Scripts `.ps1` em **ASCII puro** (o PowerShell 5.1 lê .ps1 sem BOM como ANSI e estraga acentos).

## Comandos rápidos (na raiz do projeto)
```
powershell -ExecutionPolicy Bypass -File scripts\preparar_ambiente.ps1 [-Arduino]
.\venv\Scripts\python.exe controle_mao.py                (ou rodar_gestos.bat)
powershell -ExecutionPolicy Bypass -File scripts\gravar_firmware.ps1 [-SoCompilar] [-Sketch descobrir_parametros_controle]
powershell -ExecutionPolicy Bypass -File tests\rodar_testes.ps1
.\venv\Scripts\python.exe tests\python\render_hud.py      (imagens em tests\saida\)
```
Depois de qualquer mudança: rodar `tests\rodar_testes.ps1` (e `render_hud.py` se for visual, e olhar as imagens).

## Ajustes rápidos
| Pedido | Onde | Precisa regravar a ESP32? |
|---|---|---|
| Velocidade do robô (andar / girar), por modo | `parametros.h`: `LIMITE_CONTROLE_FRENTE=80`, `LIMITE_CONTROLE_GIRO=60`, `LIMITE_GESTOS_FRENTE=60`, `LIMITE_GESTOS_GIRO=40` (% do máximo) | sim |
| Direção por gestos sensível demais / fraca | `controle_mao.py`: `CURVA_DZ=0.25` (faixa "reto"), `CURVA_ALCANCE=0.8` (100% de curva), `CURVA_EXPO=2.0` (suavidade) | não |
| Aceleração por gestos (frente/ré da mão) | `controle_mao.py`: `PROF_DZ=0.12`, `PROF_ALCANCE=0.45` | não |
| Arma por gestos entra forte/fraca | `controle_mao.py`: `ARMA_MIN=70` | não |
| Tremedeira/atraso dos gestos | `controle_mao.py`: `SUAVIZACAO=0.4` (maior = mais rápido e mais ruidoso) | não |
| Câmera errada / não abre | `controle_mao.py`: `CAMERA=0` (índice) | não |
| Cores do HUD | `controle_mao.py`: constantes `COR_*` (em **BGR**); `sim_robo.py` usa o mesmo roxo na arma | não |
| Tempo da calibração | `controle_mao.py`: `CAL_TIMER_SEGUNDOS=5`, `CAL_JANELA_SEGUNDOS=2` | não |
| Mão de gente atrás sendo pega / mão do piloto ignorada (cinza) | `controle_mao.py`: `MAO_DIST_MAX=0.8` (larguras de ombro do pulso) | não |
| Sentido das rodas ao ligar | `setup()` do `.ino` (hoje = seta ↓). Na hora dá pra corrigir pelas setas | sim |
| Senha/nome da rede do robô | `modo_mao.h`: `MAO_SSID`, `MAO_SENHA` (notebook tem que reconectar) | sim |
| Robô para rápido/devagar sem gestos | `modo_mao.h`: `MAO_TIMEOUT_MS=300` | sim |
| Zona morta dos analógicos do controle | `parametros.h`: `TOLERANCIA_JOYSTICK=5` | sim |

## Como funciona (o essencial pra corrigir coisas)
- **Controle:** PS3 **paralelo**, MAC `98:B6:37:E7:3E:F7`. O Bluepad32 o vê como **HID genérico**: pareia pelo Bluetooth comum, **não precisa de SixaxisPairTool**. R2/L2 são **só liga/desliga** (`throttle()`/`brake()` = 0 ou 1020). Analógico direito: `axisRX` +508 direita / −512 esquerda. No USB do PC ele aparece como "Xbox 360" — irrelevante. Parear: gravar `descobrir_parametros_controle` (faz `forgetBluetoothKeys`), colocar o controle em pareamento, depois gravar o firmware principal (que reconecta sozinho).
- **Firmware:** START liga, SELECT desliga (para tudo, volta pro modo controle). Controle desconectou → robô desliga (failsafe oficial, vale nos dois modos). R1/L1 trocam o modo e **param todos os motores**. Setas (sentido das rodas) e L3+R3 (trava) valem nos dois modos.
- **Modo gestos:** a ESP32 cria a rede **`RoboBatalha`** / `12345678` (canal 1, **só 1 aparelho**), IP `192.168.4.1`, UDP 4210. Pacote de 5 bytes: `0xAA, acel, dire, arma (int8 −100..100), seq`, ~30/s. Aplicado a cada 30 ms. **Sem pacote por 300 ms → tudo parado.** LED azul da ESP32 no modo gestos: pisca **devagar = sem pacotes**, **rápido = recebendo** (no modo controle o LED é a trava).
- **Mistura dos motores:** a mistura **oficial** (função `aplicaMovimento`, modo controle) só funciona bem com comando no fim do curso; com comando parcial ela **vira pro lado errado** em curvas leves. Por isso: no modo controle o limite de velocidade reduz o **PWM final** (nunca o comando); o modo gestos usa **mistura própria** (`movimentoGestos`: esquerda = frente + curva, direita = frente − curva). Não "simplificar" isso de volta.
- **Pinos/bornes da placa:** ESQUERDO 27/14 · ARMA2 12/13 · ARMA1 32/33 · DIREITO 25/26. O robô usa só uma arma ligada (ARMA2) — os comandos de arma acionam os dois bornes de arma.
- **Serial (bancada, 115200):** a cada 3 s `[wifi] rede RoboBatalha IP ... | aparelhos conectados: N | ultimo pacote ha X ms | modo ...`. O código oficial imprime PWM a cada comando.

## Armadilhas já vividas (não repetir)
- Repositório base certo: **nrc-cupim/start-automacao-eletrica** (commit a52bbf3). **Não** usar `nrc-cupim/cupim_start_inatel` (mandado por engano; causou pinagem e mapeamento errados).
- **A ESP32 não fica no PC e no robô ao mesmo tempo**: grava no PC, testa no robô; no robô não há serial — diagnóstico é pelo que o usuário vê (LED, motores). Para diagnosticar motores sem PC: `firmware/teste_motores`.
- **Não conectar o PC da sessão na rede `RoboBatalha`**: ele perde a internet e a sessão com a IA cai. Quem conecta é o usuário, na hora do teste.
- A lista de Wi-Fi do Windows fica desatualizada; pra conferir se a rede existe, forçar busca (ver `HANDOFF.md`) ou olhar o serial.
- Ao abrir o serial por .NET, usar `DtrEnable=false`/`RtsEnable=false`. Mexer em RTS pra "resetar" já deixou a ESP32 muda até religar o USB.
- `py_compile`/importar com Python global gera `__pycache__` 3.14 — usar o venv e `PYTHONDONTWRITEBYTECODE=1` nos testes.
- Testes de firmware que comparam com o oficial precisam dos limites em 100 (`-DLIMITE_...=100`); o `rodar_testes.ps1` já faz isso.
- O HANDOFF antigo dizia "R2 acelera/L2 ré" como se fosse o oficial — no oficial L2/R2 não fazem nada; R2/L2 foi mudança pedida pelo usuário.

## Regras do usuário
- Mudanças **mínimas e incrementais**; firmware **o mais fiel possível ao código oficial**, com toda mudança registrada no comentário do topo do `.ino`.
- Respostas diretas e técnicas, em português, com passos acionáveis. Não inventar comportamento de lib/hardware: medir (`descobrir_parametros_controle`, testes no PC) ou dizer que não sabe.
- O usuário testa no robô/webcam e relata. **O usuário faz os commits** — a IA sugere a mensagem (terminando com `Co-Authored-By: Claude ...`).
- Testar sempre com o robô **suspenso** antes do chão. Documentar toda mudança (este arquivo, `HANDOFF.md`, `README.md`).
