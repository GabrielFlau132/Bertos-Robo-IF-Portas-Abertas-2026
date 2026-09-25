# Apresentação — checklist e plano B

## Na véspera
- [ ] **Bateria do robô carregada.** Leve a de reserva e o carregador.
- [ ] **Controle de PS3 carregado.** Leve o cabo USB dele.
- [ ] **Firmware certo gravado na ESP32:**
  ```
  powershell -ExecutionPolicy Bypass -File scripts\gravar_firmware.ps1
  ```
  Tem que aparecer "Gravado" e a linha `[wifi] rede RoboBatalha ...`.
- [ ] **Ambiente do notebook ok:**
  ```
  powershell -ExecutionPolicy Bypass -File scripts\preparar_ambiente.ps1
  ```
  Tem que terminar com "AMBIENTE PRONTO". Se for **outro notebook**, rode isso lá com internet: ele baixa ~300 MB.
- [ ] Teste completo com o robô **suspenso**: modo controle, R1 com os gestos e L1 de volta.
- [ ] Leve: cabo USB de **dados** da ESP32, notebook carregado e carregador, suporte para a webcam.

## Montagem no local (nesta ordem)
1. Posicione a **webcam** a uns 1,5 a 2 m de onde o piloto vai ficar, com o **tronco inteiro** (ombros até o quadril) no quadro e boa luz de frente. Evite gente passando atrás do piloto.
2. Ligue o **robô** e o **controle** (botão PS). Aperte **START** e teste o **modo controle** (R2, L2, analógico direito, O/□/△).
3. Conecte o notebook na rede Wi-Fi **RoboBatalha**, senha `12345678`. O notebook fica sem internet enquanto estiver nela.
4. Abra os gestos com **`rodar_gestos.bat`** (duplo clique).
5. Piloto na frente da câmera, com as duas mãos abertas na posição neutra: aperte **`c`** e espere a barra encher.
6. No controle, aperte **R1**. O LED azul da ESP32 deve piscar **rápido**. Pronto, está pilotando por gestos. **L1** volta para o controle.

## Durante a apresentação
- **Mão direita aberta:** anda. Mão para frente acelera, para trás dá ré, para os lados faz a curva. **Fechar a mão** para o robô.
- **Mão esquerda aberta:** liga a arma. Para frente ou para trás escolhe o sentido. Fechar a mão desliga.
- **Emergência:** **SELECT** no controle desliga tudo na hora. Desligar o controle também para o robô.
- Trocou o piloto, ou ele mudou de distância? Aperte **`c`** de novo para recalibrar.

## Diagnóstico rápido
| Sintoma | Causa provável | O que fazer |
|---|---|---|
| Controle não conecta (robô não liga no START) | Controle não está pareado com esta ESP32 | Gravar `descobrir_parametros_controle` (`gravar_firmware.ps1 -Sketch descobrir_parametros_controle`), colocar o controle em pareamento, esperar "controle conectado" no serial e gravar o principal de novo |
| Uma roda gira ao contrário / R2 faz o robô girar no lugar | Sentido de um motor invertido | Setas do controle (↓ é o padrão; tente → ou ←), depois L3+R3 para travar |
| R1 e o LED pisca **devagar** | Comandos do PC não chegam | Notebook está na rede **RoboBatalha**? `rodar_gestos.bat` aberto? Rodapé mostra "ROBO SEM REDE"? A rede aceita **1 aparelho**: desconecte celulares dela |
| Rede RoboBatalha não aparece | Lista de Wi-Fi do Windows desatualizada, ou ESP32 sem energia | Abrir a lista de redes de novo e esperar uns segundos; conferir se o robô está ligado |
| Gestos: o programa fecha com "nao consegui abrir a camera" | Índice da câmera errado ou câmera em uso | Fechar outros programas que usam a câmera; mudar `CAMERA` no topo do `controle_mao.py` (0, 1, 2...) |
| Mão fica **cinza** ("ignorada") | Mão longe do pulso detectado, ou é de outra pessoa | Piloto mais perto e sozinho no quadro; se for sempre, aumentar `MAO_DIST_MAX` |
| Difícil andar reto por gestos | Curva sensível demais | Aumentar `CURVA_DZ` (ex.: 0.30) no `controle_mao.py`, sem regravar a ESP32 |
| Robô rápido ou lento demais | Limites | `LIMITE_*` no `firmware/codigo_robo_controle_p3/parametros.h`, depois regravar |
| Ré por gestos quase não funciona | Limitação conhecida da medição de profundidade | Diminuir `PROF_ALCANCE` (ex.: 0.30) no `controle_mao.py` |
| ESP32 não aparece no PC (sem porta COM) | Cabo só de carga, ou falta o driver CP210x | Trocar o cabo; instalar o driver (link no `README.md`) |
| Gravação trava em "Connecting..." | ESP32 não entrou em modo de gravação | Segurar o botão **BOOT** da ESP32 durante a gravação |
| "ModuleNotFoundError" / "mediapipe has no attribute solutions" | Rodou com o Python errado | Usar o `rodar_gestos.bat` (usa o venv) ou `scripts\preparar_ambiente.ps1` |
| Robô para sozinho no modo gestos | Mais de 300 ms sem comando (programa travou ou fechou, Wi-Fi caiu) | Normal, é a segurança. Reabrir o programa ou voltar para o controle (L1) |

**Plano B:** se os gestos não funcionarem no local, apresente pelo **modo controle (L1)**, que não depende do Wi-Fi nem da câmera.
