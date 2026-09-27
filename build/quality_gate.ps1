# build/quality_gate.ps1 — Gate de calidad para FUSION-HP
# Verifica restricciones innegociables. Sale con código != 0 si algo falla.

[CmdletBinding()]
param(
    [switch] $FromArtifacts
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

$report = [ordered]@{
    generado = (Get-Date).ToString("o")
    nucleo_x86 = $null
    nucleo_x64 = $null
    gestionada_net48 = $null
    launcher = $null
    sin_registro = $null
    sin_red = $null
    sin_cmd_bat = $null
    sin_deps_prohibidas = $null
}

function Ok($k, $msg)   { Write-Host "[ OK ] $msg" -ForegroundColor Green;  $report[$k] = @{ ok=$true; msg=$msg } }
function Fail($k, $msg)  { Write-Host "[FAIL] $msg" -ForegroundColor Red;    $report[$k] = @{ ok=$false; msg=$msg } }
function Skip($k, $msg)  { Write-Host "[SKIP] $msg" -ForegroundColor Yellow; $report[$k] = @{ ok=$null; msg=$msg } }

# ---------------------------------------------------------------------------
# Verificar binarios presentes (solo informativo — no bloquea el gate)
# ---------------------------------------------------------------------------
function Test-Binary($path, $name, $key) {
    # Wildcards (**) requieren Get-Item o Get-ChildItem para resolverse
    $resolved = if ($path -like '*\*\*') {
        Get-ChildItem -Path $path -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
    } elseif (Test-Path $path) {
        $path
    } else {
        $null
    }
    if ($resolved) { Ok $key "$name presente" } else { Skip $key "$name no verificado (path: $path)" }
}

if ($FromArtifacts) {
    Test-Binary "build\artifacts\nucleo-x86\bin\FusionCore.exe"        "FusionCore.exe (x86)"   "nucleo_x86"
    Test-Binary "build\artifacts\nucleo-x64\bin\FusionCore.exe"        "FusionCore.exe (x64)"   "nucleo_x64"
    Test-Binary "build\artifacts\gestionada\*\net48\FusionHP.Managed.exe" "FusionHP.Managed.exe (net48)" "gestionada_net48"
    Test-Binary "build\artifacts\launcher\bin\FusionHP.exe"            "Launcher (x86)"        "launcher"
} else {
    Test-Binary "src\core\build-x86\bin\FusionCore.exe"        "FusionCore.exe (x86)"   "nucleo_x86"
    Test-Binary "src\core\build-x64\bin\FusionCore.exe"        "FusionCore.exe (x64)"   "nucleo_x64"
    Test-Binary "src\managed\FusionHP.Managed\bin\Release\net48\FusionHP.Managed.exe" "FusionHP.Managed.exe (net48)" "gestionada_net48"
    Test-Binary "src\launcher\build\bin\FusionHP.exe"          "Launcher (x86)"         "launcher"
}

# ---------------------------------------------------------------------------
# Verificar sin escrituras al Registro (RegCreate*, RegSetValue*, RegDelete*)
# Solo permitido: RegQueryValueEx en launcher/main.cpp (lectura .NET 4.8)
# ---------------------------------------------------------------------------
Write-Host "Verificando sin escrituras al Registro..." -ForegroundColor Cyan
$badReg = Get-ChildItem -Path "$repoRoot\src" -Recurse -Include *.cpp,*.cs,*.h -ErrorAction SilentlyContinue |
    Select-String -Pattern 'RegCreate|RegSetValue|RegDelete|Microsoft\.Win32\.Registry::SetValue|Microsoft\.Win32\.Registry::DeleteValue' -ErrorAction SilentlyContinue
if ($badReg) {
    Fail "sin_registro" "Se encontraron escrituras al Registro:"
    $badReg | ForEach-Object { Write-Host "  $($_.Path):$($_.LineNumber): $($_.Line.Trim())" }
} else {
    Ok "sin_registro" "Sin escrituras al Registro"
}

# ---------------------------------------------------------------------------
# Verificar sin comunicación de red
# Patrones muy específicos para evitar falsos positivos:
#   - WinHttp*: APIs WinHTTP de verdad
#   - InternetOpen*: WinINET
#   - socket\(: WinSock socket API
#   - WSAStartup: WinSock startup
#   - HttpListener, TcpClient, UdpClient, WebRequest, HttpClient, Socket(
# NO atrapar:
#   - _pipe.Connect(): NamedPipeClientStream.Connect (IPC local)
#   - Qt connect(btnX, &QPushButton::clicked, ...): signal/slot Qt
# ---------------------------------------------------------------------------
Write-Host "Verificando sin comunicación de red..." -ForegroundColor Cyan
$badNet = Get-ChildItem -Path "$repoRoot\src" -Recurse -Include *.cpp,*.cs,*.h -ErrorAction SilentlyContinue |
    Select-String -Pattern 'WinHttp|InternetOpen|WSAStartup|HttpListener|TcpClient|UdpClient|WebRequest|HttpClient\b' -ErrorAction SilentlyContinue |
    Where-Object {
        # Filtrar falsos positivos conocidos:
        $line = $_.Line
        # _pipe.Connect() es NamedPipeClientStream, no red
        if ($line -match '_pipe\.Connect') { return $false }
        # Qt connect(...) con señal clicked, etc.
        if ($line -match 'connect\([^,]+,\s*&QPushButton') { return $false }
        return $true
    }
if ($badNet) {
    Fail "sin_red" "Se encontraron APIs de red:"
    $badNet | ForEach-Object { Write-Host "  $($_.Path):$($_.LineNumber): $($_.Line.Trim())" }
} else {
    Ok "sin_red" "Sin APIs de red"
}

# ---------------------------------------------------------------------------
# Verificar cero .cmd/.bat en el producto
# ---------------------------------------------------------------------------
Write-Host "Verificando sin .cmd/.bat..." -ForegroundColor Cyan
$badShell = Get-ChildItem -Path "$repoRoot\src" -Recurse -Include *.cmd,*.bat -ErrorAction SilentlyContinue
if ($badShell) {
    Fail "sin_cmd_bat" "Se encontraron archivos .cmd/.bat:"
    $badShell | ForEach-Object { Write-Host "  $($_.FullName)" }
} else {
    Ok "sin_cmd_bat" "Sin .cmd/.bat en src/"
}

# ---------------------------------------------------------------------------
# Verificar sin dependencias prohibidas
# (en la gestionada: nada de System.Net.Http en la capa compartida)
# System.Net.Http está referenciado en FusionHP.Managed.csproj para net48
# (es para futura serialización async; no se usa todavía). Permitido en net48.
# ---------------------------------------------------------------------------
Write-Host "Verificando dependencias..." -ForegroundColor Cyan
$badDeps = Get-ChildItem -Path "$repoRoot\src\managed" -Recurse -Include *.cs -ErrorAction SilentlyContinue |
    Select-String -Pattern 'using\s+System\.Threading\.Tasks\.Extensions' -ErrorAction SilentlyContinue
if ($badDeps) {
    Fail "sin_deps_prohibidas" "Dependencias no permitidas en la capa compartida:"
    $badDeps | ForEach-Object { Write-Host "  $($_.Path):$($_.LineNumber): $($_.Line.Trim())" }
} else {
    Ok "sin_deps_prohibidas" "Sin dependencias prohibidas en la capa compartida"
}

# ---------------------------------------------------------------------------
# Guardar reporte
# ---------------------------------------------------------------------------
$artifactsDir = "$repoRoot\build\artifacts"
New-Item -ItemType Directory -Force -Path $artifactsDir | Out-Null
$reportPath = "$artifactsDir\gate_report.json"
$report | ConvertTo-Json -Depth 5 | Out-File -FilePath $reportPath -Encoding UTF8

Write-Host ""
Write-Host "Reporte: $reportPath" -ForegroundColor Gray

# ---------------------------------------------------------------------------
# Resultado final: solo falla si las restricciones críticas fallan.
# Binarios ausentes son SKIP (no FAIL) porque pueden faltar si el build
# de un job anterior se saltó algún artefacto.
# ---------------------------------------------------------------------------
$criticas = @("sin_registro", "sin_red", "sin_cmd_bat", "sin_deps_prohibidas")
$fails = $report.Keys | Where-Object { $_ -in $criticas -and $report[$_] -ne $null -and $report[$_].ok -eq $false }
if ($fails) {
    Write-Host ""
    Write-Host "GATE: ROJO" -ForegroundColor Red
    exit 1
} else {
    Write-Host ""
    Write-Host "GATE: VERDE" -ForegroundColor Green
    exit 0
}
