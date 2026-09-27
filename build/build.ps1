# build/build.ps1 — Script de build dual para Windows
# Compila x86+x64 (núcleo C++), net35+net48 (gestionada), x86 (launcher), x86+x64 (Qt shell)
# y ejecuta el gate de calidad al final.

[CmdletBinding()]
param(
    [switch] $Dual,
    [switch] $x86,
    [switch] $x64,
    [switch] $Nucleo,
    [switch] $Gestionada,
    [switch] $Launcher,
    [switch] $Qt,
    [switch] $Tests,
    [switch] $VerifyEnv,
    [switch] $PrepareCommit,
    [switch] $SkipGate,
    [string] $Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

function Write-Step($msg) { Write-Host ">>> $msg" -ForegroundColor Cyan }
function Write-Ok($msg)   { Write-Host "[ OK ] $msg" -ForegroundColor Green }
function Write-Warn2($msg){ Write-Host "[WARN] $msg" -ForegroundColor Yellow }
function Write-Err($msg)  { Write-Host "[FAIL] $msg" -ForegroundColor Red }

# ---------------------------------------------------------------------------
# Verificación de entorno
# ---------------------------------------------------------------------------
function Test-Env {
    Write-Step "Verificando entorno"
    $ok = $true

    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $vsPath = & $vsWhere -latest -property installationPath
        Write-Ok "Visual Studio: $vsPath"
    } else {
        Write-Err "Visual Studio no encontrado"; $ok = $false
    }

    if (Test-Path "C:\Program Files (x86)\Reference Assemblies\Microsoft\Framework\.NETFramework\v3.5\Profile\Client") {
        Write-Ok ".NET 3.5 SDK"
    } else {
        Write-Err ".NET 3.5 SDK no encontrado (Activar feature Windows)"; $ok = $false
    }

    if (Test-Path "C:\Program Files (x86)\Reference Assemblies\Microsoft\Framework\.NETFramework\v4.8") {
        Write-Ok ".NET 4.8 SDK"
    } else {
        Write-Err ".NET 4.8 SDK no encontrado"; $ok = $false
    }

    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmake) { Write-Ok "CMake: $($cmake.Source)" } else { Write-Err "CMake no encontrado"; $ok = $false }

    $iscc = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
    if (Test-Path $iscc) { Write-Ok "Inno Setup 6" } else { Write-Warn2 "Inno Setup 6 no encontrado (solo necesario para empaquetar)" }

    if (Test-Path "C:\Qt\5.15.2\msvc2019")      { Write-Ok "Qt 5.15.2 x86" } else { Write-Warn2 "Qt 5.15.2 x86 no encontrado" }
    if (Test-Path "C:\Qt\5.15.2\msvc2019_64")    { Write-Ok "Qt 5.15.2 x64" } else { Write-Warn2 "Qt 5.15.2 x64 no encontrado" }

    return $ok
}

if ($VerifyEnv) {
    if (Test-Env) { Write-Ok "Entorno OK"; exit 0 } else { exit 1 }
}

# ---------------------------------------------------------------------------
# Build núcleo C++17
# ---------------------------------------------------------------------------
function Build-Nucleo([string]$arch) {
    Write-Step "Compilando núcleo C++17 ($arch)"
    Push-Location "$repoRoot\src\core"
    try {
        $buildDir = "build-$arch"
        cmake -S . -B $buildDir -G "Ninja" `
            -DCMAKE_BUILD_TYPE=$Configuration `
            -DCMAKE_SYSTEM_PROCESSOR=$arch `
            -DFUSION_STATIC_CRT=ON `
            -DFUSION_BUILD_TESTS=$Tests.ToString()
        if ($LASTEXITCODE -ne 0) { throw "CMake config falló ($arch)" }

        cmake --build $buildDir --config $Configuration --parallel
        if ($LASTEXITCODE -ne 0) { throw "Build falló ($arch)" }

        Write-Ok "Núcleo $arch OK"
    } finally { Pop-Location }
}

# ---------------------------------------------------------------------------
# Build capa gestionada C#
# ---------------------------------------------------------------------------
function Build-Gestionada([string]$target) {
    Write-Step "Compilando capa gestionada ($target)"
    Push-Location "$repoRoot\src\managed"
    try {
        # Restaurar NuGet (si hay packages.config)
        if (Test-Path "packages.config") {
            nuget restore FusionHP.Managed.sln
        }

        msbuild FusionHP.Managed.sln `
            /p:Configuration=$Configuration `
            /p:Platform="Any CPU" `
            /p:TargetFramework=$target `
            /m
        if ($LASTEXITCODE -ne 0) { throw "Build gestionada falló ($target)" }

        Write-Ok "Gestionada $target OK"
    } finally { Pop-Location }
}

# ---------------------------------------------------------------------------
# Build launcher (x86, sin consola)
# ---------------------------------------------------------------------------
function Build-Launcher {
    Write-Step "Compilando launcher nativo (x86)"
    Push-Location "$repoRoot\src\launcher"
    try {
        cmake -S . -B build -G "Ninja" `
            -DCMAKE_BUILD_TYPE=$Configuration `
            -DCMAKE_SYSTEM_PROCESSOR=x86
        cmake --build build --parallel
        if ($LASTEXITCODE -ne 0) { throw "Build launcher falló" }
        Write-Ok "Launcher OK"
    } finally { Pop-Location }
}

# ---------------------------------------------------------------------------
# Build Qt shell
# ---------------------------------------------------------------------------
function Build-Qt([string]$arch) {
    Write-Step "Compilando Qt shell ($arch)"
    $qtPath = if ($arch -eq "x86") { "C:\Qt\5.15.2\msvc2019" } else { "C:\Qt\5.15.2\msvc2019_64" }
    if (-not (Test-Path $qtPath)) { Write-Warn2 "Qt 5.15.2 $arch no encontrado — saltando"; return }

    Push-Location "$repoRoot\src\qt-shell"
    try {
        $env:CMAKE_PREFIX_PATH = $qtPath
        $buildDir = "build-$arch"
        cmake -S . -B $buildDir -G "Ninja" -DCMAKE_BUILD_TYPE=$Configuration
        cmake --build $buildDir --parallel
        if ($LASTEXITCODE -ne 0) { throw "Build Qt falló ($arch)" }
        Write-Ok "Qt shell $arch OK"
    } finally { Pop-Location }
}

# ---------------------------------------------------------------------------
# Ejecutar tests
# ---------------------------------------------------------------------------
function Run-Tests {
    Write-Step "Ejecutando tests"

    # Nativos
    foreach ($arch in @("x86","x64")) {
        $exe = "$repoRoot\src\core\build-$arch\bin\fusion_tests.exe"
        if (Test-Path $exe) {
            & $exe --reporters=console --no-breaks
            if ($LASTEXITCODE -ne 0) { throw "Tests nativos ($arch) fallaron" }
            Write-Ok "Tests nativos $arch OK"
        }
    }

    # Gestionados
    $testExe = "$repoRoot\src\managed\FusionHP.Managed.Tests\bin\$Configuration\net48\FusionHP.Managed.Tests.exe"
    if (Test-Path $testExe) {
        & $testExe
        if ($LASTEXITCODE -ne 0) { throw "Tests gestionados fallaron" }
        Write-Ok "Tests gestionados OK"
    }
}

# ---------------------------------------------------------------------------
# Ejecutar gate de calidad
# ---------------------------------------------------------------------------
function Run-Gate {
    Write-Step "Gate de calidad"
    & "$repoRoot\build\quality_gate.ps1"
    if ($LASTEXITCODE -ne 0) { throw "Gate de calidad falló" }
    Write-Ok "Gate de calidad: VERDE"
}

# ---------------------------------------------------------------------------
# Orquestación
# ---------------------------------------------------------------------------
if (-not (Test-Env)) { Write-Err "Entorno no OK"; exit 1 }

$doNucleo    = $Nucleo   -or $Dual -or (-not ($Gestionada -or $Launcher -or $Qt))
$doGestionada= $Gestionada -or $Dual -or (-not ($Nucleo -or $Launcher -or $Qt))
$doLauncher   = $Launcher   -or $Dual -or (-not ($Nucleo -or $Gestionada -or $Qt))
$doQt         = $Qt -or $Dual

if ($doNucleo) {
    if ($Dual -or $x86 -or -not $x64) { Build-Nucleo "x86" }
    if ($Dual -or $x64 -or -not $x86) { Build-Nucleo "x64" }
}
if ($doGestionada) {
    Build-Gestionada "net35"
    Build-Gestionada "net48"
}
if ($doLauncher) { Build-Launcher }
if ($doQt) {
    if ($Dual -or $x86) { Build-Qt "x86" }
    if ($Dual -or $x64) { Build-Qt "x64" }
}

if ($Tests) { Run-Tests }

if (-not $SkipGate) { Run-Gate }

if ($PrepareCommit) {
    Write-Step "Preparando commit"
    git add -A
    Write-Ok "Cambios en staged. Revisa con 'git status' y 'git diff --cached'."
}

Write-Ok "Build completo."
