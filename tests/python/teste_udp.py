"""
Teste de ponta a ponta do controle_mao.py sem webcam e sem robô: roda com a câmera falsa,
manda os pacotes UDP pra um receptor local e confere formato e valores.
Uso: .\\venv\\Scripts\\python.exe tests\\python\\teste_udp.py   (sai com código 1 se falhar)
"""
import os
import socket
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import camera_falsa as cf  # noqa: E402

c = cf.c
receptor = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
receptor.bind(("127.0.0.1", 0))
receptor.setblocking(False)
c.ENVIAR_UDP = True
c.IP_ROBO, c.PORTA = "127.0.0.1", receptor.getsockname()[1]

cf.rodar(cf.roteiro_padrao)

pacotes = []
while True:
    try:
        pacotes.append(receptor.recv(64))
    except BlockingIOError:
        break

falhas = []
if len(pacotes) < 90:
    falhas.append(f"poucos pacotes: {len(pacotes)} (esperado ~102)")
if not all(len(p) == 5 and p[0] == 0xAA for p in pacotes):
    falhas.append("pacote com formato errado (5 bytes, começando com 0xAA)")
valores = [struct.unpack("<Bbbb", p[:4])[1:] for p in pacotes]
if any(v != (0, 0, 0) for v in valores[:60]):
    falhas.append("robô deveria ficar parado (0,0,0) até o fim da calibração")
if not any(v[0] > 30 and v[2] < -60 for v in valores[70:]):
    falhas.append("depois de calibrar, mão direita pra frente e esquerda pra trás deveriam dar acel>30 e arma<-60")
if valores[-1] != (0, 0, 0):
    falhas.append("o último pacote (ao sair) tem que ser de parada")

anterior = None
for i, v in enumerate(valores):
    if v != anterior:
        print(f"  pacote {i:3d}: acel={v[0]:+4d} dire={v[1]:+4d} arma={v[2]:+4d}")
        anterior = v
print(f"{len(pacotes)} pacotes recebidos")
if falhas:
    print("FALHOU:\n  - " + "\n  - ".join(falhas))
    sys.exit(1)
print("TUDO OK")
