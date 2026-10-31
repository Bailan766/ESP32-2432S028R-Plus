@echo off
chcp 65001 >nul
title CYD Plus 网页一键刷机

set PYTHON_CMD=python
where python >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    if exist "%USERPROFILE%\.platformio\penv\Scripts\python.exe" (
        set PYTHON_CMD="%USERPROFILE%\.platformio\penv\Scripts\python.exe"
    ) else (
        echo [错误] 系统中未找到 Python，请先安装 Python 或 PlatformIO 环境。
        pause
        exit /b 1
    )
)

echo ====================================================
echo   正在启动 CYD Plus 摆烂播放器 网页一键刷机工具...
echo ====================================================

%PYTHON_CMD% "%~dp0web_flasher\server.py"
