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
    gestionada_net35 = $null
    gestionada_net48 = $null
    launcher = $null
    qt_x86 = $null
    qt_x64 = $null
    tests_nat = $null
    tests_gestionados = $null
    sin_registro = $null
    sin_red = $null
    sin_cmd_bat = $null
    sin_deps_prohibidas = $null
}

function Ok($k, $msg)   { Write-Host "[ OK ] $msg" -ForegroundColor Green;  $report[$k] = @{ ok=$true; msg=$msg } }
function Fail($k, $msg)  { Write-Host "[FAIL] $msg" -ForegroundColor Red;    $report[$k] = @{ ok=$false; msg=$msg } }
function Skip($k, $msg)  { Write-Host "[SKIP] $msg" -ForegroundColor Yellow; $report[$k] = @{ ok=$null; msg=$msg } }

# ---------------------------------------------------------------------------
# Verificar binarios presentes
# ---------------------------------------------------------------------------
function Test-Binary($path, $name, $key) {
    if (Test-Path $path) { Ok $key "$name presente" } else { Fail $key "$name AUSENTE ($path)" }
}

if ($FromArtifacts) {
    Test-Binary "build\artifacts\nucleo-x86\bin\FusionCore.exe"        "FusionCore.exe (x86)"   "nucleo_x86"
    Test-Binary "build\artifacts\nucleo-x64\bin\FusionCore.exe"        "FusionCore.exe (x64)"   "nucleo_x64"
    Test-Binary "build\artifacts\gestionada\**\net35\FusionHP.Managed.exe" "FusionHP.Managed.exe (net35)" "gestionada_net35"
    Test-Binary "build\artifacts\gestionada\**\net48\FusionHP.Managed.exe" "FusionHP.Managed.exe (net48)" "gestionada_net48"
    Test-Binary "build\artifacts\launcher\bin\FusionHP.exe"             "Launcher (x86)"         "launcher"
} else {
    Test-Binary "src\core\build-x86\bin\FusionCore.exe"        "FusionCore.exe (x86)"   "nucleo_x86"
    Test-Binary "src\core\build-x64\bin\FusionCore.exe"        "FusionCore.exe (x64)"   "nucleo_x64"
    Test-Binary "src\managed\FusionHP.Managed\bin\Release\net35\FusionHP.Managed.exe" "FusionHP.Managed.exe (net35)" "gestionada_net35"
    Test-Binary "src\managed\FusionHP.Managed\bin\Release\net48\FusionHP.Managed.exe" "FusionHP.Managed.exe (net48)" "gestionada_net48"
    Test-Binary "src\launcher\build\bin\FusionHP.exe"           "Launcher (x86)"         "launcher"
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
# (WinHttp*, WinINET*, socket, connect, WSAStartup, HttpListener, TcpClient, UdpClient, WebRequest)
# ---------------------------------------------------------------------------
Write-Host "Verificando sin comunicación de red..." -ForegroundColor Cyan
$badNet = Get-ChildItem -Path "$repoRoot\src" -Recurse -Include *.cpp,*.cs,*.h -ErrorAction SilentlyContinue |
    Select-String -Pattern 'WinHttp|InternetOpen|socket\(|connect\(|WSAStartup|HttpListener|TcpClient|UdpClient|WebRequest|HttpClient|Socket\(' -ErrorAction SilentlyContinue
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
# (en la gestionada net35: nada de System.Net.Http, System.Threading.Tasks)
# ---------------------------------------------------------------------------
Write-Host "Verificando dependencias..." -ForegroundColor Cyan
$badDeps = Get-ChildItem -Path "$repoRoot\src\managed" -Recurse -Include *.cs -ErrorAction SilentlyContinue |
    Select-String -Pattern 'using\s+System\.Net\.Http|using\s+System\.Threading\.Tasks\.Extensions' -ErrorAction SilentlyContinue
if ($badDeps) {
    Fail "sin_deps_prohibidas" "Dependencias no permitidas en net35:"
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
# Resultado final
# ---------------------------------------------------------------------------
$fails = $report.Values | Where-Object { $_ -ne $null -and $_.ok -eq $false }
if ($fails) {
    Write-Host ""
    Write-Host "GATE: ROJO" -ForegroundColor Red
    exit 1
} else {
    Write-Host ""
    Write-Host "GATE: VERDE" -ForegroundColor Green
    exit 0
}
