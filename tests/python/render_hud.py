"""
Gera imagens do HUD sem webcam, pra conferir mudanças visuais (cores, barra, rótulos).
Uso: .\\venv\\Scripts\\python.exe tests\\python\\render_hud.py   -> PNGs em tests\\saida\\
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import camera_falsa as cf  # noqa: E402

cf.c.ENVIAR_UDP = False
fotos = {}
fotos.update({f"calibrando": p for p in cf.rodar(cf.roteiro_padrao, {30}, "calibrando").values()})
fotos.update({f"pilotando": p for p in cf.rodar(cf.roteiro_padrao, {95}, "pilotando").values()})
fotos.update({f"falta_mao": p for p in cf.rodar(cf.roteiro_falta_mao, {60}, "falta_mao").values()})
for nome, caminho in fotos.items():
    print(f"{nome:12s} {caminho}")
