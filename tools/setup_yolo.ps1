param(
    [string]$PythonExecutable = "",
    [string]$VenvPath = "",
    [switch]$CpuOnly
)

$ErrorActionPreference = "Stop"
$RepositoryRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($VenvPath)) {
    $VenvPath = Join-Path $RepositoryRoot ".venv-yolo"
}

function Test-PythonVersion([string]$Executable) {
    if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) { return $false }
    & $Executable -c "import sys; raise SystemExit(0 if (3, 10) <= sys.version_info[:2] <= (3, 12) else 1)"
    return $LASTEXITCODE -eq 0
}

if ([string]::IsNullOrWhiteSpace($PythonExecutable)) {
    $Candidates = @(
        (Join-Path $env:LocalAppData "python312\python.exe"),
        (Join-Path $env:LocalAppData "Programs\Python\Python312\python.exe"),
        (Join-Path $env:LocalAppData "Programs\Python\Python311\python.exe"),
        (Get-Command python.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -First 1)
    ) | Where-Object { $_ }
    $PythonExecutable = $Candidates | Where-Object { Test-PythonVersion $_ } | Select-Object -First 1
}

if ([string]::IsNullOrWhiteSpace($PythonExecutable)) {
    $Winget = Get-Command winget.exe -ErrorAction SilentlyContinue
    if (-not $Winget) {
        throw "Python 3.10-3.12 was not found. Install Python 3.12 for the current user or pass -PythonExecutable."
    }
    Write-Host "Installing Python 3.12 for the current user (no administrator access required)..."
    & $Winget.Source install --id Python.Python.3.12 --exact --scope user --accept-package-agreements --accept-source-agreements --silent
    $PythonExecutable = @(
        (Join-Path $env:LocalAppData "python312\python.exe"),
        (Join-Path $env:LocalAppData "Programs\Python\Python312\python.exe")
    ) | Where-Object { Test-PythonVersion $_ } | Select-Object -First 1
}

if (-not (Test-PythonVersion $PythonExecutable)) {
    throw "This CameraView YOLO environment requires Python 3.10-3.12: $PythonExecutable"
}

Write-Host "Creating isolated environment: $VenvPath"
& $PythonExecutable -m venv $VenvPath
$VenvPython = Join-Path $VenvPath "Scripts\python.exe"
& $VenvPython -m pip install --upgrade pip wheel
if ($CpuOnly) {
    & $VenvPython -m pip install "torch==2.5.1+cpu" "torchvision==0.20.1+cpu" --index-url https://download.pytorch.org/whl/cpu
} else {
    & $VenvPython -m pip install "torch==2.5.1" "torchvision==0.20.1"
}
& $VenvPython -m pip install "ultralytics>=8.3,<9" onnx onnxruntime
& $VenvPython -c "import torch, ultralytics; print(f'PyTorch {torch.__version__}; Ultralytics {ultralytics.__version__}')"
& $VenvPython (Join-Path $PSScriptRoot "yolo_backend.py") probe

Write-Host "YOLO environment is ready. CameraView will discover: $VenvPython"
