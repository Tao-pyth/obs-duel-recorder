param(
    [Parameter(Mandatory = $true)]
    [string]$Version,

    [string]$PluginDllPath = "build/plugin/Release/obs-duel-recorder.dll",

    [string]$OutputDir = "build/release",

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

function Copy-RequiredFile {
    param(
        [string]$Source,
        [string]$Destination
    )

    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        throw "Required file is missing: $Source"
    }

    $parent = Split-Path -Parent $Destination
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
}

function Copy-RequiredDirectory {
    param(
        [string]$Source,
        [string]$Destination
    )

    if (-not (Test-Path -LiteralPath $Source -PathType Container)) {
        throw "Required directory is missing: $Source"
    }

    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Get-ChildItem -LiteralPath $Source -Force | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $Destination -Recurse -Force
    }
}

function Assert-NoBlockedPackageFiles {
    param([string]$PackageRoot)

    $blockedPatterns = @(
        '(^|\\|/)user_data(\\|/)',
        '(^|\\|/)logs(\\|/)',
        '(^|\\|/)videos(\\|/)',
        '(^|\\|/)screenshots(\\|/)',
        '(^|\\|/)exports(\\|/)',
        '(^|\\|/)config(\\|/)secrets(\\|/)',
        '(^|\\|/)secrets(\\|/)',
        '\.sqlite$',
        '\.sqlite3$',
        '\.db$',
        '\.log$',
        '(^|\\|/)token\.json$',
        '(^|\\|/)credentials\.json$',
        '\.token$'
    )

    $files = Get-ChildItem -LiteralPath $PackageRoot -Recurse -File
    $rootFullPath = (Resolve-Path -LiteralPath $PackageRoot).Path.TrimEnd('\', '/')
    foreach ($file in $files) {
        $fileFullPath = $file.FullName
        if ($fileFullPath.StartsWith($rootFullPath, [System.StringComparison]::OrdinalIgnoreCase)) {
            $relative = $fileFullPath.Substring($rootFullPath.Length).TrimStart('\', '/')
        }
        else {
            $relative = $fileFullPath
        }
        foreach ($pattern in $blockedPatterns) {
            if ($relative -match $pattern) {
                throw "Blocked runtime or secret file would be packaged: $relative"
            }
        }
    }
}

$RepoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path

if ($Version -notmatch '^v?\d+\.\d+\.\d+$') {
    throw "Version must use X.Y.Z or vX.Y.Z format: $Version"
}

$tagVersion = if ($Version.StartsWith("v")) { $Version } else { "v$Version" }
$packageName = "obs-duel-recorder-$tagVersion"
$outputRoot = Resolve-RepoPath $OutputDir
$packageRoot = Join-Path $outputRoot $packageName
$zipPath = Join-Path $outputRoot "$packageName.zip"
$checksumPath = Join-Path $outputRoot "SHA256SUMS.txt"
$pluginDll = Resolve-RepoPath $PluginDllPath

if (-not (Test-Path -LiteralPath $pluginDll -PathType Leaf)) {
    throw "Plugin DLL is required for release packaging: $pluginDll"
}

if ((Split-Path -Leaf $pluginDll) -ne "obs-duel-recorder.dll") {
    throw "Plugin DLL file name must be obs-duel-recorder.dll: $pluginDll"
}

New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null

if ($Clean) {
    foreach ($path in @($packageRoot, $zipPath, $checksumPath)) {
        if (Test-Path -LiteralPath $path) {
            Remove-Item -LiteralPath $path -Recurse -Force
        }
    }
}

if (Test-Path -LiteralPath $packageRoot) {
    throw "Package directory already exists. Use -Clean to replace it: $packageRoot"
}

New-Item -ItemType Directory -Force -Path $packageRoot | Out-Null

Copy-RequiredFile -Source $pluginDll -Destination (Join-Path $packageRoot "app/plugin/obs-duel-recorder.dll")

$workerSource = Join-Path $RepoRoot "app/worker"
$workerDest = Join-Path $packageRoot "app/worker"
Copy-RequiredDirectory -Source (Join-Path $workerSource "odr_worker") -Destination (Join-Path $workerDest "odr_worker")
Copy-RequiredFile -Source (Join-Path $workerSource "pyproject.toml") -Destination (Join-Path $workerDest "pyproject.toml")
Copy-RequiredFile -Source (Join-Path $workerSource "README.md") -Destination (Join-Path $workerDest "README.md")
Copy-RequiredFile -Source (Join-Path $workerSource "requirements.txt") -Destination (Join-Path $workerDest "requirements.txt")

Copy-RequiredFile -Source (Join-Path $RepoRoot "update.bat") -Destination (Join-Path $packageRoot "update.bat")
Copy-RequiredFile -Source (Join-Path $RepoRoot "README.md") -Destination (Join-Path $packageRoot "README.md")
Copy-RequiredFile -Source (Join-Path $RepoRoot "LICENSE") -Destination (Join-Path $packageRoot "LICENSE")

Copy-RequiredDirectory -Source (Join-Path $RepoRoot "docs/user") -Destination (Join-Path $packageRoot "docs/user")
Copy-RequiredFile -Source (Join-Path $RepoRoot "docs/README.md") -Destination (Join-Path $packageRoot "docs/README.md")
Copy-RequiredFile -Source (Join-Path $RepoRoot "docs/architecture/packaging.md") -Destination (Join-Path $packageRoot "docs/architecture/packaging.md")
Copy-RequiredFile -Source (Join-Path $RepoRoot "docs/architecture/update-system.md") -Destination (Join-Path $packageRoot "docs/architecture/update-system.md")
Copy-RequiredFile -Source (Join-Path $RepoRoot "docs/architecture/plugin-worker.md") -Destination (Join-Path $packageRoot "docs/architecture/plugin-worker.md")

$manifest = [ordered]@{
    package = $packageName
    version = $tagVersion
    created_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    plugin_dll = "app/plugin/obs-duel-recorder.dll"
    worker_package = "app/worker"
    update_entrypoint = "update.bat"
    checksum_file = "SHA256SUMS.txt beside the ZIP"
    docs = @(
        "docs/user",
        "docs/architecture/packaging.md",
        "docs/architecture/update-system.md",
        "docs/architecture/plugin-worker.md"
    )
}

($manifest | ConvertTo-Json -Depth 4) | Set-Content -LiteralPath (Join-Path $packageRoot "RELEASE-MANIFEST.json") -Encoding UTF8

Assert-NoBlockedPackageFiles -PackageRoot $packageRoot

if (Test-Path -LiteralPath $zipPath) {
    throw "ZIP already exists. Use -Clean to replace it: $zipPath"
}

Compress-Archive -LiteralPath $packageRoot -DestinationPath $zipPath -CompressionLevel Optimal

$zipHash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToUpperInvariant()
"$zipHash  $(Split-Path -Leaf $zipPath)" | Set-Content -LiteralPath $checksumPath -Encoding ASCII

& (Join-Path $PSScriptRoot "validate_release_package.ps1") -PackageZip $zipPath -ChecksumFile $checksumPath -ExpectedVersion $tagVersion

Write-Output "Release package: $zipPath"
Write-Output "Checksum file: $checksumPath"
