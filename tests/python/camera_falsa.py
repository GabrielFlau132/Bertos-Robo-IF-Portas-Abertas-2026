"""
Roda o controle_mao.main() sem webcam: câmera, MediaPipe Pose/Hands e relógio falsos.

Um "roteiro" diz, para cada frame f, qual tecla foi apertada e onde estão as mãos:
    roteiro(f) -> (tecla, [(cx, cy, tamanho_px, forma, espelhada), ...])
cx/cy normalizados (0..1) na imagem já espelhada; mão com cx > 0.5 = mão direita.
Cada frame avança 0,1 s no relógio falso.
"""
import os
import sys
from types import SimpleNamespace as P

import cv2
import numpy as np

RAIZ = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SAIDA = os.path.join(RAIZ, "tests", "saida")
sys.path.insert(0, RAIZ)
import controle_mao as c  # noqa: E402

# mão aberta com a palma pra câmera, em unidades de "tamanho" (x pra fora do polegar = +)
ABERTA = [(0, 1.0), (-0.35, 0.8), (-0.6, 0.55), (-0.8, 0.35), (-0.95, 0.15),
          (-0.3, 0.0), (-0.35, -0.45), (-0.38, -0.75), (-0.4, -1.0),
          (0, -0.05), (0, -0.55), (0, -0.9), (0, -1.15),
          (0.27, 0.0), (0.3, -0.45), (0.32, -0.75), (0.34, -0.98),
          (0.5, 0.12), (0.6, -0.2), (0.66, -0.42), (0.7, -0.6)]
FECHADA = [p if i < 5 or i in (5, 9, 13, 17) else (p[0] * 0.8, 0.05 if i % 4 in (3, 0) else -0.25)
           for i, p in enumerate(ABERTA)]

W, H = 640, 480
FUNDO = np.zeros((H, W, 3), np.uint8)
for y in range(H):
    FUNDO[y] = (95 + y // 12, 90 + y // 14, 85 + y // 16)
cv2.ellipse(FUNDO, (W // 2, H + 40), (190, 260), 0, 180, 360, (60, 55, 50), -1)  # tronco
cv2.circle(FUNDO, (W // 2, 150), 55, (70, 80, 110), -1)                         # cabeça


def _mao(cx, cy, s, forma, espelha):
    sx = -1 if espelha else 1
    return P(landmark=[P(x=(cx * W + sx * px * s) / W, y=(cy * H + py * s) / H) for px, py in forma])


def rodar(roteiro, capturar=(), nome_base="quadro"):
    """roda main() com o roteiro; salva PNG dos frames em 'capturar'. Retorna {frame: caminho}."""
    os.makedirs(SAIDA, exist_ok=True)
    st = {"f": -1}
    fotos = {}

    class Cap:
        def isOpened(self): return True
        def read(self):
            st["f"] += 1
            return True, cv2.flip(FUNDO, 1)
        def release(self): pass

    def corpo():
        plm = [P(x=0.5, y=0.5) for _ in range(33)]
        plm[11], plm[12] = P(x=0.62, y=0.52), P(x=0.38, y=0.52)
        plm[23], plm[24] = P(x=0.6, y=1.1), P(x=0.4, y=1.1)
        maos = roteiro(st["f"])[1]
        # pulsos do Pose em cima do punho de cada mão (15 = lado direito da tela)
        pulsos = {m[0] > 0.5: (m[0], m[1] + m[2] / H) for m in maos}
        plm[15] = P(x=pulsos.get(True, (0.7, 0.8))[0], y=pulsos.get(True, (0.7, 0.8))[1])
        plm[16] = P(x=pulsos.get(False, (0.3, 0.8))[0], y=pulsos.get(False, (0.3, 0.8))[1])
        return plm

    class Pose:
        def __init__(s, **k): pass
        def __enter__(s): return s
        def __exit__(s, *a): pass
        def process(s, rgb): return P(pose_landmarks=P(landmark=corpo()))

    class Hands(Pose):
        def process(s, rgb):
            return P(multi_hand_landmarks=[_mao(*m) for m in roteiro(st["f"])[1]], multi_handedness=None)

    def imshow(nome, img):
        if st["f"] in capturar:
            caminho = os.path.join(SAIDA, f"{nome_base}_{st['f']}.png")
            cv2.imwrite(caminho, img)
            fotos[st["f"]] = caminho

    c.cv2.VideoCapture = lambda i: Cap()
    c.cv2.imshow = imshow
    c.cv2.destroyAllWindows = lambda: None
    c.cv2.waitKey = lambda _: roteiro(st["f"])[0]
    c.time.time = lambda: 1000.0 + st["f"] * 0.1
    c.mp_hands.Hands, c.mp_pose.Pose = Hands, Pose
    c.DEBUG = False
    c.main()
    return fotos


def roteiro_padrao(f):
    """calibra (c no frame 5, contagem até o 55), neutro até o 70, depois mão direita pra
    frente e pro lado e mão esquerda pra trás; sai (q) no frame 100."""
    tecla = ord("c") if f == 5 else ord("q") if f >= 100 else 255
    if f < 70:
        return tecla, [(0.66, 0.5, 42, ABERTA, False), (0.34, 0.5, 42, ABERTA, True)]
    return tecla, [(0.72, 0.48, 55, ABERTA, False), (0.33, 0.52, 34, ABERTA, True)]


def roteiro_falta_mao(f):
    """calibra, mas a mão esquerda some antes do fim da contagem."""
    tecla = ord("c") if f == 5 else ord("q") if f >= 70 else 255
    maos = [(0.66, 0.5, 42, ABERTA, False)]
    if f < 50:
        maos.append((0.34, 0.5, 42, ABERTA, True))
    return tecla, maos
