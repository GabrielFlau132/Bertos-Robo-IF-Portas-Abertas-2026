# .\venv\Scripts\python.exe controle_mao.py  
#
# Braço DIREITO (locomoção)
#   mão aberta  = anda | mão fechada/fora da câmera = para
#   mão pra frente (em direção à câmera) = acelera | pra trás = ré
#   braço pra direita/esquerda (em relação ao ombro) = curva
#
# Braço ESQUERDO (arma)
#   mão aberta  = arma ativa | mão fechada/fora da câmera = arma parada
#   mão pra frente = gira num sentido | pra trás = gira no outro
#   ao sair da zona morta já entra com ARMA_MIN % (gira rápido)
#
# Calibrar: aperte 'c' e fique na posição neutra com as duas mãos na câmera
# até a contagem de 5s terminar (o robô fica parado enquanto conta).
# 'c' = calibrar | 'r' = resetar simulador | 'q' = sair
# UDP 30x/s: [0xAA, aceleracao(int8), direcao(int8), arma(int8, sinal = sentido), seq(uint8)]
# para 192.168.4.1:4210 (rede Wi-Fi RoboBatalha criada pela ESP32). O robô só obedece
# no modo gestos (R1 no controle) e para sozinho se ficar 300 ms sem pacote.
#
# Profundidade = tamanho da palma na imagem / largura dos ombros.
# Mão perto da câmera -> palma maior. Dividir pelos ombros anula o efeito
# de você inteiro chegar mais perto ou mais longe da câmera.

import math
import socket
import struct
import time

import cv2
import mediapipe as mp
import numpy as np

from sim_robo import SimRobo

SIMULAR = True      # mostra o robô virtual ao lado da câmera
ENVIAR_UDP = True   # manda os comandos pro robô: notebook na rede Wi-Fi RoboBatalha e
                    # robô no modo gestos (R1 no controle; L1 volta pro controle)
DEBUG = True         # imprime diagnóstico da calibração/estado no terminal
CAMERA = 0           # índice da webcam (0 = padrão; troque pra 1, 2... se abrir a câmera errada)

IP_ROBO = "192.168.4.1"
PORTA = 4210
FPS_ENVIO = 30

PROF_DZ = 0.12        # variação de tamanho da palma ignorada (12%)
PROF_ALCANCE = 0.45   # variação que dá 100% (45%)
CURVA_DZ = 0.25       # em larguras de ombro: faixa em volta do neutro que conta como "reto"
CURVA_ALCANCE = 0.8   # deslocamento que dá 100% de curva
CURVA_EXPO = 2.0      # 1 = linear; maior = curva bem suave perto do centro e forte só no fim
ARMA_MIN = 70         # % assim que sai da zona morta
SUAVIZACAO = 0.4      # 0..1: maior = mais rápido e mais ruidoso
CAL_TIMER_SEGUNDOS = 5.0   # contagem depois de apertar 'c'
CAL_JANELA_SEGUNDOS = 2.0  # depois da contagem, tolerancia p/ as duas maos aparecerem
MAO_DIST_MAX = 0.8    # distancia max (em larguras de ombro) entre a mao e o pulso do Pose

# Paleta roxo + verde (valores em BGR, como o OpenCV usa)
COR_HUD = (255, 60, 150)       # roxo - acentos do HUD (molduras, bordas, títulos)
COR_HUD_VERDE = (130, 255, 60)  # verde - destaque (estado, barra de calibração)
COR_NUCLEO = (240, 255, 235)   # branco levemente esverdeado (núcleo do repulsor, números)
COR_MAO_D = COR_HUD_VERDE      # mão direita (locomoção)
COR_MAO_E = (255, 100, 190)    # mão esquerda (arma), roxo mais claro que o do HUD
COR_FUNDO_PAINEL = (22, 10, 18)  # fundo escuro arroxeado dos painéis
FONTE = cv2.FONT_HERSHEY_SIMPLEX

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
mp_hands = mp.solutions.hands
mp_pose = mp.solutions.pose


def dist(a, b, w, h):
    return math.hypot((a.x - b.x) * w, (a.y - b.y) * h)


def mao_aberta(lm, w, h):
    """dedo esticado = ponta mais longe do punho que a articulação do meio"""
    esticados = sum(dist(lm[t], lm[0], w, h) > dist(lm[p], lm[0], w, h)
                    for t, p in [(8, 6), (12, 10), (16, 14), (20, 18)])
    return esticados >= 3


def palma(lm, w, h):
    """tamanho da palma em px (maior de duas medidas, menos sensível a rotação)"""
    return max(dist(lm[0], lm[9], w, h), dist(lm[5], lm[17], w, h))


def cantos(frame, x0, y0, x1, y1, cor, tam=18, esp=2):
    """moldura tipo mira HUD, só os 4 cantos do retângulo"""
    for cx, cy, dx, dy in [(x0, y0, 1, 1), (x1, y0, -1, 1), (x0, y1, 1, -1), (x1, y1, -1, -1)]:
        cv2.line(frame, (cx, cy), (cx + dx * tam, cy), cor, esp)
        cv2.line(frame, (cx, cy), (cx, cy + dy * tam), cor, esp)


def desenhar_alvo(frame, lm, w, h, cor, folga=16, tam=18):
    """retículo ao redor da mão"""
    xs = [p.x * w for p in lm]
    ys = [p.y * h for p in lm]
    cantos(frame, int(min(xs)) - folga, int(min(ys)) - folga,
           int(max(xs)) + folga, int(max(ys)) + folga, cor, tam)


# ---------------------------------------------------------------- visual HUD
# Tudo que brilha é desenhado numa camada preta separada ('hud'); brilho() soma
# essa camada borrada (halo neon) + ela nítida por cima da imagem da câmera.

def escurecer(cor, f):
    return tuple(int(c * f) for c in cor)


def texto(img, txt, org, escala, cor, esp=1, larg_max=None):
    """putText suavizado; diminui a fonte se passar de larg_max px"""
    if larg_max:
        tw = cv2.getTextSize(txt, FONTE, escala, esp)[0][0]
        if tw > larg_max:
            escala *= larg_max / tw
    cv2.putText(img, txt, org, FONTE, escala, cor, esp, cv2.LINE_AA)


def texto_centro(img, txt, cx, cy, escala, cor, esp=2, larg_max=None):
    if larg_max:
        tw = cv2.getTextSize(txt, FONTE, escala, esp)[0][0]
        if tw > larg_max:
            escala *= larg_max / tw
    (tw, th), _ = cv2.getTextSize(txt, FONTE, escala, esp)
    cv2.putText(img, txt, (cx - tw // 2, cy + th // 2), FONTE, escala, cor, esp, cv2.LINE_AA)


def estilo_camera(frame):
    """escurece a câmera e põe linhas de varredura, pro HUD saltar da imagem"""
    cv2.convertScaleAbs(frame, frame, alpha=0.72)
    frame[::3] = frame[::3] // 4 * 3


def brilho(frame, hud):
    h, w = frame.shape[:2]
    halo = cv2.resize(hud, (w // 2, h // 2))
    halo = cv2.resize(cv2.GaussianBlur(halo, (0, 0), 5), (w, h))
    frame = cv2.addWeighted(frame, 1.0, halo, 1.6, 0)
    return cv2.add(frame, hud)


def centro_palma(lm, w, h):
    ids = (0, 5, 9, 13, 17)
    return (int(sum(lm[i].x for i in ids) / 5 * w), int(sum(lm[i].y for i in ids) / 5 * h))


def hud_mao(hud, lm, w, h, cor, ativa, t, linhas, lado):
    """
    HUD de armadura sobre a mão: núcleo de repulsor na palma (aceso = mão aberta),
    marcas nas pontas dos dedos e rótulo puxado pro lado de fora (lado = +1/-1).
    """
    c = centro_palma(lm, w, h)
    r = max(28, int(palma(lm, w, h) * 0.95))
    fraca = escurecer(cor, 0.45)

    for i in (4, 8, 12, 16, 20):  # só as pontas dos dedos, nada de esqueleto
        p = (int(lm[i].x * w), int(lm[i].y * h))
        cv2.line(hud, c, p, escurecer(cor, 0.25), 1, cv2.LINE_AA)
        cv2.circle(hud, p, 5, cor, 1, cv2.LINE_AA)
        cv2.circle(hud, p, 2, COR_NUCLEO, -1, cv2.LINE_AA)

    if ativa:
        pulsa = 0.85 + 0.15 * math.sin(t * 8)
        cv2.circle(hud, c, int(r * 0.34 * pulsa), cor, -1, cv2.LINE_AA)
        cv2.circle(hud, c, int(r * 0.17), COR_NUCLEO, -1, cv2.LINE_AA)
    else:
        cv2.circle(hud, c, int(r * 0.2), fraca, 1, cv2.LINE_AA)

    rm = int(r * 1.1)  # onde a linha do rótulo começa, um pouco fora da mão
    a = math.radians(-45 if lado > 0 else -135)
    p0 = (int(c[0] + rm * math.cos(a)), int(c[1] + rm * math.sin(a)))
    p1 = (p0[0] + lado * 26, max(p0[1] - 26, 26 + 20 * len(linhas)))  # rótulo não sai pelo topo
    larg = 140
    x_txt = max(4, min(w - larg - 4, p1[0] + 6 if lado > 0 else p1[0] - larg))
    p2 = (x_txt + larg if lado > 0 else x_txt, p1[1])
    cv2.circle(hud, p0, 3, cor, -1, cv2.LINE_AA)
    cv2.line(hud, p0, p1, cor, 1, cv2.LINE_AA)
    cv2.line(hud, p1, p2, cor, 1, cv2.LINE_AA)
    for i, txt in enumerate(linhas):
        y = p1[1] - 8 - (len(linhas) - 1 - i) * 20
        if i == 0:
            texto(hud, txt, (x_txt, y), 0.45, cor, 1)
        else:
            texto(hud, txt, (x_txt, y), 0.55, COR_NUCLEO, 1, larg_max=larg)


def ret_arredondado(img, x0, y0, x1, y1, r, cor):
    """retângulo preenchido com cantos arredondados"""
    cv2.rectangle(img, (x0 + r, y0), (x1 - r, y1), cor, -1)
    cv2.rectangle(img, (x0, y0 + r), (x1, y1 - r), cor, -1)
    for cx, cy in ((x0 + r, y0 + r), (x1 - r, y0 + r), (x0 + r, y1 - r), (x1 - r, y1 - r)):
        cv2.circle(img, (cx, cy), r, cor, -1, cv2.LINE_AA)


def barra_calibracao(frame, hud, t, restante, total):
    """
    Cartão com barra de progresso na parte de baixo da tela. O fundo, os textos e o
    trilho vão no frame; o preenchimento vai na camada 'hud' (ganha o brilho neon).
    restante=None = contagem acabou e ainda falta mão: barra cheia pulsando em roxo.
    """
    h, w = frame.shape[:2]
    larg, alt = min(440, w - 60), 84
    x0, y0 = (w - larg) // 2, h - 175
    x1, y1 = x0 + larg, y0 + alt

    roi = frame[y0:y1, x0:x1]
    fundo = roi.copy()
    ret_arredondado(fundo, 0, 0, larg - 1, alt - 1, 14, COR_FUNDO_PAINEL)
    cv2.addWeighted(fundo, 0.8, roi, 0.2, 0, roi)

    if restante is None:
        titulo, cor = "MOSTRE AS DUAS MAOS", COR_MAO_E
        detalhe, direita = "AS DUAS MAOS PRECISAM APARECER NA CAMERA", ""
        progresso = 1.0
        cor_barra = escurecer(cor, 0.45 + 0.55 * (0.5 + 0.5 * math.sin(t * 6)))
    else:
        titulo, cor = "CALIBRANDO", COR_HUD_VERDE
        detalhe, direita = "FIQUE NA POSICAO NEUTRA, MAOS NA CAMERA", f"{restante:.1f} s"
        progresso = max(0.0, min(1.0, 1 - restante / total))
        cor_barra = cor

    texto(frame, titulo, (x0 + 20, y0 + 30), 0.62, cor, 2)
    if direita:
        tw = cv2.getTextSize(direita, FONTE, 0.62, 2)[0][0]
        texto(frame, direita, (x1 - 20 - tw, y0 + 30), 0.62, COR_NUCLEO, 2)
    texto(frame, detalhe, (x0 + 20, y0 + 51), 0.4, (165, 165, 165), 1, larg_max=larg - 40)

    bx0, bx1, yb, esp = x0 + 22, x1 - 22, y0 + 68, 10
    cv2.line(frame, (bx0, yb), (bx1, yb), (62, 48, 58), esp, cv2.LINE_AA)  # trilho
    xf = bx0 + int((bx1 - bx0) * progresso)
    if xf > bx0:
        cv2.line(hud, (bx0, yb), (xf, yb), cor_barra, esp, cv2.LINE_AA)  # pontas redondas


def painel_status(frame, estado, linhas, larg=440):
    """painel escuro com canto cortado; linhas = [(texto, cor), ...]"""
    alt = 62 + 24 * len(linhas)
    roi = frame[:alt + 1, :larg + 1]
    pts = np.array([(0, 0), (larg, 0), (larg, alt - 20), (larg - 20, alt), (0, alt)], np.int32)
    fundo = roi.copy()
    cv2.fillPoly(fundo, [pts], COR_FUNDO_PAINEL)
    cv2.addWeighted(fundo, 0.65, roi, 0.35, 0, roi)
    cv2.polylines(frame, [pts[1:]], False, COR_HUD, 1, cv2.LINE_AA)
    cv2.line(frame, (0, 1), (130, 1), COR_HUD_VERDE, 3)
    texto(frame, "BERTOS  //  CONTROLE POR GESTOS", (12, 20), 0.42, COR_HUD, 1)
    texto(frame, estado, (12, 46), 0.6, COR_HUD_VERDE, 2, larg_max=larg - 24)
    for i, (txt, cor) in enumerate(linhas):
        texto(frame, txt, (12, 72 + 24 * i), 0.5, cor, 1, larg_max=larg - 24)


def escala(d, dz, alcance):
    """-1..1 com zona morta"""
    if abs(d) < dz:
        return 0.0
    return math.copysign(min(1.0, (abs(d) - dz) / (alcance - dz)), d)


udp_ok = True  # False enquanto o envio estiver falhando (fora da rede RoboBatalha)


def enviar(acel, dire, arma, seq):
    """manda o comando pro robô; se a rede cair, avisa e continua (o robô para sozinho
    depois de 300 ms sem pacote)"""
    global udp_ok
    if not ENVIAR_UDP:
        return
    try:
        sock.sendto(struct.pack("<BbbbB", 0xAA, acel, dire, arma, seq & 0xFF), (IP_ROBO, PORTA))
        udp_ok = True
    except OSError as e:
        if udp_ok:
            print(f"AVISO: nao consegui enviar pro robo ({e}). O notebook esta na rede RoboBatalha?")
        udp_ok = False


def main():
    cal = None
    fase = "ociosa"  # ociosa | contando
    contando_desde = None
    cal_msg, cal_msg_ate = "", 0.0
    suav = {"r_dir": None, "x_dir": None, "r_arma": None}

    def ema(k, v):
        suav[k] = v if suav[k] is None else suav[k] + SUAVIZACAO * (v - suav[k])
        return suav[k]

    sim = SimRobo()
    cap = cv2.VideoCapture(CAMERA)
    if not cap.isOpened():
        print(f"ERRO: nao consegui abrir a camera {CAMERA}. Confira se a webcam esta ligada e "
              f"nao esta aberta em outro programa, ou mude CAMERA no topo do controle_mao.py.")
        return
    seq = 0
    ultimo = 0.0
    ultimo_log = 0.0
    periodo = 1.0 / FPS_ENVIO

    with mp_hands.Hands(max_num_hands=2, model_complexity=0,
                        min_detection_confidence=0.6, min_tracking_confidence=0.5) as hands, \
         mp_pose.Pose(model_complexity=0, min_detection_confidence=0.5,
                      min_tracking_confidence=0.5) as pose:
        while True:
            ok, frame = cap.read()
            if not ok:
                print("ERRO: a camera parou de mandar imagem (desconectada?). Encerrando.")
                break
            frame = cv2.flip(frame, 1)  # espelho: lado da tela = lado do corpo
            h, w = frame.shape[:2]
            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            rp = pose.process(rgb)
            rh = hands.process(rgb)
            tecla = cv2.waitKey(1) & 0xFF
            agora = time.time()

            acel = dire = arma = 0
            estado = "SEM CORPO"
            txt_d = txt_e = "--"
            mao_d = mao_e = None
            ab_d = ab_e = False
            r_d = x_d = r_e = None
            largura = 0
            progresso_cal = None
            contagem = None
            maos_ignoradas = 0
            desenhos = []  # (landmarks, "d" | "e" | None) — desenhados no fim, já com os valores

            if rp.pose_landmarks:
                plm = rp.pose_landmarks.landmark
                oe, od = sorted([plm[11], plm[12]], key=lambda p: p.x)
                largura = abs(od.x - oe.x) * w
                # o pulso acompanha o ombro do mesmo lado no Pose (16 com 12, 15 com 11),
                # então continua certo mesmo com os braços cruzados.
                if plm[12].x > plm[11].x:
                    pulso_d, pulso_e = plm[16], plm[15]
                else:
                    pulso_d, pulso_e = plm[15], plm[16]

                # identidade da mão = pulso do Pose mais próximo. Não usa o rótulo Left/Right
                # do Hands (às vezes rotula as duas mãos iguais e uma some) e ignora mãos
                # longe dos pulsos do corpo detectado (gente atrás do operador).
                maos = rh.multi_hand_landmarks or []
                pares = []
                if largura > 20:
                    for i, m in enumerate(maos):
                        for lado, pulso in (("d", pulso_d), ("e", pulso_e)):
                            d = dist(m.landmark[0], pulso, w, h) / largura
                            if d < MAO_DIST_MAX:
                                pares.append((d, i, lado))
                dono = {}
                for d, i, lado in sorted(pares):
                    if i not in dono and lado not in dono.values():
                        dono[i] = lado
                maos_ignoradas = len(maos) - len(dono)

                for i, m in enumerate(maos):
                    lado = dono.get(i)
                    desenhos.append((m.landmark, lado))
                    if lado == "d":
                        mao_d = m.landmark
                    elif lado == "e":
                        mao_e = m.landmark

                if largura > 20:
                    if mao_d:
                        r_d = ema("r_dir", palma(mao_d, w, h) / largura)
                        x_d = ema("x_dir", (mao_d[0].x - od.x) * w / largura)
                        ab_d = mao_aberta(mao_d, w, h)
                        txt_d = "aberta" if ab_d else "fechada"
                    else:
                        suav["r_dir"] = suav["x_dir"] = None
                    if mao_e:
                        r_e = ema("r_arma", palma(mao_e, w, h) / largura)
                        ab_e = mao_aberta(mao_e, w, h)
                        txt_e = "aberta" if ab_e else "fechada"
                    else:
                        suav["r_arma"] = None

                    # Enquanto fase == "contando", acel/dire/arma ficam em 0 (o robô para)
                    # porque só o branch de baixo os altera.
                    if fase == "ociosa":
                        if cal is None:
                            estado = "APERTE C PRA CALIBRAR"
                        else:
                            estado = "ATIVO"
                            if mao_d and ab_d:
                                acel = int(100 * escala(r_d / cal["r_dir"] - 1, PROF_DZ, PROF_ALCANCE))
                                curva = escala(x_d - cal["x_dir"], CURVA_DZ, CURVA_ALCANCE)
                                dire = int(100 * math.copysign(abs(curva) ** CURVA_EXPO, curva))
                            if mao_e and ab_e:
                                v = escala(r_e / cal["r_arma"] - 1, PROF_DZ, PROF_ALCANCE)
                                if v != 0:
                                    arma = int(math.copysign(ARMA_MIN + (100 - ARMA_MIN) * abs(v), v))
                else:
                    estado = "CORPO LONGE DEMAIS (ombros pequenos)"

            # 'c' (re)inicia a contagem; apertar de novo durante a contagem recomeça do zero
            if tecla == ord("c"):
                fase = "contando"
                contando_desde = agora

            if fase == "contando":
                acel = dire = arma = 0  # garante robô parado já no frame em que 'c' foi apertado
                restante = CAL_TIMER_SEGUNDOS - (agora - contando_desde)
                if restante > 0:
                    progresso_cal = 1 - restante / CAL_TIMER_SEGUNDOS
                    contagem = restante
                    estado = f"PARADO - CALIBRANDO EM {restante:.1f}s"
                elif r_d is not None and r_e is not None:
                    # r_d/r_e só existem com corpo detectado e as duas mãos atribuídas
                    cal = {"r_dir": r_d, "x_dir": x_d, "r_arma": r_e}
                    cal_msg, cal_msg_ate = "CALIBRADO!", agora + 1.5
                    fase = "ociosa"
                elif restante <= -CAL_JANELA_SEGUNDOS:
                    # passou a folga e ainda falta mão/corpo: desiste e diz o que faltou
                    falta = []
                    if not rp.pose_landmarks or largura <= 20:
                        falta.append("corpo")
                    if not mao_d:
                        falta.append("mao direita")
                    if not mao_e:
                        falta.append("mao esquerda")
                    cal_msg, cal_msg_ate = f"CALIBRACAO FALHOU: falta {' e '.join(falta)}", agora + 2.5
                    fase = "ociosa"
                else:
                    # contagem acabou mas falta mão neste frame: espera um pouco em vez de
                    # falhar de primeira (uma mão some por 1 frame com frequência)
                    progresso_cal = 1.0
                    estado = "MOSTRE AS DUAS MAOS PRA CAMERA"

            if DEBUG and agora - ultimo_log > 0.3:
                info = (f"fase={fase} cal={'ok' if cal else 'None'} "
                        f"mao_d={'sim' if mao_d else 'nao'} ab_d={ab_d} "
                        f"mao_e={'sim' if mao_e else 'nao'} ab_e={ab_e} "
                        f"progresso_cal={progresso_cal} "
                        f"maos_ignoradas={maos_ignoradas}")
                if cal and mao_d:
                    info += f" r_d/cal={r_d / cal['r_dir']:.2f} x_d-cal={x_d - cal['x_dir']:.2f}"
                if cal and mao_e:
                    info += f" r_e/cal={r_e / cal['r_arma']:.2f}"
                print(info)
                ultimo_log = agora

            if agora - ultimo >= periodo:
                enviar(acel, dire, arma, seq)
                seq += 1
                ultimo = agora

            # ---- visual: câmera escurecida + camada HUD com brilho neon ----
            estilo_camera(frame)
            hud = np.zeros_like(frame)
            for lm, lado in desenhos:
                if lado == "d":
                    linhas = (["LOCOMOCAO", f"ACEL {acel:+4d}", f"CURVA {dire:+4d}"] if ab_d
                              else ["LOCOMOCAO", "TRAVADO"])
                    hud_mao(hud, lm, w, h, COR_MAO_D, ab_d, agora, linhas, +1)
                elif lado == "e":
                    sentido = "HORARIO" if arma > 0 else "ANTI-HOR" if arma < 0 else "PARADA"
                    linhas = ["ARMA", f"{arma:+4d} {sentido}"] if ab_e else ["ARMA", "TRAVADA"]
                    hud_mao(hud, lm, w, h, COR_MAO_E, ab_e, agora, linhas, -1)
                else:  # mão de outra pessoa / longe do pulso: só um retículo apagado
                    desenhar_alvo(hud, lm, w, h, (70, 70, 70), tam=10)
            if fase == "contando":
                barra_calibracao(frame, hud, agora, contagem, CAL_TIMER_SEGUNDOS)
            if agora < cal_msg_ate:
                if cal_msg == "CALIBRADO!":
                    texto_centro(hud, "CALIBRADO", w // 2, h - 70, 1.3, COR_HUD_VERDE, 3)
                else:
                    texto_centro(hud, cal_msg, w // 2, h - 70, 0.8, COR_MAO_E, 2, larg_max=w - 40)
            cantos(hud, 4, 4, w - 4, h - 4, COR_HUD, tam=30, esp=2)
            frame = brilho(frame, hud)

            painel_status(frame, estado, [
                (f"LOCOMOCAO  {txt_d:<8} ACEL {acel:+4d}   CURVA {dire:+4d}", COR_MAO_D),
                (f"ARMA       {txt_e:<8} {arma:+4d}", COR_MAO_E),
            ])
            texto(frame, "C = CALIBRAR   R = RESETAR SIM   Q = SAIR", (12, h - 14), 0.42,
                  escurecer(COR_HUD_VERDE, 0.8))
            online = int(agora * 2) % 2 == 0
            udp_txt = "ROBO " + ("OFF" if not ENVIAR_UDP else "ON" if udp_ok else "SEM REDE")
            status = f"{udp_txt} | SIM {'ON' if SIMULAR else 'OFF'}"
            larg_status = cv2.getTextSize(status, FONTE, 0.42, 1)[0][0]
            cor_status = COR_MAO_E if ENVIAR_UDP and not udp_ok else escurecer(COR_HUD_VERDE, 0.8)
            cv2.circle(frame, (w - larg_status - 22, h - 19), 4,
                       COR_HUD if online else escurecer(COR_HUD, 0.4), -1, cv2.LINE_AA)
            texto(frame, status, (w - larg_status - 12, h - 14), 0.42, cor_status)
            if SIMULAR:
                if tecla == ord("r"):
                    sim.reset()
                sim.atualizar(acel, dire, arma, agora)
                painel = cv2.resize(sim.desenhar(acel, dire, arma), (h, h))
                frame = np.hstack([frame, painel])
            cv2.imshow("Controle por bracos", frame)
            if tecla == ord("q"):
                break

    enviar(0, 0, 0, 0)
    cap.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
