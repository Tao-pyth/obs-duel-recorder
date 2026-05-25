@echo off
setlocal
cd /d "%~dp0"

if "%~1"=="" (
  echo Usage: install.bat "C:\Path\To\OBS"
  echo Example: install.bat "C:\Program Files\obs-studio"
  exit /b 64
)

powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\install_release_package.ps1" -PackageRoot "%~dp0." -ObsRoot "%~f1"
exit /b %ERRORLEVEL%
