param(
    [string]$ObsPrefix = "",
    [string]$QtPrefix = "",
    [string]$UserDataDir = "",
    [string]$BuildDir = "",
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$Configuration = "Release",
    [string]$ObsLogPath = "",
    [string]$WorkerLogDir = "",
    [string]$ReportPath = "",
    [string]$DockState = "",
    [string]$SettingsFlow = "",
    [string]$WorkerOwnership = "",
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-RepoPath {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ""
    }
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }
    return Join-Path (Get-Location) $Path
}

function Get-CommandPath {
    param([string]$Name)
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -eq $command) {
        return ""
    }
    return $command.Source
}

function Invoke-LoggedCommand {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )
    $display = "$FilePath $($Arguments -join ' ')"
    Write-Host "Running: $display"
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code $LASTEXITCODE`: $display"
    }
}

function Test-FileContains {
    param(
        [string]$Path,
        [string]$Needle
    )
    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path -LiteralPath $Path)) {
        return $false
    }
    return Select-String -LiteralPath $Path -SimpleMatch $Needle -Quiet
}

function Add-CheckLine {
    param(
        [System.Collections.Generic.List[string]]$Lines,
        [string]$Label,
        [bool]$Passed
    )
    $mark = if ($Passed) { "x" } else { " " }
    $Lines.Add("- [$mark] $Label")
}

$repoRoot = Get-Location
if ([string]::IsNullOrWhiteSpace($UserDataDir)) {
    $UserDataDir = Join-Path $repoRoot "user_data"
}
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $repoRoot "build/plugin"
}
if ([string]::IsNullOrWhiteSpace($WorkerLogDir)) {
    $WorkerLogDir = Join-Path $UserDataDir "logs"
}
if ([string]::IsNullOrWhiteSpace($ReportPath)) {
    $ReportPath = Join-Path $BuildDir "v0.4-plugin-smoke-report.md"
}

$UserDataDir = Resolve-RepoPath $UserDataDir
$BuildDir = Resolve-RepoPath $BuildDir
$WorkerLogDir = Resolve-RepoPath $WorkerLogDir
$ReportPath = Resolve-RepoPath $ReportPath

$cmake = Get-CommandPath "cmake"
$cl = Get-CommandPath "cl"
$qmake = Get-CommandPath "qmake"
$obs64 = Get-CommandPath "obs64"

$configureSucceeded = $false
$buildSucceeded = $false
$artifactPath = Join-Path $BuildDir "$Configuration/obs-duel-recorder.dll"

if (-not $SkipBuild) {
    if ([string]::IsNullOrWhiteSpace($cmake)) {
        throw "cmake was not found. Install CMake 3.24+ or rerun with -SkipBuild to collect manual evidence only."
    }

    $prefixes = New-Object System.Collections.Generic.List[string]
    if (-not [string]::IsNullOrWhiteSpace($ObsPrefix)) {
        $prefixes.Add($ObsPrefix)
    }
    if (-not [string]::IsNullOrWhiteSpace($QtPrefix)) {
        $prefixes.Add($QtPrefix)
    }

    $configureArgs = @(
        "-S", "app/plugin",
        "-B", $BuildDir,
        "-G", "Visual Studio 17 2022",
        "-A", "x64"
    )
    if ($prefixes.Count -gt 0) {
        $configureArgs += "-DCMAKE_PREFIX_PATH=$($prefixes -join ';')"
    }

    Invoke-LoggedCommand -FilePath $cmake -Arguments $configureArgs
    $configureSucceeded = $true

    Invoke-LoggedCommand -FilePath $cmake -Arguments @("--build", $BuildDir, "--config", $Configuration)
    $buildSucceeded = $true
}

$artifactExists = Test-Path -LiteralPath $artifactPath
$settingsPath = Join-Path $env:APPDATA "obs-duel-recorder/plugin-settings.json"
$settingsExists = Test-Path -LiteralPath $settingsPath

$obsLogExists = -not [string]::IsNullOrWhiteSpace($ObsLogPath) -and (Test-Path -LiteralPath $ObsLogPath)
$workerLogExists = Test-Path -LiteralPath $WorkerLogDir
$workerLogFiles = @()
if ($workerLogExists) {
    $workerLogFiles = @(Get-ChildItem -LiteralPath $WorkerLogDir -Filter "*.log" -File -ErrorAction SilentlyContinue)
}

$checks = New-Object System.Collections.Generic.List[string]
Add-CheckLine $checks "CMake configure succeeded or build was skipped intentionally" ($configureSucceeded -or $SkipBuild)
Add-CheckLine $checks "Plugin build succeeded or build was skipped intentionally" ($buildSucceeded -or $SkipBuild)
Add-CheckLine $checks "Plugin artifact exists at $artifactPath" $artifactExists
Add-CheckLine $checks "Settings file exists at $settingsPath" $settingsExists
Add-CheckLine $checks "OBS log path was provided and exists" $obsLogExists
Add-CheckLine $checks "Worker log directory exists at $WorkerLogDir" $workerLogExists
Add-CheckLine $checks "OBS log contains plugin startup" (Test-FileContains $ObsLogPath "OBS Duel Recorder plugin startup")
Add-CheckLine $checks "OBS log contains dock registration" (Test-FileContains $ObsLogPath "dock registered")
Add-CheckLine $checks "OBS log contains settings save and worker restart evidence" (Test-FileContains $ObsLogPath "apply=worker_restart")
Add-CheckLine $checks "OBS log contains worker preflight or launch evidence" (Test-FileContains $ObsLogPath "worker preflight")
Add-CheckLine $checks "OBS log contains heartbeat baseline" (Test-FileContains $ObsLogPath "heartbeat baseline")
Add-CheckLine $checks "OBS log contains plugin shutdown" (Test-FileContains $ObsLogPath "OBS Duel Recorder plugin shutdown")
Add-CheckLine $checks "Dock state was recorded" (-not [string]::IsNullOrWhiteSpace($DockState))
Add-CheckLine $checks "Settings flow result was recorded" (-not [string]::IsNullOrWhiteSpace($SettingsFlow))
Add-CheckLine $checks "Worker ownership was recorded" (-not [string]::IsNullOrWhiteSpace($WorkerOwnership))

$reportDir = Split-Path -Parent $ReportPath
if (-not [string]::IsNullOrWhiteSpace($reportDir)) {
    New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("# v0.4 Plugin Smoke Report")
$lines.Add("")
$lines.Add("Generated: $(Get-Date -Format o)")
$lines.Add("")
$lines.Add("## Environment")
$lines.Add("")
$lines.Add("- OS: $([System.Environment]::OSVersion.VersionString)")
$lines.Add("- PowerShell: $($PSVersionTable.PSVersion)")
$lines.Add("- Repository: $repoRoot")
$lines.Add("- cmake: $cmake")
$lines.Add("- cl: $cl")
$lines.Add("- qmake: $qmake")
$lines.Add("- obs64: $obs64")
$lines.Add("- OBS prefix: $ObsPrefix")
$lines.Add("- Qt prefix: $QtPrefix")
$lines.Add("- User data dir: $UserDataDir")
$lines.Add("- Worker log dir: $WorkerLogDir")
$lines.Add("- OBS log path: $ObsLogPath")
$lines.Add("")
$lines.Add("## Build")
$lines.Add("")
$lines.Add("- Configuration: $Configuration")
$lines.Add("- Build dir: $BuildDir")
$lines.Add("- Artifact: $artifactPath")
$lines.Add("- Configure succeeded: $configureSucceeded")
$lines.Add("- Build succeeded: $buildSucceeded")
$lines.Add("- Artifact exists: $artifactExists")
$lines.Add("")
$lines.Add("## Manual Observations")
$lines.Add("")
$lines.Add("- Dock state: $DockState")
$lines.Add("- Settings flow: $SettingsFlow")
$lines.Add("- Worker ownership: $WorkerOwnership")
$lines.Add("")
$lines.Add("## Evidence Checks")
$lines.Add("")
foreach ($check in $checks) {
    $lines.Add($check)
}
$lines.Add("")
$lines.Add("## Settings")
$lines.Add("")
$lines.Add("- Settings path: $settingsPath")
$lines.Add("- Settings exists: $settingsExists")
if ($settingsExists) {
    $lines.Add("")
    $lines.Add('```json')
    $lines.Add((Get-Content -LiteralPath $settingsPath -Raw).Trim())
    $lines.Add('```')
}
$lines.Add("")
$lines.Add("## Worker Logs")
$lines.Add("")
if ($workerLogFiles.Count -eq 0) {
    $lines.Add("- No Worker log files found.")
} else {
    foreach ($file in $workerLogFiles) {
        $lines.Add("- $($file.FullName)")
    }
}
$lines.Add("")
$lines.Add("## Next Action")
$lines.Add("")
$lines.Add("Paste this report into #110 or the relevant child issue only after confirming no secrets or local-only paths need redaction.")

Set-Content -LiteralPath $ReportPath -Value $lines -Encoding UTF8
Write-Host "Wrote smoke report: $ReportPath"
