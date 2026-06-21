@echo off
setlocal EnableDelayedExpansion

set "ROOT=%~dp0"
set "SERVER_CWD=%ROOT%SyncFPSServer\SyncFPSServer"
set "SERVER_EXE=%ROOT%SyncFPSServer\x64\Release\SyncFPSServer.exe"

if not exist "!SERVER_EXE!" (
    set "SERVER_EXE=%ROOT%SyncFPSServer\x64\Debug\SyncFPSServer.exe"
)

echo SERVER_CWD=!SERVER_CWD!
echo SERVER_EXE=!SERVER_EXE!
if not exist "!SERVER_CWD!" (
    echo [ERROR] Server working directory not found:
    echo !SERVER_CWD!
    pause
    exit /b 1
)
if not exist "!SERVER_EXE!" (
    echo [ERROR] Server exe not found:
    echo !SERVER_EXE!
    echo Build SyncFPSServer ^(Release x64 or Debug x64^) first.
    pause
    exit /b 1
)

start /affinity FFFF "Server1-Zone73" /D "!SERVER_CWD!" cmd /k ""!SERVER_EXE!" 73 9073 73"
start /affinity FFFF "Server1-Zone74" /D "!SERVER_CWD!" cmd /k ""!SERVER_EXE!" 74 9074 74"
start /affinity FFFF "Server1-Zone83" /D "!SERVER_CWD!" cmd /k ""!SERVER_EXE!" 83 9083 83"
start /affinity FFFF "Server1-Zone84" /D "!SERVER_CWD!" cmd /k ""!SERVER_EXE!" 84 9084 84"

echo [INFO] Launched both server processes.
endlocal
