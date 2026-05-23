@echo off
setlocal
cd /d "%~dp0"
set "ODR_USER_DATA_DIR=%~dp0user_data"
set "PYTHONPATH=%~dp0app\worker;%PYTHONPATH%"
python -m odr_worker.update_system %*
