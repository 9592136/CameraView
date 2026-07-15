param(
    [ValidateSet("check", "png", "svg")]
    [string]$Mode = "check",

    [string]$PlantUmlJar = "",

    [string]$OutputDir = "docs\uml\rendered"
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptRoot
$umlDir = Join-Path $repoRoot "docs\uml"
$renderDir = Join-Path $repoRoot $OutputDir

if (-not (Test-Path $umlDir)) {
    Write-Error "UML directory not found: $umlDir"
}

$files = Get-ChildItem -Path $umlDir -Filter "*.puml" -File | Sort-Object Name
if ($files.Count -eq 0) {
    Write-Error "No PlantUML files found in $umlDir"
}

$checkRows = foreach ($file in $files) {
    $text = Get-Content -Raw -Encoding UTF8 $file.FullName
    [PSCustomObject]@{
        File = $file.Name
        Start = ([regex]::Matches($text, "@startuml")).Count
        End = ([regex]::Matches($text, "@enduml")).Count
    }
}

$checkRows | Format-Table -AutoSize
Write-Host "TOTAL=$($files.Count)"

$invalid = $checkRows | Where-Object { $_.Start -ne 1 -or $_.End -ne 1 }
if ($invalid) {
    Write-Error "PlantUML source check failed. Each file must contain exactly one @startuml and one @enduml."
}

if ($Mode -eq "check") {
    Write-Host "Source check completed."
    exit 0
}

New-Item -ItemType Directory -Force $renderDir | Out-Null

$format = $Mode
if ($PlantUmlJar) {
    if (-not (Test-Path $PlantUmlJar)) {
        Write-Error "PlantUML jar not found: $PlantUmlJar"
    }

    $java = Get-Command java -ErrorAction SilentlyContinue
    if (-not $java) {
        Write-Error "Java was not found. Install Java or use PlantUML command line."
    }

    foreach ($file in $files) {
        & $java.Source -jar $PlantUmlJar "-t$format" -o $renderDir $file.FullName
        if ($LASTEXITCODE -ne 0) {
            Write-Error "PlantUML failed for $($file.Name)"
        }
    }
} else {
    $plantuml = Get-Command plantuml -ErrorAction SilentlyContinue
    if (-not $plantuml) {
        Write-Error "PlantUML command was not found. Run with -Mode check only, install PlantUML, or pass -PlantUmlJar."
    }

    foreach ($file in $files) {
        & $plantuml.Source "-t$format" -o $renderDir $file.FullName
        if ($LASTEXITCODE -ne 0) {
            Write-Error "PlantUML failed for $($file.Name)"
        }
    }
}

$rendered = Get-ChildItem -Path $renderDir -Filter "*.$format" -File -ErrorAction SilentlyContinue
Write-Host "Rendered $($rendered.Count) .$format files to $renderDir"

if ($rendered.Count -lt $files.Count) {
    Write-Error "Rendered file count is lower than source file count."
}

