# preparar_ambiente.ps1 - prepara (ou confere) tudo que o projeto precisa neste PC.
#
# Uso (na raiz do projeto):
#   powershell -ExecutionPolicy Bypass -File scripts\preparar_ambiente.ps1
#   powershell -ExecutionPolicy Bypass -File scripts\preparar_ambiente.ps1 -Arduino   # tambem ESP32
#   powershell -ExecutionPolicy Bypass -File scripts\preparar_ambiente.ps1 -RecriarVenv
#
# Python: precisa do 3.12 (mediapipe 0.10.14 nao existe pra versoes mais novas e as
# versoes novas do mediapipe nao tem mp.solutions). Cria venv\ e instala requirements-lock.txt
# (versoes exatas; cai pro requirements.txt se o lock nao existir).
# -Arduino: confere arduino-cli, URLs de placas, core esp32-bluepad32 e driver CP210x.
param([switch]$Arduino, [switch]$RecriarVenv)

$raiz = Split-Path $PSScriptRoot -Parent
Set-Location $raiz
$ok = $true
function Ok($t) { Write-Host "[ ok ] $t" -ForegroundColor Green }
function Aviso($t) { Write-Host "[ !! ] $t" -ForegroundColor Yellow }
function Erro($t) { Write-Host "[ERRO] $t" -ForegroundColor Red; $script:ok = $false }

# ---------------------------------------------------------------- Python 3.12
function Versao($exe) { try { (& $exe -c "import sys; print('%d.%d' % sys.version_info[:2])" 2>$null) } catch { $null } }

$py312 = $null
$candidatos = @(
    "$env:LOCALAPPDATA\Programs\Python\Python312\python.exe",
    "C:\Python312\python.exe",
    "C:\Program Files\Python312\python.exe"
)
try { $viaPy = (& py -3.12 -c "import sys; print(sys.executable)" 2>$null); if ($viaPy) { $candidatos = @($viaPy) + $candidatos } } catch {}
try { $viaPath = (Get-Command python -ErrorAction SilentlyContinue).Source; if ($viaPath) { $candidatos += $viaPath } } catch {}
foreach ($c in $candidatos) {
    if ($c -and (Test-Path $c) -and ((Versao $c) -eq '3.12')) { $py312 = $c; break }
}
if ($py312) { Ok "Python 3.12: $py312" }
else {
    Erro "Python 3.12 nao encontrado. Instale (depois rode este script de novo):"
    Write-Host "       winget install -e --id Python.Python.3.12" -ForegroundColor Yellow
    Write-Host "       ou https://www.python.org/downloads/ (versao 3.12.x, marcar 'Add to PATH')" -ForegroundColor Yellow
}

# ---------------------------------------------------------------- venv
$venvPy = Join-Path $raiz 'venv\Scripts\python.exe'
if ((Test-Path $venvPy) -and ((Versao $venvPy) -ne '3.12')) {
    Aviso "venv existente nao e Python 3.12 (ou esta quebrado): sera recriado."
    $RecriarVenv = $true
}
if ($RecriarVenv -and (Test-Path 'venv')) { Remove-Item -Recurse -Force 'venv' }
if (-not (Test-Path $venvPy)) {
    if ($py312) {
        Write-Host "Criando venv com $py312 ..."
        & $py312 -m venv venv
    }
}
if (Test-Path $venvPy) {
    # requirements-lock.txt = versoes exatas do ambiente que funcionou; requirements.txt = so as diretas
    $req = if (Test-Path 'requirements-lock.txt') { 'requirements-lock.txt' } else { 'requirements.txt' }
    Write-Host "Instalando/conferindo dependencias ($req) ..."
    & $venvPy -m pip install --disable-pip-version-check -q -r $req
    $teste = & $venvPy -c "import mediapipe as mp, cv2, numpy; assert hasattr(mp, 'solutions'); print(mp.__version__, cv2.__version__, numpy.__version__)" 2>&1
    if ($LASTEXITCODE -eq 0) { Ok "venv pronto (mediapipe, opencv, numpy): $teste" }
    else { Erro "venv com problema nas bibliotecas: $teste" }
} else {
    Erro "venv nao foi criado."
}

# ---------------------------------------------------------------- webcam
if (Test-Path $venvPy) {
    $cam = & $venvPy -c "import cv2; c=cv2.VideoCapture(0); print('ok' if c.isOpened() and c.read()[0] else 'falhou'); c.release()" 2>$null
    if ($cam -eq 'ok') { Ok "webcam 0 abre e manda imagem" }
    else { Aviso "webcam 0 nao abriu (outra camera? em uso por outro programa?). Ajuste CAMERA no controle_mao.py." }
}

# ---------------------------------------------------------------- Arduino / ESP32
if ($Arduino) {
    $cli = Get-Command arduino-cli -ErrorAction SilentlyContinue
    if (-not $cli) {
        Erro "arduino-cli nao encontrado. Instale: winget install -e --id ArduinoSA.CLI  (ou use a Arduino IDE)"
    } else {
        Ok "arduino-cli: $($cli.Source)"
        $urls = @(
            'https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json',
            'https://raw.githubusercontent.com/ricardoquesada/esp32-arduino-lib-builder/master/bluepad32_files/package_esp32_bluepad32_index.json'
        )
        $atuais = (arduino-cli config get board_manager.additional_urls 2>$null) -join "`n"
        foreach ($u in $urls) { if ($atuais -notmatch [regex]::Escape($u)) { arduino-cli config add board_manager.additional_urls $u | Out-Null } }
        $cores = (arduino-cli core list 2>$null) -join "`n"
        if ($cores -notmatch 'esp32-bluepad32:esp32') {
            Write-Host "Instalando o core esp32-bluepad32:esp32@4.1.0 (download grande, alguns minutos) ..."
            arduino-cli core update-index | Out-Null
            arduino-cli core install esp32-bluepad32:esp32@4.1.0
        }
        $cores = (arduino-cli core list 2>$null) -join "`n"
        if ($cores -match 'esp32-bluepad32:esp32\s+(\S+)') { Ok "core esp32-bluepad32:esp32 $($Matches[1])" } else { Erro "core esp32-bluepad32 nao instalado" }
    }
    $cp210x = Get-CimInstance Win32_PnPEntity | Where-Object { $_.PNPDeviceID -match 'VID_10C4&PID_EA60' }
    if (-not $cp210x) { Aviso "ESP32 nao esta no USB agora (nao deu pra conferir o driver CP210x)." }
    elseif ($cp210x | Where-Object { $_.ConfigManagerErrorCode -ne 0 }) {
        Erro "ESP32 ligada mas SEM DRIVER (CP210x). Instale: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers"
    } else { Ok "ESP32 no USB com driver: $(($cp210x | Select-Object -First 1).Name)" }
}

Write-Host ""
if ($ok) { Write-Host "AMBIENTE PRONTO. Rodar os gestos: rodar_gestos.bat (ou venv\Scripts\python.exe controle_mao.py)" -ForegroundColor Green }
else { Write-Host "Ha itens com ERRO acima." -ForegroundColor Red; exit 1 }
