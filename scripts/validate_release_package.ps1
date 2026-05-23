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
    "update.bat",
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
