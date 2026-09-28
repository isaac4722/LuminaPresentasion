# ============================================================================
# build/empaquetar.ps1 — Monta el árbol de paquete de FUSION-HP a partir
# de los artefactos descargados de GitHub Actions (o de builds locales).
#
# Uso (CI):
#   .\build\empaquetar.ps1 -Arch x64 -Artefactos build\artifacts `
#       -Destino build\paquete\x64
# El resultado es un árbol portable completo:
#   FusionHP.exe (launcher), FusionCore.exe (núcleo), gestionado\,
#   qt\ (shell Qt + DLL de Qt ya desplegadas), data\, LEEME.txt, licencias.
# El mismo árbol alimenta el instalador Inno Setup (installer/FusionHP.iss)
# y el .zip portable: una sola fuente de verdad para ambos.
#
# Reglas del producto: sin red, sin Registro, sin .cmd/.bat.
# ============================================================================

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('x86', 'x64')]
    [string]$Arch,

    [string]$Artefactos = "build/artifacts",

    [Parameter(Mandatory = $true)]
    [string]$Destino,

    # Raíz del repo (para data\, licencias y LEEME). Por defecto, la
    # carpeta dos niveles arriba de este script.
    [string]$Raiz = ""
)

$ErrorActionPreference = 'Stop'

if ($Raiz -eq "") {
    $Raiz = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
}

function Buscar-Exe {
    param([string]$Nombre, [string]$FiltroRuta)
    $hallados = Get-ChildItem -Path $Artefactos -Recurse -Filter $Nombre -File |
        Where-Object { $_.FullName -match $FiltroRuta } |
        Sort-Object FullName
    if (-not $hallados) {
        Write-Host "##[error]empaquetar: no se encontró '$Nombre' (filtro '$FiltroRuta') bajo '$Artefactos'"
        exit 1
    }
    return $hallados[0].FullName
}

Write-Host "== Empaquetando FUSION-HP ($Arch) =="
Write-Host "   artefactos: $Artefactos"
Write-Host "   destino:    $Destino"

# Limpiar destino previo.
if (Test-Path $Destino) { Remove-Item -Recurse -Force $Destino }
New-Item -ItemType Directory -Force -Path $Destino | Out-Null

# ---------------------------------------------------------------------------
# 1) Launcher (x86 en ambas arquitecturas: es el arranque común)
# ---------------------------------------------------------------------------
$launcher = Buscar-Exe 'FusionHP.exe' 'launcher'
Copy-Item $launcher (Join-Path $Destino 'FusionHP.exe')
Write-Host "   launcher: $launcher"

# ---------------------------------------------------------------------------
# 2) Núcleo de la arquitectura pedida
# ---------------------------------------------------------------------------
$nucleo = Buscar-Exe 'FusionCore.exe' ("nucleo-" + $Arch)
Copy-Item $nucleo (Join-Path $Destino 'FusionCore.exe')
Write-Host "   nucleo:   $nucleo"

# ---------------------------------------------------------------------------
# 3) Capa gestionada (net48) — exe + DLL/config de su carpeta, sin tests
# ---------------------------------------------------------------------------
$gest = Get-ChildItem -Path $Artefactos -Recurse -Filter 'FusionHP.Managed.exe' -File |
    Where-Object { $_.FullName -notmatch 'Tests' } |
    Sort-Object FullName | Select-Object -First 1
if (-not $gest) {
    Write-Host "##[error]empaquetar: falta FusionHP.Managed.exe en los artefactos"
    exit 1
}
$dirGest = Split-Path -Parent $gest.FullName
New-Item -ItemType Directory -Force -Path (Join-Path $Destino 'gestionado') | Out-Null
Copy-Item (Join-Path $dirGest '*') (Join-Path $Destino 'gestionado') -Force
# Por si el build trae subcarpetas de runtime (p. ej. es/, cs/ de satélites).
Get-ChildItem $dirGest -Directory -ErrorAction SilentlyContinue | ForEach-Object {
    Copy-Item $_.FullName (Join-Path $Destino ('gestionado\' + $_.Name)) -Recurse -Force
}
Write-Host "   gestionado: $($gest.FullName)"

# ---------------------------------------------------------------------------
# 4) Shell Qt + DLL de Qt (windeployqt ya corrió en el job qt_shell)
# ---------------------------------------------------------------------------
# Buscar-Exe devuelve la ruta (string): no re-aplicar .FullName.
$qt = Buscar-Exe 'FusionQtShell.exe' ("qt-shell-" + $Arch)
$dirQt = Split-Path -Parent $qt
New-Item -ItemType Directory -Force -Path (Join-Path $Destino 'qt') | Out-Null
Copy-Item (Join-Path $dirQt '*') (Join-Path $Destino 'qt') -Recurse -Force
$qtDll = Get-ChildItem (Join-Path $Destino 'qt') -Filter 'Qt5Core.dll' -Recurse -ErrorAction SilentlyContinue
if (-not $qtDll) {
    Write-Host "##[warning]empaquetar: Qt5Core.dll no está junto al shell Qt (¿faltó windeployqt?)"
}
# Limpiar restos de compilación que el artefacto de qt_shell arrastra
# (el artefacto es el build dir completo, útil para depurar; el
# paquete solo lleva lo que se ejecuta).
$destinoQt = Join-Path $Destino 'qt'
foreach ($b in @('CMakeFiles', 'CMakeCache.txt', 'build.ninja',
                 'cmake_install.cmake', 'CMakeDoxyfile.in',
                 'FusionQtShell_autogen', 'CMakeLists.txt.user')) {
    $p = Join-Path $destinoQt $b
    if (Test-Path $p) { Remove-Item -Recurse -Force $p }
}
Get-ChildItem $destinoQt -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like 'vc_redist*.exe' -or
                   $_.Name -like '*.obj' -or
                   $_.Name -like '*.ninja' -or
                   $_.Name -like '.ninja*' -or
                   $_.Name -like '*.d' } |
    Remove-Item -Force -ErrorAction SilentlyContinue
Write-Host "   qt:       $qt"

# ---------------------------------------------------------------------------
# 5) Datos (esquemas, biblias, assets, muestras) — del checkout
# ---------------------------------------------------------------------------
New-Item -ItemType Directory -Force -Path (Join-Path $Destino 'data') | Out-Null
foreach ($d in @('schema', 'bibles', 'assets', 'samples')) {
    $origen = Join-Path $Raiz ("data/" + $d)
    if (Test-Path $origen) {
        Copy-Item $origen (Join-Path $Destino ('data/' + $d)) -Recurse -Force
    }
}

# ---------------------------------------------------------------------------
# 6) Documentación y licencias
# ---------------------------------------------------------------------------
$leeme = Join-Path $Raiz 'installer/LEEME-portable.txt'
if (Test-Path $leeme) {
    Copy-Item $leeme (Join-Path $Destino 'LEEME.txt')
}
foreach ($f in @('LICENSE', 'THIRD_PARTY_LICENSES.txt', 'README.md')) {
    $p = Join-Path $Raiz $f
    if (Test-Path $p) { Copy-Item $p (Join-Path $Destino $f) }
}

# ---------------------------------------------------------------------------
# Verificación final del árbol
# ---------------------------------------------------------------------------
foreach ($rel in @('FusionHP.exe', 'FusionCore.exe',
                   'gestionado/FusionHP.Managed.exe', 'qt/FusionQtShell.exe',
                   'data/schema')) {
    if (-not (Test-Path (Join-Path $Destino $rel))) {
        Write-Host "##[error]empaquetar: falta '$rel' en el paquete"
        exit 1
    }
}

$n = (Get-ChildItem $Destino -Recurse -File | Measure-Object).Count
$mb = [math]::Round(((Get-ChildItem $Destino -Recurse -File |
        Measure-Object Length -Sum).Sum) / 1MB, 1)
Write-Host "== Paquete listo: $Destino ($n archivos, $mb MB) =="
