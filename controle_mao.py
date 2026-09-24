# controle_mao.py — controle por dois braços (MediaPipe Hands + Pose)
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
# Calibrar: cruze os braços em X (pulsos trocados de lado, acima do quadril) e
# segure a posição neutra de mãos abertas quando a contagem de 3s terminar.
# 'c' = calibra na hora (as duas mãos abertas, palma pra câmera) | 'q' = sair
# UDP 30x/s: [0xAA, aceleracao(int8), direcao(int8), arma(int8, sinal = sentido), seq(uint8)]
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
ENVIAR_UDP = False  # True quando o ESP32 estiver ligado e o notebook na rede RoboBatalha
DEBUG = True         # imprime diagnóstico da calibração/estado no terminal

IP_ROBO = "192.168.4.1"
PORTA = 4210
FPS_ENVIO = 30

PROF_DZ = 0.12        # variação de tamanho da palma ignorada (12%)
PROF_ALCANCE = 0.45   # variação que dá 100% (45%)
CURVA_DZ = 0.15       # em larguras de ombro
CURVA_ALCANCE = 0.7
ARMA_MIN = 70         # % assim que sai da zona morta
SUAVIZACAO = 0.4      # 0..1: maior = mais rápido e mais ruidoso
CAL_AUTO_SEGUNDOS = 3.0    # depois do X com os bracos, conta esse tempo antes de exigir maos abertas
CAL_JANELA_SEGUNDOS = 2.0  # depois da contagem, tolerancia p/ abrir as duas maos
CAL_CONFIRMA_SEGUNDOS = 0.2  # maos precisam ficar abertas por esse tempo seguido p/ confirmar

COR_HUD = (0, 30, 220)       # vermelho (BGR) - acentos do HUD
COR_HUD_OURO = (0, 170, 255)  # dourado - detalhe secundario

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
mp_hands = mp.solutions.hands
mp_pose = mp.solutions.pose
mp_draw = mp.solutions.drawing_utils
POSE_SEM_ROSTO = [c for c in mp_pose.POSE_CONNECTIONS if c[0] > 10 and c[1] > 10]


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


def pose_cruzada(plm):
    """braços em X: pulsos trocados de lado e acima do quadril"""
    return (plm[16].x < plm[11].x and plm[15].x > plm[12].x and
            plm[15].y < plm[23].y and plm[16].y < plm[24].y)


def escala(d, dz, alcance):
    """-1..1 com zona morta"""
    if abs(d) < dz:
        return 0.0
    return math.copysign(min(1.0, (abs(d) - dz) / (alcance - dz)), d)


def enviar(acel, dire, arma, seq):
    if not ENVIAR_UDP:
        return
    sock.sendto(struct.pack("<BbbbB", 0xAA, acel, dire, arma, seq & 0xFF), (IP_ROBO, PORTA))


def main():
    cal = None
    fase = "ociosa"  # ociosa | contando | aguardando
    segurando_desde = None
    aguardando_ate = None
    abertas_desde = None
    cruzado_antes = False
    cal_msg, cal_msg_ate = "", 0.0
    suav = {"r_dir": None, "x_dir": None, "r_arma": None}

    def ema(k, v):
        suav[k] = v if suav[k] is None else suav[k] + SUAVIZACAO * (v - suav[k])
        return suav[k]

    sim = SimRobo()
    cap = cv2.VideoCapture(0)
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
            cruzado = None

            if rp.pose_landmarks:
                plm = rp.pose_landmarks.landmark
                for a, b in POSE_SEM_ROSTO:
                    pa, pb = plm[a], plm[b]
                    cv2.line(frame, (int(pa.x * w), int(pa.y * h)), (int(pb.x * w), int(pb.y * h)),
                              COR_HUD, 2)
                for i in range(11, 33):
                    p = plm[i]
                    cv2.circle(frame, (int(p.x * w), int(p.y * h)), 3, COR_HUD_OURO, -1)

                oe, od = sorted([plm[11], plm[12]], key=lambda p: p.x)
                largura = abs(od.x - oe.x) * w
                meio = (oe.x + od.x) / 2

                # identidade da mão pelo rótulo Left/Right do MediaPipe (funciona com os
                # braços cruzados; a posição em relação a 'meio' era só um fallback).
                handedness = rh.multi_handedness or []
                for i, m in enumerate(rh.multi_hand_landmarks or []):
                    label = handedness[i].classification[0].label if i < len(handedness) else None
                    if label is None:
                        label = "Right" if m.landmark[0].x > meio else "Left"
                    cor = (80, 255, 120) if label == "Right" else (255, 80, 220)
                    mp_draw.draw_landmarks(
                        frame, m, mp_hands.HAND_CONNECTIONS,
                        mp_draw.DrawingSpec(color=cor, thickness=2, circle_radius=3),
                        mp_draw.DrawingSpec(color=cor, thickness=2))
                    desenhar_alvo(frame, m.landmark, w, h, cor)
                    if label == "Right":
                        mao_d = m.landmark
                    else:
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

                    # o X é checado sempre (mesmo já calibrado, pra dar pra recalibrar), mas só
                    # dispara numa borda de subida (False->True) — sem isso, se a calibração
                    # falhasse com os braços ainda cruzados, a contagem reiniciava na hora,
                    # dando a impressão de "fica tentando e não consegue" (loop instantâneo).
                    cruzado = pose_cruzada(plm)
                    if fase == "ociosa" and cruzado and not cruzado_antes:
                        fase = "contando"
                        segurando_desde = agora
                    cruzado_antes = cruzado

                    # Enquanto fase != "ociosa", acel/dire/arma ficam em 0 (o robô para)
                    # porque só o branch de baixo ("ociosa" com cal definido) os altera.
                    if fase == "contando":
                        restante = CAL_AUTO_SEGUNDOS - (agora - segurando_desde)
                        progresso_cal = max(0.0, min(1.0, 1 - restante / CAL_AUTO_SEGUNDOS))
                        if restante <= 0:
                            # não exige mais mãos abertas neste frame exato: abre uma janela
                            # de tolerância, porque descruzar os braços e abrir as mãos leva
                            # um instante e raramente cai certinho no frame da virada dos 3s.
                            fase = "aguardando"
                            aguardando_ate = agora + CAL_JANELA_SEGUNDOS
                            abertas_desde = None
                        else:
                            estado = f"X DETECTADO — PARADO, CALIBRANDO EM {restante:.1f}s"

                    if fase == "aguardando":
                        ambas_abertas = bool(mao_d and mao_e and ab_d and ab_e)
                        if ambas_abertas:
                            if abertas_desde is None:
                                abertas_desde = agora
                            elif agora - abertas_desde >= CAL_CONFIRMA_SEGUNDOS:
                                cal = {"r_dir": r_d, "x_dir": x_d, "r_arma": r_e}
                                cal_msg, cal_msg_ate = "CALIBRADO!", agora + 1.5
                                fase = "ociosa"
                        else:
                            abertas_desde = None

                        if fase == "aguardando":
                            if agora >= aguardando_ate:
                                cal_msg = "CALIBRACAO FALHOU: abra as duas maos"
                                cal_msg_ate = agora + 1.5
                                fase = "ociosa"
                            else:
                                progresso_cal = max(0.0, min(1.0, 1 - (aguardando_ate - agora) / CAL_JANELA_SEGUNDOS))
                                estado = f"ABRA AS DUAS MAOS PRA CALIBRAR ({aguardando_ate - agora:.1f}s)"

                    if fase == "ociosa":
                        if cal is None:
                            estado = "FACA UM X COM OS BRACOS PRA CALIBRAR (ou aperte C)"
                        else:
                            estado = "ATIVO"
                            if mao_d and ab_d:
                                acel = int(100 * escala(r_d / cal["r_dir"] - 1, PROF_DZ, PROF_ALCANCE))
                                dire = int(100 * escala(x_d - cal["x_dir"], CURVA_DZ, CURVA_ALCANCE))
                            if mao_e and ab_e:
                                v = escala(r_e / cal["r_arma"] - 1, PROF_DZ, PROF_ALCANCE)
                                if v != 0:
                                    arma = int(math.copysign(ARMA_MIN + (100 - ARMA_MIN) * abs(v), v))
                else:
                    estado = "CORPO LONGE DEMAIS (ombros pequenos)"

            if tecla == ord("c"):
                if not rp.pose_landmarks or largura <= 20:
                    cal_msg, cal_msg_ate = "C RECUSADO: corpo nao detectado direito", agora + 1.5
                elif not mao_d or not mao_e:
                    falta = []
                    if not mao_d:
                        falta.append("direita")
                    if not mao_e:
                        falta.append("esquerda")
                    cal_msg, cal_msg_ate = f"C RECUSADO: falta mao {' e '.join(falta)}", agora + 1.5
                else:
                    cal = {"r_dir": r_d, "x_dir": x_d, "r_arma": r_e}
                    cal_msg, cal_msg_ate = "CALIBRADO!", agora + 1.5
                    fase = "ociosa"

            if DEBUG and agora - ultimo_log > 0.3:
                info = (f"fase={fase} cal={'ok' if cal else 'None'} "
                        f"mao_d={'sim' if mao_d else 'nao'} ab_d={ab_d} "
                        f"mao_e={'sim' if mao_e else 'nao'} ab_e={ab_e} "
                        f"cruzado={cruzado} "
                        f"progresso_cal={progresso_cal}")
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

            painel_h = 112 if progresso_cal is None else 128
            overlay = frame.copy()
            cv2.rectangle(overlay, (0, 0), (420, painel_h), (10, 10, 10), -1)
            frame = cv2.addWeighted(overlay, 0.45, frame, 0.55, 0)
            cv2.line(frame, (0, painel_h), (420, painel_h), COR_HUD, 1)

            cv2.putText(frame, estado, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, COR_HUD_OURO, 2)
            cv2.putText(frame, f"D:{txt_d} acel={acel} dir={dire}", (10, 60),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (80, 255, 120), 2)
            cv2.putText(frame, f"E:{txt_e} arma={arma}", (10, 90),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 80, 220), 2)
            if progresso_cal is not None:
                cv2.rectangle(frame, (10, 105), (410, 118), (80, 80, 80), 1)
                cv2.rectangle(frame, (10, 105), (10 + int(400 * progresso_cal), 118), COR_HUD, -1)
            cv2.putText(frame, "c = calibrar | q = sair", (10, h - 15),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)
            if agora < cal_msg_ate:
                cor = (0, 255, 0) if cal_msg == "CALIBRADO!" else (0, 0, 255)
                cv2.putText(frame, cal_msg, (10, h - 45),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.7, cor, 2)
            cantos(frame, 4, 4, w - 4, h - 4, COR_HUD, tam=30, esp=2)
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
