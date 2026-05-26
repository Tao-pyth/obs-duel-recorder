param(
    [Parameter(Mandatory = $true)]
    [string]$Version,

    [string]$PluginDllPath = "build/plugin/Release/obs-duel-recorder.dll",

    [string]$WorkerBundlePath = "build/worker/odr-worker",

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
        $skipGeneratedPythonCache = ($_.PSIsContainer -and $_.Name -eq "__pycache__") -or
            (-not $_.PSIsContainer -and $_.Extension -eq ".pyc")
        if (-not $skipGeneratedPythonCache) {
            Copy-Item -LiteralPath $_.FullName -Destination $Destination -Recurse -Force
        }
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
        '(^|\\|/)__pycache__(\\|/)',
        '\.pyc$',
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

function Assert-PEExecutable {
    param(
        [string]$Path,
        [string]$Label
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Label is missing: $Path"
    }

    $file = Get-Item -LiteralPath $Path
    if ($file.Length -lt 1024) {
        throw "$Label is too small to be a release executable: $Path ($($file.Length) bytes)"
    }

    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $reader = New-Object System.IO.BinaryReader($stream)
        $mz = $reader.ReadUInt16()
        if ($mz -ne 0x5A4D) {
            throw "$Label is not a PE executable (missing MZ header): $Path"
        }
        $stream.Seek(0x3C, [System.IO.SeekOrigin]::Begin) | Out-Null
        $peOffset = $reader.ReadInt32()
        if ($peOffset -le 0 -or $peOffset -gt ($file.Length - 4)) {
            throw "$Label has an invalid PE header offset: $Path"
        }
        $stream.Seek($peOffset, [System.IO.SeekOrigin]::Begin) | Out-Null
        $peSignature = $reader.ReadUInt32()
        if ($peSignature -ne 0x00004550) {
            throw "$Label is not a PE executable (missing PE signature): $Path"
        }
    }
    finally {
        $stream.Dispose()
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
$workerBundle = Resolve-RepoPath $WorkerBundlePath
$workerExe = Join-Path $workerBundle "odr-worker.exe"

if (-not (Test-Path -LiteralPath $pluginDll -PathType Leaf)) {
    throw "Plugin DLL is required for release packaging: $pluginDll"
}

if ((Split-Path -Leaf $pluginDll) -ne "obs-duel-recorder.dll") {
    throw "Plugin DLL file name must be obs-duel-recorder.dll: $pluginDll"
}

if (-not (Test-Path -LiteralPath $workerBundle -PathType Container)) {
    throw "Worker executable bundle is required for release packaging: $workerBundle"
}

if (-not (Test-Path -LiteralPath $workerExe -PathType Leaf)) {
    throw "Worker executable is required for release packaging: $workerExe"
}

Assert-PEExecutable -Path $workerExe -Label "Worker executable"
Assert-PEExecutable -Path $pluginDll -Label "Plugin DLL"

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
Copy-RequiredDirectory -Source $workerBundle -Destination (Join-Path $packageRoot "app/worker/odr-worker")

$workerSource = Join-Path $RepoRoot "app/worker"
$workerDest = Join-Path $packageRoot "app/worker"
Copy-RequiredDirectory -Source (Join-Path $workerSource "odr_worker") -Destination (Join-Path $workerDest "odr_worker")
Copy-RequiredFile -Source (Join-Path $workerSource "pyproject.toml") -Destination (Join-Path $workerDest "pyproject.toml")
Copy-RequiredFile -Source (Join-Path $workerSource "README.md") -Destination (Join-Path $workerDest "README.md")
Copy-RequiredFile -Source (Join-Path $workerSource "requirements.txt") -Destination (Join-Path $workerDest "requirements.txt")

Copy-RequiredFile -Source (Join-Path $RepoRoot "update.bat") -Destination (Join-Path $packageRoot "update.bat")
Copy-RequiredFile -Source (Join-Path $RepoRoot "install.bat") -Destination (Join-Path $packageRoot "install.bat")
Copy-RequiredFile -Source (Join-Path $RepoRoot "verify-install.bat") -Destination (Join-Path $packageRoot "verify-install.bat")
Copy-RequiredFile -Source (Join-Path $RepoRoot "README.md") -Destination (Join-Path $packageRoot "README.md")
Copy-RequiredFile -Source (Join-Path $RepoRoot "LICENSE") -Destination (Join-Path $packageRoot "LICENSE")

Copy-RequiredDirectory -Source (Join-Path $RepoRoot "docs/user") -Destination (Join-Path $packageRoot "docs/user")
Copy-RequiredFile -Source (Join-Path $RepoRoot "docs/README.md") -Destination (Join-Path $packageRoot "docs/README.md")
Copy-RequiredFile -Source (Join-Path $RepoRoot "docs/architecture/packaging.md") -Destination (Join-Path $packageRoot "docs/architecture/packaging.md")
Copy-RequiredFile -Source (Join-Path $RepoRoot "docs/architecture/update-system.md") -Destination (Join-Path $packageRoot "docs/architecture/update-system.md")
Copy-RequiredFile -Source (Join-Path $RepoRoot "docs/architecture/plugin-worker.md") -Destination (Join-Path $packageRoot "docs/architecture/plugin-worker.md")
Copy-RequiredFile -Source (Join-Path $RepoRoot "scripts/install_release_package.ps1") -Destination (Join-Path $packageRoot "scripts/install_release_package.ps1")
Copy-RequiredFile -Source (Join-Path $RepoRoot "scripts/verify_obs_install_layout.ps1") -Destination (Join-Path $packageRoot "scripts/verify_obs_install_layout.ps1")

$manifest = [ordered]@{
    package = $packageName
    version = $tagVersion
    created_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    plugin_dll = "app/plugin/obs-duel-recorder.dll"
    worker_executable = "app/worker/odr-worker/odr-worker.exe"
    worker_source_fallback = "app/worker/odr_worker"
    install_entrypoint = "install.bat"
    update_entrypoint = "update.bat"
    install_verifier = "verify-install.bat"
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
