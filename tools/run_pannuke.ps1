param(
    [ValidateSet("download", "prepare", "train", "test", "import", "all")]
    [string]$Stage = "all",
    [ValidateSet("cpu", "cuda:0")]
    [string]$Device = "cpu",
    [string]$BuildDirectory = "build-qt-scroll-test"
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$pythonExecutable = Join-Path $repositoryRoot ".venv-yolo\Scripts\python.exe"
$backendScript = Join-Path $repositoryRoot "tools\yolo_backend.py"
$prepareScript = Join-Path $repositoryRoot "tools\prepare_pannuke.py"
$sourceDirectory = Join-Path $repositoryRoot "runs\datasets\pannuke\official"
$datasetDirectory = Join-Path $repositoryRoot "runs\datasets\pannuke\yolo"
$modelDirectory = Join-Path $repositoryRoot "runs\pannuke\models"
$validationDirectory = Join-Path $repositoryRoot "runs\pannuke\validation"

$datasetShards = [ordered]@{
    "fold1-00000-of-00002.parquet" = "033166d98dbc1fae4aa8409d1c8ca9d7c160b73137d7032a9806d601f17f7389"
    "fold1-00001-of-00002.parquet" = "d1eca2058b64a2e5e890b385ff1ba349919a9a116a12bbd2133912dc446c3611"
    "fold2-00000-of-00002.parquet" = "f9b38bdc3ca93f6c14b5352d6a6e980eca65aa2b31d604e26e41a354e3274972"
    "fold2-00001-of-00002.parquet" = "6351f7e1bae80c6f705900737d02b5c9f56d2d1bc3b44d1b0567a01d6f614355"
    "fold3-00000-of-00002.parquet" = "7600caf6445f4e20cea55293b395cbc356f91a1eb19afa6cce2bcbb1422e411c"
    "fold3-00001-of-00002.parquet" = "2da2cfc054a341f3310c310892480c414bcce82a1560a68339147c7f38caae73"
}

function Test-Stage([string]$RequestedStage) {
    return $Stage -eq "all" -or $Stage -eq $RequestedStage
}

function Invoke-Python([string[]]$Arguments) {
    & $pythonExecutable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Python command failed with exit code $LASTEXITCODE."
    }
}

if (Test-Stage "download") {
    New-Item -ItemType Directory -Force -Path $sourceDirectory | Out-Null
    foreach ($entry in $datasetShards.GetEnumerator()) {
        $destination = Join-Path $sourceDirectory $entry.Key
        $downloadRequired = -not (Test-Path -LiteralPath $destination)
        if (-not $downloadRequired) {
            $actualHash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant()
            $downloadRequired = $actualHash -ne $entry.Value
        }
        if ($downloadRequired) {
            $uri = "https://huggingface.co/datasets/MedOtter/PanNuke/resolve/main/data/$($entry.Key)?download=true"
            Write-Host "Downloading $($entry.Key)..."
            Invoke-WebRequest -Uri $uri -OutFile $destination
        }
        $verifiedHash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($verifiedHash -ne $entry.Value) {
            throw "SHA-256 mismatch for $($entry.Key)."
        }
    }
}

if ($Stage -ne "download" -and -not (Test-Path -LiteralPath $pythonExecutable)) {
    throw "YOLO environment is missing. Run .\tools\setup_yolo.ps1 -CpuOnly first."
}

if (Test-Stage "prepare") {
    Invoke-Python @($prepareScript, "audit", "--source", $sourceDirectory,
        "--report", (Join-Path $repositoryRoot "runs\datasets\pannuke\audit-report.json"))
    Invoke-Python @($prepareScript, "prepare", "--source", $sourceDirectory,
        "--output", $datasetDirectory, "--train-cap", "967", "--val-cap", "3000",
        "--test-cap", "3000", "--force")
    Invoke-Python @($prepareScript, "validate", "--output", $datasetDirectory,
        "--report", (Join-Path $repositoryRoot "runs\datasets\pannuke\converted-validation.json"))
}

if (Test-Stage "train") {
    Invoke-Python @($backendScript, "train", "--model", (Join-Path $repositoryRoot "yolo11n.pt"),
        "--data", (Join-Path $datasetDirectory "detect\dataset.yaml"), "--task", "detect",
        "--epochs", "5", "--imgsz", "256", "--batch", "32", "--workers", "0",
        "--device", $Device, "--project", $modelDirectory, "--name", "detect-yolo11n-e5",
        "--seed", "42", "--exist-ok")
    Invoke-Python @($backendScript, "train", "--model", (Join-Path $repositoryRoot "yolo11n-seg.pt"),
        "--data", (Join-Path $datasetDirectory "segment\dataset.yaml"), "--task", "segment",
        "--epochs", "5", "--imgsz", "256", "--batch", "16", "--workers", "0",
        "--device", $Device, "--project", $modelDirectory, "--name", "segment-yolo11n-e5",
        "--seed", "42", "--exist-ok")
    Invoke-Python @($backendScript, "train", "--model", (Join-Path $repositoryRoot "yolo11n-cls.pt"),
        "--data", (Join-Path $datasetDirectory "classify"), "--task", "classify",
        "--epochs", "10", "--imgsz", "128", "--batch", "64", "--workers", "0",
        "--device", $Device, "--project", $modelDirectory, "--name", "classify-yolo11n-e10-fixed",
        "--seed", "42", "--exist-ok")
}

if (Test-Stage "test") {
    New-Item -ItemType Directory -Force -Path $validationDirectory | Out-Null
    Invoke-Python @($backendScript, "validate", "--model", (Join-Path $modelDirectory "detect-yolo11n-e5\weights\best.pt"),
        "--data", (Join-Path $datasetDirectory "detect\dataset.yaml"), "--task", "detect", "--split", "test",
        "--imgsz", "256", "--batch", "32", "--workers", "0", "--device", $Device,
        "--project", $validationDirectory, "--name", "detect-test", "--exist-ok",
        "--report", (Join-Path $validationDirectory "detect-test-metrics.json"))
    Invoke-Python @($backendScript, "validate", "--model", (Join-Path $modelDirectory "segment-yolo11n-e5\weights\best.pt"),
        "--data", (Join-Path $datasetDirectory "segment\dataset.yaml"), "--task", "segment", "--split", "test",
        "--imgsz", "256", "--batch", "16", "--workers", "0", "--device", $Device,
        "--project", $validationDirectory, "--name", "segment-test", "--exist-ok",
        "--report", (Join-Path $validationDirectory "segment-test-metrics.json"))
    Invoke-Python @($backendScript, "validate", "--model", (Join-Path $modelDirectory "classify-yolo11n-e10-fixed\weights\best.pt"),
        "--data", (Join-Path $datasetDirectory "classify"), "--task", "classify", "--split", "test",
        "--imgsz", "128", "--batch", "64", "--workers", "0", "--device", $Device,
        "--project", $validationDirectory, "--name", "classify-test", "--exist-ok",
        "--report", (Join-Path $validationDirectory "classify-test-metrics.json"))
}

if (Test-Stage "import") {
    $manifestPath = Join-Path $repositoryRoot "runs\pannuke\model-import.json"
    $manifest = @{
        models = @(
            @{ name = "PanNuke YOLO11n Detection e5"; path = (Join-Path $modelDirectory "detect-yolo11n-e5\weights\best.pt"); task = "detect"; classes = @("neoplastic", "inflammatory", "connective", "dead", "epithelial"); metrics = @{ split = "test"; precision = 0.6614782576; recall = 0.4534019408; mAP50 = 0.4565109337; mAP50_95 = 0.2743894885 }; active = $false }
            @{ name = "PanNuke YOLO11n Classification e10"; path = (Join-Path $modelDirectory "classify-yolo11n-e10-fixed\weights\best.pt"); task = "classify"; classes = @("connective", "dead", "epithelial", "inflammatory", "neoplastic"); metrics = @{ split = "test"; top1_accuracy = 0.6617140174; top5_accuracy = 1.0 }; active = $false }
            @{ name = "PanNuke YOLO11n Segmentation e5"; path = (Join-Path $modelDirectory "segment-yolo11n-e5\weights\best.pt"); task = "segment"; classes = @("neoplastic", "inflammatory", "connective", "dead", "epithelial"); metrics = @{ split = "test"; box_mAP50 = 0.4427550752; mask_mAP50 = 0.4347635617; mask_mAP50_95 = 0.2061278208 }; active = $true }
        )
    }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $manifestPath) | Out-Null
    $manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
    $application = Join-Path $repositoryRoot "$BuildDirectory\CameraView.exe"
    if (-not (Test-Path -LiteralPath $application)) {
        throw "CameraView executable is missing at $application. Build it first."
    }
    & $application --import-yolo-manifest $manifestPath
    if ($LASTEXITCODE -ne 0) {
        throw "CameraView model import failed with exit code $LASTEXITCODE."
    }
}

Write-Host "PanNuke stage '$Stage' completed."
