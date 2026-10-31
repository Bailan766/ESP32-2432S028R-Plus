@echo off
title CYD Plus Web Flasher

set PYTHON_CMD=python
where python >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    if exist "%USERPROFILE%\.platformio\penv\Scripts\python.exe" (
        set PYTHON_CMD="%USERPROFILE%\.platformio\penv\Scripts\python.exe"
    ) else (
        echo [ERROR] Python was not found in PATH or PlatformIO penv.
        echo Please ensure Python or PlatformIO is installed.
        pause
        exit /b 1
    )
)

echo ====================================================
echo   Starting CYD Plus Web Flasher Server...
echo ====================================================

%PYTHON_CMD% "%~dp0web_flasher\server.py" %*
if %ERRORLEVEL% NEQ 0 (
    pause
)
