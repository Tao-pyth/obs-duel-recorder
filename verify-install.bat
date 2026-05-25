@echo off
setlocal
cd /d "%~dp0"

if "%~1"=="" (
  echo Usage: verify-install.bat "C:\Path\To\OBS"
  echo Example: verify-install.bat "C:\Program Files\obs-studio"
  exit /b 64
)

powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\verify_obs_install_layout.ps1" -PackageRoot "%~dp0." -ObsRoot "%~f1"
exit /b %ERRORLEVEL%
