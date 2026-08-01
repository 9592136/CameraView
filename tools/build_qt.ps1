param(
    [string]$QtRoot = "",
    [string]$BuildDirectory = "build-qt-msvcrt",
    [switch]$WithOpenCV,
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot

if (-not $QtRoot) {
    $qtCandidates = @()
    $qtCandidates += @(Get-ChildItem "C:\Qt6" -Directory -ErrorAction SilentlyContinue |
        ForEach-Object { Join-Path $_.FullName "mingw_64" })
    $qtCandidates += @(Get-ChildItem "C:\Qt" -Directory -ErrorAction SilentlyContinue |
        ForEach-Object {
            Get-ChildItem $_.FullName -Directory -Filter "mingw_64" -ErrorAction SilentlyContinue |
                Select-Object -ExpandProperty FullName
        })
    $qtCandidates = @($qtCandidates | Where-Object { Test-Path (Join-Path $_ "bin\qmake.exe") })
    $QtRoot = $qtCandidates | Sort-Object -Descending | Select-Object -First 1
}

if (-not $QtRoot -or -not (Test-Path (Join-Path $QtRoot "bin\qmake.exe"))) {
    throw "Qt 6 MinGW 64-bit was not found. Pass the kit directory with -QtRoot."
}

$userTools = Join-Path $env:USERPROFILE "tools"
$cmakeCandidates = @()
$cmakeCandidates += @(Get-Command cmake.exe -ErrorAction SilentlyContinue |
    Select-Object -ExpandProperty Source)
$cmakeCandidates += @(Get-ChildItem $userTools -Filter cmake.exe -Recurse -ErrorAction SilentlyContinue |
    Select-Object -ExpandProperty FullName)
$cmakeCandidates = @($cmakeCandidates | Where-Object { $_ } | Select-Object -Unique)
$cmake = $cmakeCandidates | Select-Object -First 1
if (-not $cmake) {
    throw "CMake was not found."
}

# Official Qt MinGW packages use MSVCRT. Prefer the same ABI to avoid
# incompatible C runtime symbols such as __imp___argc at link time.
$compilerCandidates = @()
$compilerCandidates += Join-Path $userTools "mingw64_msvcrt\mingw64\bin\g++.exe"
$compilerCandidates += Join-Path (Split-Path -Parent (Split-Path -Parent $QtRoot)) "Tools\mingw1310_64\bin\g++.exe"
$compilerCandidates += @(Get-Command g++.exe -ErrorAction SilentlyContinue |
    Select-Object -ExpandProperty Source)
$compilerCandidates = @($compilerCandidates | Where-Object { $_ -and (Test-Path $_) })
$compiler = $compilerCandidates | Select-Object -First 1
if (-not $compiler) {
    throw "A MinGW C++ compiler compatible with Qt was not found."
}

$compilerBin = Split-Path -Parent $compiler
$ninjaCandidates = @()
$ninjaCandidates += Join-Path $compilerBin "ninja.exe"
$ninjaCandidates += @(Get-Command ninja.exe -ErrorAction SilentlyContinue |
    Select-Object -ExpandProperty Source)
$ninjaCandidates = @($ninjaCandidates | Where-Object { $_ -and (Test-Path $_) })
$ninja = $ninjaCandidates | Select-Object -First 1
if (-not $ninja) {
    throw "Ninja was not found."
}

$buildPath = Join-Path $projectRoot $BuildDirectory
$opencvFlag = if ($WithOpenCV) { "ON" } else { "OFF" }

& $cmake -S $projectRoot -B $buildPath -G Ninja `
    "-DCMAKE_BUILD_TYPE=Release" `
    "-DCMAKE_PREFIX_PATH=$QtRoot" `
    "-DCMAKE_CXX_COMPILER=$compiler" `
    "-DCMAKE_MAKE_PROGRAM=$ninja" `
    "-DCAMERAVIEW_WITH_OPENCV=$opencvFlag"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $cmake --build $buildPath --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (-not $SkipTests) {
    $ctest = Join-Path (Split-Path -Parent $cmake) "ctest.exe"
    & $ctest --test-dir $buildPath --output-on-failure
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host "Build completed: $(Join-Path $buildPath 'CameraView.exe')"
