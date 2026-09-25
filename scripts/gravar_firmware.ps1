# gravar_firmware.ps1 - compila e grava um sketch na ESP32 (acha a porta sozinho).
#
# Uso (na raiz do projeto, ESP32 no USB, FORA do robo):
#   powershell -ExecutionPolicy Bypass -File scripts\gravar_firmware.ps1
#   powershell -ExecutionPolicy Bypass -File scripts\gravar_firmware.ps1 -Sketch descobrir_parametros_controle
#   powershell -ExecutionPolicy Bypass -File scripts\gravar_firmware.ps1 -Porta COM11
#   -SoCompilar   so compila (confere se nao tem erro), nao grava
# Depois de gravar o codigo_robo_controle_p3, le o serial por alguns segundos pra confirmar
# que a ESP32 esta rodando (linha "[wifi] rede RoboBatalha ...").
param([string]$Sketch = 'codigo_robo_controle_p3', [string]$Porta = '', [switch]$SoCompilar)

$raiz = Split-Path $PSScriptRoot -Parent
$fqbn = 'esp32-bluepad32:esp32:esp32'
$pasta = Join-Path $raiz "firmware\$Sketch"
$build = Join-Path $env:TEMP "bertos_build_$Sketch"
if (-not (Test-Path $pasta)) { Write-Host "Sketch nao existe: $pasta" -ForegroundColor Red; exit 1 }

Write-Host "Compilando $Sketch ..."
arduino-cli compile --fqbn $fqbn --build-path $build $pasta 2>&1 | Where-Object { $_ -match 'error|Sketch uses|Global' }
if ($LASTEXITCODE -ne 0) { Write-Host "ERRO de compilacao." -ForegroundColor Red; exit 1 }
if ($SoCompilar) { Write-Host "Compilou ok." -ForegroundColor Green; exit 0 }

if (-not $Porta) {
    $Porta = (arduino-cli board list 2>&1 | Select-String 'Serial Port \(USB\)' | ForEach-Object { ($_ -split '\s+')[0] } | Select-Object -First 1)
}
if (-not $Porta) {
    Write-Host "ESP32 nao encontrada no USB. Confira o cabo (tem que ser de dados) e o driver CP210x" -ForegroundColor Red
    Write-Host "(Gerenciador de Dispositivos: 'CP2102 USB to UART Bridge' com triangulo amarelo = falta driver)." -ForegroundColor Red
    exit 1
}
Write-Host "Gravando na $Porta ... (se travar em 'Connecting...', segure o botao BOOT da ESP32)"
arduino-cli upload -p $Porta --fqbn $fqbn --input-dir $build $pasta 2>&1 | Where-Object { $_ -notmatch '^Writing at' } | Select-Object -Last 3
if ($LASTEXITCODE -ne 0) { Write-Host "ERRO ao gravar." -ForegroundColor Red; exit 1 }
Write-Host "Gravado." -ForegroundColor Green

if ($Sketch -eq 'codigo_robo_controle_p3') {
    Start-Sleep -Milliseconds 500
    try {
        $p = New-Object System.IO.Ports.SerialPort $Porta, 115200
        $p.DtrEnable = $false; $p.RtsEnable = $false; $p.Open()
        $fim = (Get-Date).AddSeconds(7); $t = ''
        while ((Get-Date) -lt $fim) { $t += $p.ReadExisting(); Start-Sleep -Milliseconds 100 }
        $p.Close()
        $linha = ($t -split "`r?`n" | Where-Object { $_ -match '\[wifi\]' } | Select-Object -First 1)
        if ($linha) { Write-Host "ESP32 rodando: $linha" -ForegroundColor Green }
        else { Write-Host "Nao vi a linha [wifi] no serial (pode ser so o monitor; teste no robo)." -ForegroundColor Yellow }
    } catch { Write-Host "Nao deu pra ler o serial: $_" -ForegroundColor Yellow }
}
