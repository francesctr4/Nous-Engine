@echo off
setlocal EnableDelayedExpansion

REM ============================================================
REM Nous Engine - RebuildScripts.bat (mimic Scripts CMake target)
REM ============================================================

REM -----------------------------
REM 1) Setup MSVC environment
REM -----------------------------
set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
if not exist "%VSWHERE%" (
    echo [ERROR] vswhere.exe not found
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`
    "%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
`) do set VS_PATH=%%i

if "%VS_PATH%"=="" (
    echo [ERROR] Visual Studio C++ Build Tools not found
    exit /b 1
)

set VCVARS64=%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat
if not exist "%VCVARS64%" (
    echo [ERROR] vcvars64.bat not found
    exit /b 1
)

call "%VCVARS64%"
if errorlevel 1 exit /b 1

REM -----------------------------
REM 2) Resolve bin dirs
REM -----------------------------
set SCRIPT_DIR=%~dp0
set BIN_DIR=%SCRIPT_DIR%..\

set SCRIPTS_OUTPUT_DIR=%BIN_DIR%Scripts
set SCRIPTS_ASSETS_BIN_DIR=%BIN_DIR%Assets\Scripts

if not exist "%SCRIPTS_OUTPUT_DIR%" mkdir "%SCRIPTS_OUTPUT_DIR%" >nul 2>&1

REM -----------------------------
REM 2.5) Build configuration (auto-detect + override)
REM Usage:
REM   RebuildScripts.bat
REM   RebuildScripts.bat Debug
REM   RebuildScripts.bat Release
REM -----------------------------
set BUILD_MODE=%~1

if /I "%BUILD_MODE%"=="Debug" goto :MODE_OK
if /I "%BUILD_MODE%"=="Release" goto :MODE_OK

REM Auto-detect from path
set BUILD_MODE=Release
echo %BIN_DIR% | findstr /I "Debug" >nul && set BUILD_MODE=Debug

:MODE_OK
echo [INFO] BUILD_MODE           = %BUILD_MODE%

REM -----------------------------
REM 3) Find repo root (Source/)
REM -----------------------------
set SEARCH_DIR=%SCRIPT_DIR%
set ENGINE_ROOT=

for /L %%i in (1,1,12) do (
    if exist "!SEARCH_DIR!\Source\Engine\Scripting\EngineAPI\EngineAPI.cpp" (
        set ENGINE_ROOT=!SEARCH_DIR!
        goto :FOUND_ROOT
    )
    set SEARCH_DIR=!SEARCH_DIR!..\
)

:FOUND_ROOT
if "%ENGINE_ROOT%"=="" (
    echo [ERROR] Could not find Source\Engine
    exit /b 1
)

set ENGINE_SOURCE_DIR=%ENGINE_ROOT%Source
set ENGINE_API_CPP=%ENGINE_SOURCE_DIR%\Engine\Scripting\EngineAPI\EngineAPI.cpp

REM -----------------------------
REM 4) Diagnostics
REM -----------------------------
echo ============================================================
echo [INFO] BIN_DIR              = %BIN_DIR%
echo [INFO] ENGINE_SOURCE_DIR    = %ENGINE_SOURCE_DIR%
echo [INFO] ENGINE_API_CPP       = %ENGINE_API_CPP%
echo ============================================================

REM -----------------------------
REM 5) Generate response file
REM -----------------------------
set RSP=%SCRIPTS_OUTPUT_DIR%\scripts_build.rsp
break > "%RSP%"

>>"%RSP%" echo /nologo
>>"%RSP%" echo /std:c++latest
>>"%RSP%" echo /EHsc
>>"%RSP%" echo /LD
>>"%RSP%" echo /DSCRIPTS_EXPORTS

REM ----- Config-specific flags -----
if /I "%BUILD_MODE%"=="Debug" (
    >>"%RSP%" echo /MDd
    >>"%RSP%" echo /Od
    >>"%RSP%" echo /Zi
    >>"%RSP%" echo /D_DEBUG
) else (
    >>"%RSP%" echo /MD
    >>"%RSP%" echo /O2
    >>"%RSP%" echo /DNDEBUG
)

REM Includes
>>"%RSP%" echo /I"%ENGINE_SOURCE_DIR%"
>>"%RSP%" echo /I"%SCRIPTS_ASSETS_BIN_DIR%"

REM Output
>>"%RSP%" echo /Fe:"%SCRIPTS_OUTPUT_DIR%\Scripts.dll"

REM Sources
>>"%RSP%" echo "%ENGINE_API_CPP%"

if exist "%SCRIPTS_ASSETS_BIN_DIR%" (
    for /R "%SCRIPTS_ASSETS_BIN_DIR%" %%F in (*.cpp) do (
        >>"%RSP%" echo "%%F"
    )
)

REM -----------------------------
REM 6) Build
REM -----------------------------
cl @"%RSP%"
if errorlevel 1 (
    echo [ERROR] Script compilation failed
    exit /b 1
)

echo [OK] Scripts compiled successfully
exit /b 0