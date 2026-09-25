@echo off
rem Duplo clique pra abrir o controle por gestos (webcam + HUD + envio pro robo).
rem Antes: notebook na rede Wi-Fi RoboBatalha (senha 12345678), robo ligado, R1 no controle.
cd /d "%~dp0"
if not exist "venv\Scripts\python.exe" (
    echo venv nao encontrado. Rode primeiro:
    echo   powershell -ExecutionPolicy Bypass -File scripts\preparar_ambiente.ps1
    pause
    exit /b 1
)
"venv\Scripts\python.exe" controle_mao.py
if errorlevel 1 pause
