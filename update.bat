@echo off
setlocal
cd /d "%~dp0"
set "ODR_USER_DATA_DIR=%~dp0user_data"
set "ODR_WORKER_EXE=%~dp0app\worker\odr-worker\odr-worker.exe"
if exist "%ODR_WORKER_EXE%" (
  "%ODR_WORKER_EXE%" update %*
  exit /b %ERRORLEVEL%
)

where python >nul 2>nul
if errorlevel 1 (
  echo OBS Duel Recorder update failed: bundled Worker executable was not found and python is not available. 1>&2
  echo Expected bundled Worker: %ODR_WORKER_EXE% 1>&2
  exit /b 9009
)

set "PYTHONPATH=%~dp0app\worker;%PYTHONPATH%"
python -m odr_worker.update_system %*
