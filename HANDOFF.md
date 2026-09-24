# Handoff — Controle por gestos do robô de batalha

## Objetivo
Controlar o robô de batalha (ESP32 + Bluepad32) de dois jeitos: pelo controle de PS4 e por gestos dos braços, reconhecidos por webcam em Python. **Fase atual: só o reconhecimento**, testado num simulador. O robô real vem depois.

## Ambiente
- Windows, pasta do projeto `C:\Users\IF maker\Desktop\Bertos`
- Python 3.12 em venv (`venv\Scripts\activate`)
- `mediapipe==0.10.14` + `opencv-python`. Versões mais novas do mediapipe não têm `mp.solutions`, **não atualizar**.

## Arquivos
| Arquivo | Função |
|---|---|
| `controle_mao.py` | Webcam → MediaPipe Hands (2 mãos) + Pose → calcula `acel`, `dire`, `arma` → simulador e/ou UDP. HUD visual (tema vermelho/dourado) e calibração automática por gesto. |
| `sim_robo.py` | Robô virtual visto de cima (mistura tipo tanque), desenhado ao lado da câmera. Não foi mexido nesta sessão. |
| `modo_mao.h` | Lado ESP32: softAP `RoboBatalha`/`12345678`, UDP 4210, timeout de 300 ms → zera o comando. **Ainda não integrado ao `.ino`**. Não foi mexido nesta sessão. |

Flags no topo de `controle_mao.py`: `SIMULAR = True`, `ENVIAR_UDP = False`, `DEBUG = True` (imprime diagnóstico no terminal a cada ~0.3s).

## Mapeamento de controle (definido pelo usuário — não mudar)
**Braço direito (locomoção)**
- Mão aberta = anda. Mão fechada ou fora da câmera = para.
- Mão pra frente, em direção à câmera = acelera. Pra trás = ré.
- Direção do braço (esquerda/direita em relação ao ombro) = curva.

**Braço esquerdo (arma)**
- Mão aberta = ativa. Fechada = para.
- Frente/trás define o sentido de rotação.
- Deve girar bem rápido: ao sair da zona morta já entra em `ARMA_MIN = 70%`.

## Como está implementado hoje
- Frame espelhado (`cv2.flip`). Como o frame já é espelhado antes de processar, o `label` ("Left"/"Right") que o MediaPipe Hands devolve em `multi_handedness` já corresponde à mão real da pessoa — é isso que identifica `mao_d`/`mao_e` agora (substituiu a lógica antiga por posição x em relação ao meio dos ombros, que ficava errada se os braços cruzassem).
- **Profundidade** = tamanho da palma em px (max de 0→9 e 5→17) ÷ largura dos ombros em px, comparado ao valor calibrado.
- **Curva** = offset horizontal do punho direito em relação ao ombro direito, em larguras de ombro, menos o neutro calibrado.
- **Mão aberta** = pelo menos 3 dedos com a ponta mais longe do punho que a articulação PIP (`mao_aberta()`).
- Suavização EMA (`SUAVIZACAO = 0.4`), zona morta e alcance configuráveis no topo (`PROF_DZ`, `PROF_ALCANCE`, `CURVA_DZ`, `CURVA_ALCANCE`).
- **Calibração automática por gesto (NOVO, é o que está quebrado — ver seção de baixo):** `pose_cruzada(plm)` (linha ~90) detecta os braços em X (pulso direito passou do ombro esquerdo, pulso esquerdo passou do ombro direito, os dois acima da linha do quadril). Ao detectar, inicia uma contagem de `CAL_AUTO_SEGUNDOS = 3.0` segundos (`segurando_desde`, checado a cada frame dentro do `if largura > 20:`, por volta da linha 200 de `controle_mao.py`). Enquanto conta, `acel`/`dire`/`arma` ficam forçados em 0 (o robô para) porque nenhum branch os recalcula nesse estado. Quando a contagem chega a 0, se `mao_d`, `mao_e`, `ab_d` e `ab_e` (mãos detectadas E abertas nesse exato frame) forem verdade, salva `cal = {"r_dir":..., "x_dir":..., "r_arma":...}`; senão mostra `CALIBRACAO FALHOU: abra as duas maos`. O gatilho X é checado todo frame (mesmo já calibrado), então dá pra recalibrar a qualquer momento repetindo o gesto.
- Tecla `c` calibra na hora (exige as duas mãos abertas no mesmo frame). Tecla `r` reseta o simulador, `q` sai.
- **Visual:** esqueleto da Pose sem os pontos do rosto (`POSE_SEM_ROSTO`, filtra landmarks 0–10), cor vermelho/dourado (`COR_HUD`, `COR_HUD_OURO`) em vez do azul/ciano original — pedido do usuário pra parecer "tipo Tony Stark". Mãos desenhadas em verde (direita/locomoção) e magenta (esquerda/arma), com retículo tipo mira nos 4 cantos de cada mão (`desenhar_alvo`/`cantos`). Painel HUD semitransparente no canto superior esquerdo com o estado, leituras e barra de progresso da calibração. Moldura de cantos ao redor do vídeo inteiro.
- Protocolo UDP, 5 bytes: `struct.pack("<BbbbB", 0xAA, acel, dire, arma, seq)`. `acel`, `dire` e `arma` vão de -100 a 100.

## PROBLEMA ATUAL (começar por aqui)
**A calibração automática por gesto de X não está completando.** Segundo o usuário: "ele fica tentando calibrar e não consegue" — ou seja, o X é detectado, a contagem de 3s aparece na tela, mas ao final ela falha (mostra `CALIBRACAO FALHOU`) e o ciclo parece reiniciar sem nunca calibrar de verdade.

O código relevante está em `controle_mao.py`, dentro do `if largura > 20:` (por volta da linha 182 em diante):
```python
if segurando_desde is None and pose_cruzada(plm):
    segurando_desde = agora

if segurando_desde is not None:
    restante = CAL_AUTO_SEGUNDOS - (agora - segurando_desde)
    progresso_cal = max(0.0, min(1.0, 1 - restante / CAL_AUTO_SEGUNDOS))
    if restante <= 0:
        if mao_d and mao_e and ab_d and ab_e:
            cal = {"r_dir": r_d, "x_dir": x_d, "r_arma": r_e}
            cal_msg, cal_msg_ate = "CALIBRADO!", agora + 1.5
        else:
            cal_msg = "CALIBRACAO FALHOU: abra as duas maos"
            cal_msg_ate = agora + 1.5
        segurando_desde = None
        ...
```

Hipóteses, da mais provável para a menos provável:
1. **A checagem final é de um único frame.** `restante <= 0` só passa em UM frame específico (o primeiro após os 3s baterem), e nesse exato frame exige `mao_d and mao_e and ab_d and ab_e` simultaneamente. Sair da pose de X (braços cruzados no peito) para mãos abertas na posição neutra é um movimento rápido — é bem provável que bem na hora que os 3s terminam, uma das mãos esteja em movimento, borrada, momentaneamente fora do quadro do MediaPipe Hands, ou ainda não "aberta" o bastante pra `mao_aberta()` contar 3 dedos esticados. Isso faria a calibração falhar quase toda vez, mesmo que o usuário esteja "com as mãos abertas" ao olho nu.
2. **`pose_cruzada()` pode estar re-disparando rápido demais.** Se, logo após uma falha, os braços do usuário ainda não descruzaram completamente (ou o corpo ainda está na pose intermediária), `pose_cruzada(plm)` pode voltar a ser `True` no frame seguinte e reiniciar a contagem de 3s imediatamente — dando a sensação de "ele fica tentando e não consegue", quando na prática está reiniciando o ciclo repetidamente.
3. **`mao_aberta()` pode estar sendo exigente demais** logo após o movimento de descruzar os braços (ângulo da mão em relação à câmera nesse momento pode não deixar 3 dedos claramente mais longe do punho que o PIP).
4. **Falta de visibilidade no terminal.** O log de `DEBUG` (por volta da linha 245-253) hoje só imprime `estado`, `cal`, se `mao_d`/`mao_e` existem, e as razões `r_d/cal` etc. quando `cal` já existe. Ele **não imprime** `ab_d`, `ab_e`, `pose_cruzada(plm)` nem `progresso_cal` — ou seja, não dá pra ver pelo terminal, no momento exato da falha, qual das condições (`mao_d`, `mao_e`, `ab_d`, `ab_e`) não bateu. Isso deveria ser o primeiro passo antes de qualquer mudança de lógica.

Primeiro passo sugerido: adicionar ao log de `DEBUG` os valores de `ab_d`, `ab_e`, `pose_cruzada(plm)` (quando `rp.pose_landmarks` existir) e `segurando_desde`/`progresso_cal`, pra ver exatamente qual condição falha no frame em que `restante <= 0`. Depois, considerar trocar a checagem de "um frame só" por algo mais tolerante — por exemplo, exigir mãos abertas por alguns frames seguidos antes do fim da contagem, ou dar uma folga extra (tipo mais 0.5s) se as mãos aparecerem abertas logo em seguida ao final da contagem, em vez de falhar de vez.

## Próximas fases
1. Resolver a calibração por gesto (acima). Depois, revisitar a sensibilidade de profundidade (`PROF_DZ`/`PROF_ALCANCE`) — havia um relato anterior do usuário de que `acel` ficava sempre em 0 mesmo com a mão aberta e calibrada; não foi confirmado se já foi resolvido, os valores de `r_d/cal` do log de DEBUG ajudam a confirmar.
2. Integrar `modo_mao.h` no firmware. Pedir o `.ino` e o `parametros.h` ao usuário se não estiverem na pasta.
   - Chamar `iniciarModoMao()` no `setup()`, depois do Bluepad32. **Testar primeiro se o gamepad continua responsivo**, porque Wi-Fi e BT dividem o mesmo rádio do ESP32.
   - O gamepad é o dono: um botão alterna o modo braço (sugestão: triângulo, `ctl->y()`, que está livre no mapeamento atual). Se o gamepad desconectar, o robô para. Com `lerModoMao() == false` (timeout), o robô para.
   - Arma: sinal → sentido, módulo → PWM até `MAX_PWM`.
   - **Aplicar rampa (soft-start) e tempo morto na inversão também nos motores da arma.** Histórico: partidas bruscas dos motores já derrubaram a alimentação e resetaram o rádio numa versão anterior do robô.
3. Colocar `ENVIAR_UDP = True` e testar com o robô suspenso antes do chão.

## Firmware atual (contexto)
- ESP32 devkit 38 pinos num shield com 2× DRV8833. Sketch `.ino` + `parametros.h` (`MAX_PWM`, `MIN_PWM`, limites e tolerância do joystick).
- 2 motores de locomoção (sentidoMotor/velocidadeMotor por lado) e 2 motores de arma (`PINO_1/2_ARMA1`, `PINO_1/2_ARMA2`).
- Gamepad: START liga, SELECT desliga, L3+R3 trava/destrava as configurações, d-pad inverte o giro dos motores, R2 acelera, L2 dá ré, analógico esquerdo faz a direção, analógico direito define o sentido da arma.

## Regras de trabalho do usuário
- Mudanças incrementais. Preservar o que já funciona e não refatorar amplamente.
- Respostas diretas e técnicas, com passos acionáveis ("substitua X por Y", "crie a função Z em W").
- Não inventar comportamento de lib ou hardware. Se não souber, dizer.
- O usuário testa rodando `python controle_mao.py` ele mesmo (a IA não consegue testar a webcam/gestos por conta própria) e reporta o que vê na tela e no terminal.
