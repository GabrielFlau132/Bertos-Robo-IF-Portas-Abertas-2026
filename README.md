# Bertos — Robô IF Portas Abertas 2026

Robô de batalha (categoria cupim) que pode ser pilotado de dois jeitos, um de cada vez. A escolha é feita no controle:

- **L1 — controle de PS3**.
- **R1 — gestos dos braços** reconhecidos por webcam (Python + MediaPipe, enviados por Wi-Fi).

Os dois modos foram testados no robô e funcionam.

Hardware e firmware base: [nrc-cupim/start-automacao-eletrica](https://github.com/nrc-cupim/start-automacao-eletrica) (placa START_AUTOMACAO_ELETRICA: ESP32 DevKit v1 + 2× DRV8833).

O estado detalhado, as decisões e os próximos passos estão no [HANDOFF.md](HANDOFF.md).

## Estrutura

| Caminho | O que é |
|---|---|
| `firmware/codigo_robo_controle_p3/` | **Firmware do robô**: código oficial + R2/L2 e analógico direito, sentido padrão das rodas e modo gestos (R1/L1). O `modo_mao.h` (Wi-Fi + UDP) fica aqui |
| `firmware/` (outras pastas) | Utilitários oficiais, diagnóstico e uma versão obsoleta. Veja o [firmware/README.md](firmware/README.md) |
| `controle_mao.py` | Controle por gestos: webcam → MediaPipe Hands + Pose → aceleração, direção e arma → simulador + UDP para o robô |
| `sim_robo.py` | Robô virtual visto de cima, desenhado ao lado da câmera |

## Pilotar

| Comando | Função |
|---|---|
| START / SELECT | liga / desliga o robô (SELECT para tudo na hora) |
| **L1 / R1** | **modo controle / modo gestos** (trocar de modo para todos os motores) |
| R2 / L2 | frente / trás *(modo controle)* |
| Analógico direito ↔ | direção; sem gatilho, gira no lugar *(modo controle)* |
| O / □ / △ | arma num sentido / no outro / desliga *(modo controle)* |
| Setas | sentido das rodas (padrão = seta ↓) *(os dois modos)* |
| L3 + R3 | trava as setas (LED azul da ESP32 aceso) |

**Velocidade:** a locomoção é limitada em cada modo:
- **modo controle:** frente/ré 80% e giro 60%;
- **modo gestos:** frente/ré 60% e giro 40%.

Para ajustar, mude os `LIMITE_*` em `firmware/codigo_robo_controle_p3/parametros.h` (100 = sem limite) e grave de novo. A arma não é limitada.

O robô sempre liga no modo controle. No **modo gestos**:
- O LED azul da ESP32 **pisca devagar** enquanto não chegam comandos do PC e **pisca rápido** quando está recebendo.
- Se o PC parar de mandar comandos por 300 ms, o robô para.
- Se o controle desconectar, o robô desliga, em qualquer modo.

**Parear o controle:**
1. Grave o `firmware/descobrir_parametros_controle`.
2. Coloque o controle em modo de pareamento e espere aparecer "controle conectado" no serial.
3. Grave o `firmware/codigo_robo_controle_p3`.

A partir daí o controle reconecta sozinho.

## Gravar o firmware

1. Driver USB da ESP32: [CP210x (Silicon Labs)](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers).
2. URLs de placas (Arduino IDE → Preferências, ou `arduino-cli config add board_manager.additional_urls`):
   - `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - `https://raw.githubusercontent.com/ricardoquesada/esp32-arduino-lib-builder/master/bluepad32_files/package_esp32_bluepad32_index.json`
3. Instale as placas **esp32** e **esp32_bluepad32**, uma de cada vez. Escolha a placa **ESP32 + Bluepad32 Arduino → ESP32 Dev Module**.
4. Pela linha de comando (troque `COM11` pela sua porta):

```bash
arduino-cli compile --fqbn esp32-bluepad32:esp32:esp32 firmware/codigo_robo_controle_p3
```

```bash
arduino-cli upload -p COM11 --fqbn esp32-bluepad32:esp32:esp32 firmware/codigo_robo_controle_p3
```

A ESP32 não fica ligada no PC e no robô ao mesmo tempo: grave no PC e depois coloque no robô.

## Rodar o controle por gestos

Precisa de **Python 3.12**. O `mediapipe==0.10.14` não instala em versões mais novas, e versões mais novas do mediapipe não têm `mp.solutions`. Nesta máquina o 3.12 está em `%LOCALAPPDATA%\Programs\Python\Python312`:

```bash
& "$env:LOCALAPPDATA\Programs\Python\Python312\python.exe" -m venv venv
```

```bash
.\venv\Scripts\python.exe -m pip install -r requirements.txt
```

```bash
.\venv\Scripts\python.exe controle_mao.py
```

Teclas: `c` calibra (fique na posição neutra com as duas mãos na câmera enquanto a barra enche, 5 s), `r` reseta o simulador, `q` sai.

Gestos:
- **Mão direita:** aberta anda. Levar a mão para frente ou para trás acelera ou dá ré, e para os lados faz a curva. Perto da posição neutra ela conta como "reto", e a curva cresce aos poucos. Ajuste em `CURVA_*` no topo do `controle_mao.py`.
- **Mão esquerda:** aberta liga a arma. Levar a mão para frente ou para trás escolhe o sentido.

**Pilotar o robô por gestos:**
1. Ligue o robô e o controle. Conecte o notebook na rede Wi-Fi **RoboBatalha** (senha `12345678`). O notebook fica sem internet enquanto estiver nela, e a rede aceita um aparelho só.
2. Rode o `controle_mao.py` e calibre com `c`.
3. No controle, aperte **START** e depois **R1**. O LED azul da ESP32 deve piscar rápido.
4. **L1** devolve a pilotagem ao controle. **SELECT** desliga tudo.

O rodapé da tela mostra `ROBO ON` quando está enviando e `ROBO SEM REDE`, em roxo claro, se o envio falhar. Estando em outra rede Wi-Fi, o envio pode não dar erro: quem confirma que o robô está recebendo é o LED piscando rápido.

## Segurança
- Teste sempre com o robô **suspenso** (rodas sem tocar no chão) antes de ir para o chão.
- A arma liga a 100% na hora: mantenha as mãos longe.
- O robô para se o controle desconectar (nos dois modos) e se o PC parar de mandar comandos no modo gestos.
- A senha da rede `RoboBatalha` é fraca. Troque-a no `firmware/codigo_robo_controle_p3/modo_mao.h` antes de um evento aberto.
