# Bertos — Robô IF Portas Abertas 2026

Robô de batalha (categoria cupim) que pode ser pilotado de dois jeitos:

- **Controle de PS3**: funcionando no robô.
- **Gestos dos braços** reconhecidos por webcam (Python + MediaPipe): funcionando no simulador, ainda não ligado ao robô.

Hardware e firmware base: [nrc-cupim/start-automacao-eletrica](https://github.com/nrc-cupim/start-automacao-eletrica) (placa START_AUTOMACAO_ELETRICA: ESP32 DevKit v1 + 2× DRV8833).

O estado detalhado, as decisões e os próximos passos estão no [HANDOFF.md](HANDOFF.md).

## Estrutura

| Caminho | O que é |
|---|---|
| `firmware/codigo_robo_controle_p3/` | **Firmware em uso no robô**: código oficial com 2 mudanças (R2/L2 + analógico direito; sentido padrão das rodas) |
| `firmware/` (outras pastas) | Utilitários oficiais, diagnóstico e uma versão obsoleta. Veja o [firmware/README.md](firmware/README.md) |
| `controle_mao.py` | Controle por gestos: webcam → MediaPipe Hands + Pose → aceleração, direção e arma → simulador e/ou UDP |
| `sim_robo.py` | Robô virtual visto de cima, desenhado ao lado da câmera |
| `modo_mao.h` | Lado ESP32 do modo gestos (Wi-Fi softAP + UDP). **Ainda não integrado** |

## Pilotar com o controle

| Comando | Função |
|---|---|
| START / SELECT | liga / desliga o robô |
| R2 / L2 | frente / trás |
| Analógico direito ↔ | direção (sem gatilho, gira no lugar) |
| O / □ / △ | arma num sentido / no outro / desliga |
| Setas | sentido das rodas (padrão = seta ↓) |
| L3 + R3 | trava as setas (LED azul da ESP32 aceso) |

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

Teclas: `c` calibra (fique na posição neutra com as duas mãos na câmera durante a contagem de 5 s), `r` reseta o simulador, `q` sai.

Gestos:
- **Mão direita:** aberta anda. Levar a mão para frente ou para trás acelera ou dá ré, e para os lados faz a curva.
- **Mão esquerda:** aberta liga a arma. Levar a mão para frente ou para trás escolhe o sentido.

Hoje o programa só mostra o resultado no simulador (`ENVIAR_UDP = False`).

## Segurança
- Teste sempre com o robô **suspenso** (rodas sem tocar no chão) antes de ir para o chão.
- A arma liga a 100% na hora: mantenha as mãos longe.
- O robô para se o controle desconectar.
