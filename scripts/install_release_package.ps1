param(
    [Parameter(Mandatory = $true)]
    [string]$ObsRoot,

    [string]$PackageRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path,

    [switch]$ElevatedAttempt
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Test-WindowsAdmin {
    try {
        $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
        $principal = New-Object Security.Principal.WindowsPrincipal($identity)
        return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    } catch {
        return $false
    }
}

function Test-ProtectedObsRoot {
    param([string]$Path)

    $programFilesRoots = @(
        [Environment]::GetFolderPath("ProgramFiles"),
        [Environment]::GetFolderPath("ProgramFilesX86")
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }

    $normalizedPath = $Path.TrimEnd("\", "/").ToLowerInvariant()
    foreach ($root in $programFilesRoots) {
        $normalizedRoot = $root.TrimEnd("\", "/").ToLowerInvariant()
        if ($normalizedPath -eq $normalizedRoot -or $normalizedPath.StartsWith($normalizedRoot + "\")) {
            return $true
        }
    }
    return $false
}

function Invoke-ElevatedSelf {
    param(
        [string]$PackageRootPath,
        [string]$ObsRootPath
    )

    $scriptPath = $PSCommandPath.Replace('"', '\"')
    $packageArg = $PackageRootPath.Replace('"', '\"')
    $obsArg = $ObsRootPath.Replace('"', '\"')
    $arguments = "-NoLogo -NoProfile -ExecutionPolicy Bypass -File `"$scriptPath`" -PackageRoot `"$packageArg`" -ObsRoot `"$obsArg`" -ElevatedAttempt"
    Write-Output "Administrator permission is required to update OBS under Program Files."
    Write-Output "Requesting Windows UAC elevation. Approve the prompt to continue."
    try {
        $process = Start-Process -FilePath "powershell.exe" -Verb RunAs -ArgumentList $arguments -Wait -PassThru
    } catch {
        throw "Administrator elevation was cancelled or failed. Start PowerShell as Administrator and run this script again."
    }
    if ($null -eq $process.ExitCode) {
        exit 1
    }
    exit $process.ExitCode
}

function Resolve-ExistingDirectory {
    param(
        [string]$Path,
        [string]$Label
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw "$Label does not exist or is not a directory: $Path"
    }
    return (Resolve-Path -LiteralPath $Path).Path
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

function Copy-DirectoryContents {
    param(
        [string]$Source,
        [string]$Destination
    )

    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Get-ChildItem -LiteralPath $Source -Force | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $Destination -Recurse -Force
    }
}

$packageRootPath = Resolve-ExistingDirectory -Path $PackageRoot -Label "Package root"
$obsRootPath = Resolve-ExistingDirectory -Path $ObsRoot -Label "OBS root"

if (Test-ProtectedObsRoot -Path $obsRootPath -and -not (Test-WindowsAdmin)) {
    if ($ElevatedAttempt) {
        throw "Administrator elevation did not produce an administrator PowerShell process. Start PowerShell as Administrator and run this script again."
    }
    Invoke-ElevatedSelf -PackageRootPath $packageRootPath -ObsRootPath $obsRootPath
}

$sourcePlugin = Join-Path $packageRootPath "app\plugin\obs-duel-recorder.dll"
$sourceWorkerDir = Join-Path $packageRootPath "app\worker\odr-worker"
$sourceWorkerExe = Join-Path $sourceWorkerDir "odr-worker.exe"

$obsExeCandidates = @(
    (Join-Path $obsRootPath "bin\64bit\obs64.exe"),
    (Join-Path $obsRootPath "obs64.exe")
)
$obsExe = $obsExeCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($obsExe)) {
    throw "OBS root does not look like a normal or portable OBS installation. Expected one of: $($obsExeCandidates -join ', ')"
}

$targetPluginDir = Join-Path $obsRootPath "obs-plugins\64bit"
$targetWorkerDir = Join-Path $obsRootPath "obs-plugins\worker\odr-worker"
$targetPlugin = Join-Path $targetPluginDir "obs-duel-recorder.dll"
$wrongWorkerDir = Join-Path $obsRootPath "obs-plugins\64bit\worker"

if (-not (Test-Path -LiteralPath $targetPluginDir -PathType Container)) {
    throw "OBS Plugin directory is missing. Use the OBS root folder, not bin or obs-plugins directly: $targetPluginDir"
}
if (-not (Test-Path -LiteralPath $sourcePlugin -PathType Leaf)) {
    throw "Package Plugin DLL is missing: $sourcePlugin"
}
if (-not (Test-WorkerBundle -WorkerDir $sourceWorkerDir)) {
    throw "Package Worker bundle is missing or incomplete. Expected odr-worker.exe and bundled files under: $sourceWorkerDir"
}

Write-Output "OBS Duel Recorder ZIP install/update"
Write-Output "Package root: $packageRootPath"
Write-Output "OBS root:     $obsRootPath"
Write-Output "OBS exe:      $obsExe"
Write-Output ""
Write-Output "Copy Plugin:"
Write-Output "  $sourcePlugin"
Write-Output "  -> $targetPlugin"
Copy-Item -LiteralPath $sourcePlugin -Destination $targetPlugin -Force

Write-Output "Copy Worker bundle:"
Write-Output "  $sourceWorkerDir"
Write-Output "  -> $targetWorkerDir"
Copy-DirectoryContents -Source $sourceWorkerDir -Destination $targetWorkerDir

if (Test-Path -LiteralPath $wrongWorkerDir -PathType Container) {
    Write-Output ""
    Write-Output "WARNING: Known wrong Worker placement still exists:"
    Write-Output "  $wrongWorkerDir"
    Write-Output "The installer does not delete user-created folders. Remove or ignore it after confirming the correct Worker path below."
}

Write-Output ""
Write-Output "Runtime user_data is not modified by this assistant."
Write-Output ""

& (Join-Path $PSScriptRoot "verify_obs_install_layout.ps1") -PackageRoot $packageRootPath -ObsRoot $obsRootPath

Write-Output ""
Write-Output "OK: Install/update assistant finished. Restart OBS before testing the Dock."
