@echo off

chcp 65001 >nul

set "projectDir=%~1"
if "%projectDir%"=="" set "projectDir=."

set "buildMode=%~2"
if /I "%buildMode%"=="" set "buildMode=debug"

cd /d "%projectDir%" || (
    echo [Error] Unable to access: %projectDir%
    pause
    exit /b 1
)

set "outDir=bin"
if not exist "%outDir%" mkdir "%outDir%" 

set "CFLAGS=-municode"
if /I "%buildMode%"=="debug" (
    set "CFLAGS=%CFLAGS% -g -O0"
) else if /I "%buildMode%"=="release" (
    set "CFLAGS=%CFLAGS% -O2 -s"
) else (
    echo [Error] Build mode not recognized: %buildMode%
    echo Use "debug" or "release"
    pause
    exit /b 1
)

echo === Compiling %buildMode% in: %cd% ===
call C:\msys64\ucrt64\bin\gcc.exe %CFLAGS% wimo.c csv.c window.c monitor.c config.c -o "%outDir%\wimo.exe" -lpsapi -lshlwapi

if %ERRORLEVEL% NEQ 0 (
    echo [Error] Build failed.
) else (
    echo ✅ Build successful.
)

