# sim_robo.py — robô virtual (vista de cima) para testar o reconhecimento sem hardware
# Tração diferencial: esq = acel + dire, dir = acel - dire (mistura tipo tanque)

import math

import cv2
import numpy as np


class SimRobo:
    def __init__(self, tam=480, vel_max=250.0, giro_max=3.0, arma_max=40.0):
        self.tam = tam
        self.vel_max = vel_max    # px/s com 100% de aceleração
        self.giro_max = giro_max  # rad/s com 100% de curva
        self.arma_max = arma_max  # rad/s da arma com 100%
        self.reset()

    def reset(self):
        self.x = self.y = self.tam / 2
        self.th = -math.pi / 2    # apontando pra cima
        self.ang_arma = 0.0
        self.rastro = []
        self.t = None

    def atualizar(self, acel, dire, arma, agora):
        dt = 0.0 if self.t is None else min(0.1, agora - self.t)
        self.t = agora
        esq = max(-100, min(100, acel + dire))
        dir_ = max(-100, min(100, acel - dire))
        v = (esq + dir_) / 200 * self.vel_max
        w = (esq - dir_) / 200 * self.giro_max
        self.th += w * dt
        self.x = (self.x + v * math.cos(self.th) * dt) % self.tam
        self.y = (self.y + v * math.sin(self.th) * dt) % self.tam
        self.ang_arma += arma / 100 * self.arma_max * dt
        self.rastro.append((int(self.x), int(self.y)))
        self.rastro = self.rastro[-300:]

    def _barra(self, img, y, nome, val, cor):
        cx, larg = self.tam // 2 + 40, 150
        cv2.putText(img, f"{nome} {val:+4d}", (10, y + 12), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (220, 220, 220), 1)
        cv2.rectangle(img, (cx - larg, y), (cx + larg, y + 14), (80, 80, 80), 1)
        cv2.rectangle(img, (cx, y), (cx + int(larg * val / 100), y + 14), cor, -1)
        cv2.line(img, (cx, y - 2), (cx, y + 16), (200, 200, 200), 1)

    def desenhar(self, acel, dire, arma):
        t = self.tam
        img = np.full((t, t, 3), 30, np.uint8)
        for i in range(0, t, 40):
            cv2.line(img, (i, 0), (i, t), (45, 45, 45), 1)
            cv2.line(img, (0, i), (t, i), (45, 45, 45), 1)

        for p, q in zip(self.rastro, self.rastro[1:]):
            if abs(p[0] - q[0]) < 50 and abs(p[1] - q[1]) < 50:  # não liga ponto quando dá a volta na borda
                cv2.line(img, p, q, (150, 70, 110), 1)  # rastro roxo apagado

        caixa = cv2.boxPoints(((self.x, self.y), (60, 44), math.degrees(self.th)))
        cv2.fillPoly(img, [caixa.astype(np.int32)], (70, 110, 70))
        cv2.polylines(img, [caixa.astype(np.int32)], True, (140, 220, 140), 2)

        fx = self.x + 38 * math.cos(self.th)
        fy = self.y + 38 * math.sin(self.th)
        cor_arma = (255, 100, 190) if arma else (120, 120, 120)  # roxo (mesma cor da mão da arma)
        dx, dy = 22 * math.cos(self.ang_arma), 22 * math.sin(self.ang_arma)
        cv2.line(img, (int(fx - dx), int(fy - dy)), (int(fx + dx), int(fy + dy)), cor_arma, 4)
        cv2.circle(img, (int(fx), int(fy)), 5, cor_arma, -1)

        sentido = "HORARIO" if arma > 0 else "ANTI-HORARIO" if arma < 0 else "PARADA"
        self._barra(img, t - 80, "acel", acel, (80, 200, 80))
        self._barra(img, t - 55, "dir ", dire, (200, 160, 60))
        self._barra(img, t - 30, "arma", arma, (255, 100, 190))
        cv2.putText(img, f"arma: {sentido}", (10, 25), cv2.FONT_HERSHEY_SIMPLEX, 0.6, cor_arma, 2)
        cv2.putText(img, "r = resetar robo", (t - 150, 25), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (200, 200, 200), 1)
        return img
