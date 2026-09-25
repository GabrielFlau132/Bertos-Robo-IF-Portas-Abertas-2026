# firmware/

Todos os sketches usam a placa **ESP32 + Bluepad32** (`esp32-bluepad32:esp32:esp32`). Veja o [README principal](../README.md) para instalar e gravar.

| Pasta | Origem | Situação | Para que serve |
|---|---|---|---|
| `codigo_robo_controle_p3/` | oficial + mudanças | **firmware do robô** | L1 = modo controle (R2/L2 frente/trás, analógico direito direção, O/□/△ arma), R1 = modo gestos (comandos do `controle_mao.py` por Wi-Fi/UDP, via `modo_mao.h`), setas (padrão = seta ↓), L3+R3 trava. Tem limite de velocidade por modo (`LIMITE_*` no `parametros.h`). O modo gestos usa uma mistura de motores própria, porque a oficial vira para o lado errado com comando proporcional. As mudanças estão descritas no topo do `.ino`. Testado no robô |
| `descobrir_parametros_controle/` | oficial, sem mudança | utilitário | Pareia o controle (apaga pareamentos antigos) e imprime no serial o MAC e os valores de todos os botões e eixos. Serviu para medir R2/L2 e o analógico direito |
| `filtro_mac_controle/` | oficial, sem mudança | utilitário (não usado ainda) | Grava na ESP32 uma lista de controles permitidos (fica salva mesmo trocando o firmware). Troque o MAC no código pelo do seu controle antes de gravar |
| `filtro_com_descobrir_parametros/` | oficial, sem mudança | utilitário (não usado ainda) | Os dois anteriores juntos |
| `teste_motores/` | feito aqui | diagnóstico | Cada seta liga um canal de motor devagar enquanto estiver apertada, e R2/L2 testam os gatilhos. Serve para descobrir, sem o PC, o que está ligado em cada borne |
| `bertos_controle/` | feito aqui | **obsoleto, não gravar** | Reescrita com rampa, tempo morto e trava corrigida, abandonada para ficar fiel ao código oficial. A pinagem dele reflete uma ligação errada dos motores que já foi corrigida. Serve só de referência para a rampa e o tempo morto |

Base oficial: [nrc-cupim/start-automacao-eletrica](https://github.com/nrc-cupim/start-automacao-eletrica), pasta `Codigos/`, commit `a52bbf3`.
