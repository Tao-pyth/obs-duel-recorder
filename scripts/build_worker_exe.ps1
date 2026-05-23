param(
    [string]$PythonExe = "python",

    [string]$OutputDir = "build/worker",

    [switch]$Clean
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-RepoPath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }
    return (Join-Path $RepoRoot $Path)
}

$RepoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$outputRoot = Resolve-RepoPath $OutputDir
$workerBundleRoot = Join-Path $outputRoot "odr-worker"
$workRoot = Join-Path $RepoRoot "build/pyinstaller/odr-worker"
$entryPoint = Join-Path $RepoRoot "app/worker/odr_worker/exe_main.py"
$workerPath = Join-Path $RepoRoot "app/worker"
$migrationsPath = Join-Path $RepoRoot "app/worker/odr_worker/migrations"
$exePath = Join-Path $workerBundleRoot "odr-worker.exe"

if (-not (Test-Path -LiteralPath $entryPoint -PathType Leaf)) {
    throw "Worker entrypoint was not found: $entryPoint"
}

if (-not (Test-Path -LiteralPath $migrationsPath -PathType Container)) {
    throw "Worker migrations directory was not found: $migrationsPath"
}

if ($Clean) {
    foreach ($path in @($workerBundleRoot, $workRoot)) {
        if (Test-Path -LiteralPath $path) {
            Remove-Item -LiteralPath $path -Recurse -Force
        }
    }
}

New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
New-Item -ItemType Directory -Force -Path $workRoot | Out-Null

$addData = "$migrationsPath;odr_worker/migrations"
$args = @(
    "-m", "PyInstaller",
    "--noconfirm",
    "--clean",
    "--onedir",
    "--name", "odr-worker",
    "--distpath", $outputRoot,
    "--workpath", $workRoot,
    "--specpath", $workRoot,
    "--paths", $workerPath,
    "--add-data", $addData,
    "--collect-submodules", "uvicorn",
    "--collect-submodules", "fastapi",
    "--collect-submodules", "starlette",
    "--collect-submodules", "pydantic",
    "--collect-submodules", "pydantic_core",
    $entryPoint
)

& $PythonExe @args

if ($LASTEXITCODE -ne 0) {
    throw "PyInstaller failed with exit code $LASTEXITCODE"
}

if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) {
    throw "Worker executable was not created: $exePath"
}

$versionOutput = & $exePath --version
if ($LASTEXITCODE -ne 0) {
    throw "Worker executable version check failed with exit code $LASTEXITCODE"
}

Write-Output "Worker executable: $exePath"
Write-Output "Worker version: $versionOutput"
