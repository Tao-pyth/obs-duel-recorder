param(
    [Parameter(Mandatory = $true)]
    [string]$PackageZip,

    [Parameter(Mandatory = $true)]
    [string]$ChecksumFile,

    [Parameter(Mandatory = $true)]
    [string]$ExpectedVersion
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-ExistingPath {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Path does not exist: $Path"
    }
    return (Resolve-Path -LiteralPath $Path).Path
}

$zipPath = Resolve-ExistingPath $PackageZip
$checksumPath = Resolve-ExistingPath $ChecksumFile

if ($ExpectedVersion -notmatch '^v\d+\.\d+\.\d+$') {
    throw "ExpectedVersion must use vX.Y.Z format: $ExpectedVersion"
}

function Assert-ZipEntryIsPEExecutable {
    param(
        [System.IO.Compression.ZipArchive]$Zip,
        [string]$EntryName,
        [string]$Label
    )

    $entry = $Zip.Entries | Where-Object { ($_.FullName -replace '\\', '/') -eq $EntryName } | Select-Object -First 1
    if ($null -eq $entry) {
        throw "Package is missing expected executable: $EntryName"
    }
    if ($entry.Length -lt 1024) {
        throw "$Label is too small to be a release executable: $EntryName ($($entry.Length) bytes)"
    }

    $entryStream = $entry.Open()
    $stream = New-Object System.IO.MemoryStream
    try {
        $entryStream.CopyTo($stream)
        $stream.Seek(0, [System.IO.SeekOrigin]::Begin) | Out-Null
        $reader = New-Object System.IO.BinaryReader($stream)
        $mz = $reader.ReadUInt16()
        if ($mz -ne 0x5A4D) {
            throw "$Label is not a PE executable (missing MZ header): $EntryName"
        }
        $stream.Seek(0x3C, [System.IO.SeekOrigin]::Begin) | Out-Null
        $peOffset = $reader.ReadInt32()
        if ($peOffset -le 0 -or $peOffset -gt ($entry.Length - 4)) {
            throw "$Label has an invalid PE header offset: $EntryName"
        }
        $stream.Seek($peOffset, [System.IO.SeekOrigin]::Begin) | Out-Null
        $peSignature = $reader.ReadUInt32()
        if ($peSignature -ne 0x00004550) {
            throw "$Label is not a PE executable (missing PE signature): $EntryName"
        }
    }
    finally {
        $entryStream.Dispose()
        $stream.Dispose()
    }
}

$zipName = Split-Path -Leaf $zipPath
$expectedRoot = "obs-duel-recorder-$ExpectedVersion/"
$expectedEntries = @(
    "app/plugin/obs-duel-recorder.dll",
    "app/worker/odr-worker/odr-worker.exe",
    "app/worker/pyproject.toml",
    "app/worker/odr_worker/__init__.py",
    "docs/user/index.md",
    "docs/user/ja/index.md",
    "docs/architecture/packaging.md",
    "docs/architecture/update-system.md",
    "scripts/install_release_package.ps1",
    "scripts/verify_obs_install_layout.ps1",
    "install.bat",
    "update.bat",
    "verify-install.bat",
    "README.md",
    "LICENSE",
    "RELEASE-MANIFEST.json"
)

$blockedPatterns = @(
    '(^|/)user_data/',
    '(^|/)logs/',
    '(^|/)videos/',
    '(^|/)screenshots/',
    '(^|/)exports/',
    '(^|/)config/secrets/',
    '(^|/)secrets/',
    '\.sqlite$',
    '\.sqlite3$',
    '\.db$',
    '\.log$',
    '(^|/)__pycache__/',
    '\.pyc$',
    '(^|/)token\.json$',
    '(^|/)credentials\.json$',
    '\.token$'
)

Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead($zipPath)
try {
    $entries = @($zip.Entries | ForEach-Object { $_.FullName -replace '\\', '/' })

    foreach ($entry in $expectedEntries) {
        $expected = "$expectedRoot$entry"
        if ($entries -notcontains $expected) {
            throw "Package is missing expected entry: $expected"
        }
    }

    foreach ($entry in $entries) {
        foreach ($pattern in $blockedPatterns) {
            if ($entry -match $pattern) {
                throw "Package contains blocked runtime or secret entry: $entry"
            }
        }
    }

    Assert-ZipEntryIsPEExecutable -Zip $zip -EntryName "${expectedRoot}app/plugin/obs-duel-recorder.dll" -Label "Plugin DLL"
    Assert-ZipEntryIsPEExecutable -Zip $zip -EntryName "${expectedRoot}app/worker/odr-worker/odr-worker.exe" -Label "Worker executable"
}
finally {
    $zip.Dispose()
}

$actualHash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToUpperInvariant()
$checksumText = Get-Content -LiteralPath $checksumPath -Raw

if ($checksumText -notmatch [regex]::Escape($actualHash)) {
    throw "Checksum file does not contain ZIP SHA256 hash: $actualHash"
}

if ($checksumText -notmatch [regex]::Escape($zipName)) {
    throw "Checksum file does not reference ZIP file name: $zipName"
}

Write-Output "OK: Release package validation passed ($zipName)"
