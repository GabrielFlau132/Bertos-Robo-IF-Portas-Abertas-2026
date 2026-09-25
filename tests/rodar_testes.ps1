# rodar_testes.ps1 — roda todos os testes sem robô e sem webcam.
# Uso (na raiz do projeto):  powershell -ExecutionPolicy Bypass -File tests\rodar_testes.ps1
#   -SemFirmware  pula os testes do firmware (precisam do g++)
#   -SemPython    pula os testes do Python (precisam do venv)
param([switch]$SemFirmware, [switch]$SemPython)

$ErrorActionPreference = 'Continue'
$raiz = Split-Path $PSScriptRoot -Parent
$falhas = 0

function Titulo($t) { Write-Host "`n=== $t" -ForegroundColor Magenta }

# ------------------------------------------------------------------ firmware
if (-not $SemFirmware) {
    if (-not (Get-Command g++ -ErrorAction SilentlyContinue) -and (Test-Path 'C:\msys64\ucrt64\bin\g++.exe')) {
        $env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
    }
    if (-not (Get-Command g++ -ErrorAction SilentlyContinue)) {
        Write-Host "g++ nao encontrado: pulando testes do firmware (instale o MSYS2/MinGW ou use -SemFirmware)." -ForegroundColor Yellow
    } else {
        $mock = Join-Path $raiz 'tests\firmware'
        $fw = Join-Path $raiz 'firmware\codigo_robo_controle_p3'
        $out = Join-Path $env:TEMP 'bertos_testes'
        New-Item -ItemType Directory -Force $out | Out-Null
        $sem = @('-DLIMITE_CONTROLE_FRENTE=100', '-DLIMITE_CONTROLE_GIRO=100', '-DLIMITE_GESTOS_FRENTE=100', '-DLIMITE_GESTOS_GIRO=100')

        Titulo 'Firmware: integracao controle + gestos (limites em 100%)'
        g++ -std=c++17 -w @sem -I $mock -I $fw "$mock\teste_gestos.cpp" -o "$out\gestos.exe"
        & "$out\gestos.exe" | Select-String 'FALHOU|TUDO OK|HOUVE' | ForEach-Object { $_.Line }
        if ($LASTEXITCODE -ne 0) { $falhas++ }

        Titulo 'Firmware: limites de velocidade (valores do parametros.h)'
        g++ -std=c++17 -w -I $mock -I $fw "$mock\teste_limites.cpp" -o "$out\limites.exe"
        & "$out\limites.exe"
        if ($LASTEXITCODE -ne 0) { $falhas++ }

        Titulo 'Firmware: curva de resposta dos gestos (informativo)'
        g++ -std=c++17 -w -I $mock -I $fw "$mock\curva_gestos.cpp" -o "$out\curva.exe"
        & "$out\curva.exe" 100

        Titulo 'Firmware: modo controle igual ao codigo oficial (limites em 100%)'
        $oficial = Join-Path $env:TEMP 'start-automacao-eletrica'
        if (-not (Test-Path $oficial)) {
            git clone --quiet --depth 1 https://github.com/nrc-cupim/start-automacao-eletrica $oficial 2>$null
        }
        $of = Join-Path $oficial 'Codigos\codigo_robo_controle_p3'
        if (Test-Path $of) {
            g++ -std=c++17 -w -DOFICIAL -I $mock -I $of "$mock\compara_p3.cpp" -o "$out\oficial.exe"
            g++ -std=c++17 -w @sem -I $mock -I $fw "$mock\compara_p3.cpp" -o "$out\novo.exe"
            $a = @(& "$out\oficial.exe"); $b = @(& "$out\novo.exe"); $dif = 0
            for ($i = 0; $i -lt $a.Count; $i++) {
                if ($a[$i].Substring(35) -ne $b[$i].Substring(35)) {
                    $dif++; Write-Host "  diferente: $($a[$i].Substring(0,34).Trim())  oficial[$($a[$i].Substring(35))]  novo[$($b[$i].Substring(35))]"
                }
            }
            Write-Host "$($a.Count) situacoes, $dif diferentes (esperado: 2, arredondamento de 1 ponto de PWM em re a 50%)"
            if ($dif -gt 2) { $falhas++ }
        } else {
            Write-Host "sem internet pra baixar o codigo oficial: comparacao pulada." -ForegroundColor Yellow
        }
    }
}

# ------------------------------------------------------------------ python
if (-not $SemPython) {
    $py = Join-Path $raiz 'venv\Scripts\python.exe'
    if (-not (Test-Path $py)) {
        Write-Host "venv nao encontrado: rode scripts\preparar_ambiente.ps1 primeiro." -ForegroundColor Yellow
        $falhas++
    } else {
        $env:PYTHONDONTWRITEBYTECODE = '1'
        Titulo 'Python: controle_mao.py de ponta a ponta (camera falsa -> pacotes UDP)'
        & $py (Join-Path $raiz 'tests\python\teste_udp.py')
        if ($LASTEXITCODE -ne 0) { $falhas++ }
    }
}

Write-Host ""
if ($falhas) { Write-Host "RESULTADO: $falhas grupo(s) com falha" -ForegroundColor Red; exit 1 }
Write-Host "RESULTADO: tudo ok" -ForegroundColor Green
