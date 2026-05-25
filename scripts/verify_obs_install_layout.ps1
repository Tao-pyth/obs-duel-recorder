param(
    [Parameter(Mandatory = $true)]
    [string]$ObsRoot,

    [string]$PackageRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Join-DisplayPath {
    param(
        [string]$Root,
        [string]$Child
    )

    return (Join-Path $Root $Child)
}

function Test-WorkerBundle {
    param([string]$WorkerDir)

    $exe = Join-Path $WorkerDir "odr-worker.exe"
    if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
        return $false
    }

    $extraItems = @(
        Get-ChildItem -LiteralPath $WorkerDir -Force |
            Where-Object { $_.Name -ne "odr-worker.exe" }
    )
    return ($extraItems.Count -gt 0)
}

function Add-CheckResult {
    param(
        [System.Collections.Generic.List[string]]$Failures,
        [string]$Label,
        [string]$Path,
        [bool]$Passed,
        [string]$FailureMessage
    )

    if ($Passed) {
        Write-Output "[OK] $Label"
        Write-Output "     $Path"
        return
    }

    Write-Output "[FAIL] $Label"
    Write-Output "       $Path"
    $Failures.Add($FailureMessage)
}

$packageRootPath = [System.IO.Path]::GetFullPath($PackageRoot)
$obsRootPath = [System.IO.Path]::GetFullPath($ObsRoot)

$sourcePlugin = Join-DisplayPath $packageRootPath "app\plugin\obs-duel-recorder.dll"
$sourceWorkerDir = Join-DisplayPath $packageRootPath "app\worker\odr-worker"
$sourceWorkerExe = Join-DisplayPath $sourceWorkerDir "odr-worker.exe"

$targetPlugin = Join-DisplayPath $obsRootPath "obs-plugins\64bit\obs-duel-recorder.dll"
$targetWorkerDir = Join-DisplayPath $obsRootPath "obs-plugins\worker\odr-worker"
$targetWorkerExe = Join-DisplayPath $targetWorkerDir "odr-worker.exe"
$wrongWorkerDir = Join-DisplayPath $obsRootPath "obs-plugins\64bit\worker"
$wrongWorkerExe = Join-DisplayPath $wrongWorkerDir "odr-worker\odr-worker.exe"

$failures = [System.Collections.Generic.List[string]]::new()

Write-Output "OBS Duel Recorder install layout verification"
Write-Output "Package root: $packageRootPath"
Write-Output "OBS root:     $obsRootPath"
Write-Output ""

Add-CheckResult `
    -Failures $failures `
    -Label "Package Plugin DLL exists" `
    -Path $sourcePlugin `
    -Passed (Test-Path -LiteralPath $sourcePlugin -PathType Leaf) `
    -FailureMessage "Missing package Plugin DLL. Expected source: $sourcePlugin"

Add-CheckResult `
    -Failures $failures `
    -Label "Package Worker bundle is complete" `
    -Path $sourceWorkerDir `
    -Passed (Test-WorkerBundle -WorkerDir $sourceWorkerDir) `
    -FailureMessage "Missing or incomplete package Worker bundle. Expected source directory with odr-worker.exe and bundled dependency files: $sourceWorkerDir"

Add-CheckResult `
    -Failures $failures `
    -Label "OBS Plugin DLL target exists" `
    -Path $targetPlugin `
    -Passed (Test-Path -LiteralPath $targetPlugin -PathType Leaf) `
    -FailureMessage "Missing OBS Plugin DLL. Copy '$sourcePlugin' to '$targetPlugin'."

Add-CheckResult `
    -Failures $failures `
    -Label "OBS Worker bundle target is complete" `
    -Path $targetWorkerDir `
    -Passed (Test-WorkerBundle -WorkerDir $targetWorkerDir) `
    -FailureMessage "Missing or incomplete OBS Worker bundle. Copy the full directory '$sourceWorkerDir' to '$targetWorkerDir'. Do not copy only '$sourceWorkerExe'."

if (Test-Path -LiteralPath $wrongWorkerDir -PathType Container) {
    Write-Output "[FAIL] Known wrong Worker placement detected"
    Write-Output "       $wrongWorkerDir"
    if (Test-Path -LiteralPath $wrongWorkerExe -PathType Leaf) {
        Write-Output "       Found nested Worker executable: $wrongWorkerExe"
    }
    $failures.Add("Worker is under the wrong directory. Move or copy '$wrongWorkerDir' content to '$targetWorkerDir', then restart OBS.")
}
else {
    Write-Output "[OK] Known wrong Worker placement is absent"
    Write-Output "     $wrongWorkerDir"
}

Write-Output ""
Write-Output "Expected OBS layout:"
Write-Output "  $targetPlugin"
Write-Output "  $targetWorkerExe"

if ($failures.Count -eq 0) {
    Write-Output ""
    Write-Output "OK: OBS Duel Recorder install layout looks valid."
    exit 0
}

Write-Output ""
Write-Output "Problems:"
foreach ($failure in $failures) {
    Write-Output "- $failure"
}

Write-Output ""
Write-Output "Fix the paths above, then restart OBS and run this verifier again."
exit 1
