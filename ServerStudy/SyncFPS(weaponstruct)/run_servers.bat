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

start "Server0-Zone0" /D "!SERVER_CWD!" cmd /k ""!SERVER_EXE!" 0 9000 0"
start "Server1-Zone1" /D "!SERVER_CWD!" cmd /k ""!SERVER_EXE!" 1 9001 1"

echo [INFO] Launched both server processes.
pause
endlocal
