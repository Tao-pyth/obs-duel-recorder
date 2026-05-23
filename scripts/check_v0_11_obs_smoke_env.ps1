param(
    [string]$ObsPrefix = "",
    [string]$ObsFrontendApiDir = "",
    [string]$ObsDepsPrefix = "",
    [string]$W32PthreadsPrefix = "",
    [string]$CMakeModulePath = "",
    [string]$ObsRuntimePath = "",
    [string]$QtPrefix = "",
    [string]$BuildDir = "build/plugin",
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$Configuration = "Release",
    [switch]$AttemptConfigure
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-CommandPath {
    param([string]$Name)
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -eq $command) {
        return ""
    }
    return $command.Source
}

function Test-AnyPath {
    param([string[]]$Paths)
    foreach ($path in $Paths) {
        if (Test-Path -LiteralPath $path) {
            return $path
        }
    }
    return ""
}

function Find-FirstFile {
    param(
        [string]$Root,
        [string]$Filter
    )
    if ([string]::IsNullOrWhiteSpace($Root) -or -not (Test-Path -LiteralPath $Root)) {
        return ""
    }
    $match = Get-ChildItem -LiteralPath $Root -Filter $Filter -Recurse -File -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($null -eq $match) {
        return ""
    }
    return $match.FullName
}

function Add-CheckLine {
    param(
        [System.Collections.Generic.List[string]]$Lines,
        [string]$Label,
        [bool]$Passed,
        [string]$Detail = ""
    )
    $mark = if ($Passed) { "PASS" } else { "FAIL" }
    if ([string]::IsNullOrWhiteSpace($Detail)) {
        $Lines.Add("- ${mark}: $Label")
    } else {
        $Lines.Add("- ${mark}: $Label - $Detail")
    }
}

$repoRoot = (Get-Location).Path
$repoParent = Split-Path -Parent $repoRoot

if (-not [string]::IsNullOrWhiteSpace($ObsPrefix) -and (Test-Path -LiteralPath $ObsPrefix)) {
    if ([string]::IsNullOrWhiteSpace($ObsFrontendApiDir)) {
        $candidate = Join-Path $ObsPrefix "frontend\api"
        if (Test-Path -LiteralPath $candidate) {
            $ObsFrontendApiDir = $candidate
        }
    }
    if ([string]::IsNullOrWhiteSpace($W32PthreadsPrefix)) {
        $candidate = Join-Path $ObsPrefix "deps\w32-pthreads"
        if (Test-Path -LiteralPath $candidate) {
            $W32PthreadsPrefix = $candidate
        }
    }

    $sourceRootCandidate = Split-Path -Parent $ObsPrefix
    if ([string]::IsNullOrWhiteSpace($CMakeModulePath)) {
        $candidate = Join-Path $sourceRootCandidate "cmake\finders"
        if (Test-Path -LiteralPath $candidate) {
            $CMakeModulePath = $candidate
        }
    }
    if ([string]::IsNullOrWhiteSpace($ObsDepsPrefix)) {
        $candidate = Get-ChildItem -LiteralPath (Join-Path $sourceRootCandidate ".deps") -Directory -Filter "obs-deps-*-x64" -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($null -ne $candidate) {
            $ObsDepsPrefix = $candidate.FullName
        }
    }
}

$knownCmake = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$cmake = Get-CommandPath "cmake"
if ([string]::IsNullOrWhiteSpace($cmake) -and (Test-Path -LiteralPath $knownCmake)) {
    $cmake = $knownCmake
}

$vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
$vsInstall = ""
if (Test-Path -LiteralPath $vswhere) {
    $vsInstall = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath) -join ""
}

$cl = Get-CommandPath "cl"
if ([string]::IsNullOrWhiteSpace($cl) -and -not [string]::IsNullOrWhiteSpace($vsInstall)) {
    $cl = Test-AnyPath @(
        (Join-Path $vsInstall "VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe")
    )
    if ([string]::IsNullOrWhiteSpace($cl)) {
        $cl = Find-FirstFile (Join-Path $vsInstall "VC\Tools\MSVC") "cl.exe"
    }
}

$obs64 = $ObsRuntimePath
if ([string]::IsNullOrWhiteSpace($obs64)) {
    $obs64 = Get-CommandPath "obs64"
}
if ([string]::IsNullOrWhiteSpace($obs64)) {
    $obs64 = Test-AnyPath @(
        "C:\Program Files\obs-studio\bin\64bit\obs64.exe",
        "C:\Program Files (x86)\obs-studio\bin\64bit\obs64.exe",
        "C:\obs-studio\bin\64bit\obs64.exe",
        (Join-Path $repoParent "_smoke\tools\obs-studio-32.1.2\bin\64bit\obs64.exe")
    )
}

$knownQtRoot = Test-AnyPath @("C:\Qt", $QtPrefix)
$libobsConfig = Find-FirstFile $ObsPrefix "libobsConfig.cmake"
$frontendConfig = Find-FirstFile $ObsFrontendApiDir "obs-frontend-apiConfig.cmake"
if ([string]::IsNullOrWhiteSpace($frontendConfig)) {
    $frontendConfig = Find-FirstFile $ObsPrefix "obs-frontend-apiConfig.cmake"
}
$w32PthreadsConfig = Find-FirstFile $W32PthreadsPrefix "w32-pthreadsConfig.cmake"
$simdeHeaders = Find-FirstFile $ObsDepsPrefix "simde-common.h"
$moduleFinder = Find-FirstFile $CMakeModulePath "FindSIMDe.cmake"
$qtConfig = Find-FirstFile $QtPrefix "Qt6Config.cmake"
$artifact = Join-Path $BuildDir "$Configuration/obs-duel-recorder.dll"

$cmdPathOutput = (cmd.exe /d /s /c "set path") -split "`r?`n"
$pathKeys = @($cmdPathOutput | Where-Object { $_ -match "^(PATH|Path)=" })
$hasDuplicatePathKeys = $pathKeys.Count -gt 1

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("# v0.11 OBS Smoke Environment Check")
$lines.Add("")
$lines.Add("Generated: $(Get-Date -Format o)")
$lines.Add("Repository: $repoRoot")
$lines.Add("")
$lines.Add("## Inputs")
$lines.Add("")
$lines.Add("- ObsPrefix: $ObsPrefix")
$lines.Add("- ObsFrontendApiDir: $ObsFrontendApiDir")
$lines.Add("- ObsDepsPrefix: $ObsDepsPrefix")
$lines.Add("- W32PthreadsPrefix: $W32PthreadsPrefix")
$lines.Add("- CMakeModulePath: $CMakeModulePath")
$lines.Add("- ObsRuntimePath: $ObsRuntimePath")
$lines.Add("- QtPrefix: $QtPrefix")
$lines.Add("- BuildDir: $BuildDir")
$lines.Add("- Configuration: $Configuration")
$lines.Add("")
$lines.Add("## Checks")
$lines.Add("")
Add-CheckLine $lines "CMake is available" (-not [string]::IsNullOrWhiteSpace($cmake)) $cmake
Add-CheckLine $lines "Visual Studio 2022 C++ install is detectable" (-not [string]::IsNullOrWhiteSpace($vsInstall)) $vsInstall
Add-CheckLine $lines "MSVC cl.exe is available" (-not [string]::IsNullOrWhiteSpace($cl)) $cl
Add-CheckLine $lines "OBS Studio x64 is installed" (-not [string]::IsNullOrWhiteSpace($obs64)) $obs64
Add-CheckLine $lines "OBS libobs CMake package is available" (-not [string]::IsNullOrWhiteSpace($libobsConfig)) $libobsConfig
Add-CheckLine $lines "OBS frontend API CMake package is available" (-not [string]::IsNullOrWhiteSpace($frontendConfig)) $frontendConfig
Add-CheckLine $lines "OBS w32-pthreads CMake package is available" (-not [string]::IsNullOrWhiteSpace($w32PthreadsConfig)) $w32PthreadsConfig
Add-CheckLine $lines "OBS deps SIMDe headers are available" (-not [string]::IsNullOrWhiteSpace($simdeHeaders)) $simdeHeaders
Add-CheckLine $lines "OBS CMake finder modules are available" (-not [string]::IsNullOrWhiteSpace($moduleFinder)) $moduleFinder
Add-CheckLine $lines "Qt root is detectable in a common location" (-not [string]::IsNullOrWhiteSpace($knownQtRoot)) $knownQtRoot
Add-CheckLine $lines "Qt6 CMake package is available" (-not [string]::IsNullOrWhiteSpace($qtConfig)) $qtConfig
Add-CheckLine $lines "PowerShell process has a single PATH key for MSBuild" (-not $hasDuplicatePathKeys) (($pathKeys -join "; "))
Add-CheckLine $lines "Plugin DLL artifact exists" (Test-Path -LiteralPath $artifact) $artifact

$lines.Add("")
$lines.Add("## Configure Command")
$lines.Add("")
$lines.Add('Use a sanitized Developer Command Prompt when the environment contains both PATH and Path:')
$lines.Add("")
$lines.Add('```cmd')
$lines.Add('set PATH=')
$lines.Add('set Path=C:\WINDOWS\system32;C:\WINDOWS;C:\WINDOWS\System32\Wbem;C:\WINDOWS\System32\WindowsPowerShell\v1.0\;C:\WINDOWS\System32\OpenSSH\;C:\Program Files\Git\cmd')
$lines.Add('call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64')
$lines.Add('"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S app/plugin -B build/plugin -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="<obs-prefix>;<obs-frontend-api-dir>;<w32-pthreads-prefix>;<obs-deps-prefix>;<qt-prefix>" -DCMAKE_MODULE_PATH="<obs-source>\cmake\finders"')
$lines.Add('cmake --build build/plugin --config Release')
$lines.Add('```')

if ($AttemptConfigure) {
    $lines.Add("")
    $lines.Add("## Configure Attempt")
    $lines.Add("")
    if ([string]::IsNullOrWhiteSpace($cmake)) {
        $lines.Add("- SKIP: cmake is unavailable.")
    } elseif ([string]::IsNullOrWhiteSpace($vsInstall)) {
        $lines.Add("- SKIP: Visual Studio Build Tools install was not found.")
    } else {
        $prefixes = New-Object System.Collections.Generic.List[string]
        if (-not [string]::IsNullOrWhiteSpace($ObsPrefix)) {
            $prefixes.Add($ObsPrefix)
        }
        if (-not [string]::IsNullOrWhiteSpace($ObsFrontendApiDir)) {
            $prefixes.Add($ObsFrontendApiDir)
        }
        if (-not [string]::IsNullOrWhiteSpace($W32PthreadsPrefix)) {
            $prefixes.Add($W32PthreadsPrefix)
        }
        if (-not [string]::IsNullOrWhiteSpace($ObsDepsPrefix)) {
            $prefixes.Add($ObsDepsPrefix)
        }
        if (-not [string]::IsNullOrWhiteSpace($QtPrefix)) {
            $prefixes.Add($QtPrefix)
        }
        $args = @("-S", "app/plugin", "-B", $BuildDir, "-G", "Visual Studio 17 2022", "-A", "x64")
        if ($prefixes.Count -gt 0) {
            $args += "-DCMAKE_PREFIX_PATH=$($prefixes -join ';')"
        }
        if (-not [string]::IsNullOrWhiteSpace($CMakeModulePath)) {
            $args += "-DCMAKE_MODULE_PATH=$CMakeModulePath"
        }
        $vsDevCmd = Join-Path $vsInstall "Common7\Tools\VsDevCmd.bat"
        $safePath = "C:\WINDOWS\system32;C:\WINDOWS;C:\WINDOWS\System32\Wbem;C:\WINDOWS\System32\WindowsPowerShell\v1.0\;C:\WINDOWS\System32\OpenSSH\;C:\Program Files\Git\cmd"
        $cmakeArgs = @($args | ForEach-Object {
            if ($_ -match "\s|;") {
                '"' + ($_ -replace '"', '\"') + '"'
            } else {
                $_
            }
        }) -join " "
        $commandLine = "set PATH=& set Path=$safePath& call `"$vsDevCmd`" -arch=x64 -host_arch=x64 && `"$cmake`" $cmakeArgs"
        $lines.Add("- Command: $commandLine")
        & cmd.exe /d /s /c $commandLine
        $lines.Add("- Exit code: $LASTEXITCODE")
    }
}

$lines | ForEach-Object { Write-Output $_ }
