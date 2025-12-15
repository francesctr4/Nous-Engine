@echo off
setlocal EnableDelayedExpansion

REM ============================================================
REM Nous Engine - RebuildScripts.bat (SDK-only, no engine source)
REM Requirements:
REM   <bin>\Scripts\SDK\include\Engine\...\IScript.inl
REM   <bin>\Scripts\SDK\src\EngineAPI.cpp
REM   <bin>\Assets\Scripts\*.cpp
REM Output:
REM   <bin>\Scripts\Scripts.dll
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
if errorlevel 1 (
    echo [ERROR] Failed to initialize MSVC environment
    exit /b 1
)

REM -----------------------------
REM 2) Resolve bin dirs
REM -----------------------------
set SCRIPT_DIR=%~dp0
set BIN_DIR=%SCRIPT_DIR%..\

set SCRIPTS_OUTPUT_DIR=%BIN_DIR%Scripts
set SCRIPTS_ASSETS_BIN_DIR=%BIN_DIR%Assets\Scripts

if not exist "%SCRIPTS_OUTPUT_DIR%" mkdir "%SCRIPTS_OUTPUT_DIR%" >nul 2>&1

set SCRIPTS_OBJ_DIR=%SCRIPTS_OUTPUT_DIR%\obj

if not exist "%SCRIPTS_OBJ_DIR%" mkdir "%SCRIPTS_OBJ_DIR%"

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
set BUILD_MODE=Debug
echo %BIN_DIR% | findstr /I "Debug" >nul && set BUILD_MODE=Debug

:MODE_OK
echo [INFO] BUILD_MODE           = %BUILD_MODE%

REM -----------------------------
REM 3) SDK paths (required)
REM -----------------------------
set SDK_DIR=%SCRIPTS_OUTPUT_DIR%\SDK
set SDK_INCLUDE=%SDK_DIR%\include
set SDK_ENGINE_API_CPP=%SDK_DIR%\src\EngineAPI.cpp

REM Validate SDK exists
if not exist "%SDK_INCLUDE%\Engine\Scripting\Internal\IScript.inl" (
    echo [ERROR] Script SDK headers missing.
    echo         Expected: "%SDK_INCLUDE%\Engine\Scripting\Internal\IScript.inl"
    exit /b 1
)

if not exist "%SDK_ENGINE_API_CPP%" (
    echo [ERROR] Script SDK EngineAPI.cpp missing.
    echo         Expected: "%SDK_ENGINE_API_CPP%"
    exit /b 1
)

REM Validate scripts exist
if not exist "%SCRIPTS_ASSETS_BIN_DIR%" (
    echo [ERROR] Scripts source folder missing:
    echo         "%SCRIPTS_ASSETS_BIN_DIR%"
    exit /b 1
)

REM -----------------------------
REM 4) Diagnostics
REM -----------------------------
echo ============================================================
echo [INFO] BIN_DIR              = %BIN_DIR%
echo [INFO] SCRIPTS_OUTPUT_DIR   = %SCRIPTS_OUTPUT_DIR%
echo [INFO] SCRIPTS_ASSETS_BIN_DIR= %SCRIPTS_ASSETS_BIN_DIR%
echo [INFO] SDK_INCLUDE          = %SDK_INCLUDE%
echo [INFO] SDK_ENGINE_API_CPP   = %SDK_ENGINE_API_CPP%
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

>>"%RSP%" echo /Fo"%SCRIPTS_OBJ_DIR%\\"

REM Config-specific flags
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

REM SDK includes + SDK EngineAPI.cpp (compiled into Scripts.dll)
>>"%RSP%" echo /I"%SDK_INCLUDE%"
>>"%RSP%" echo "%SDK_ENGINE_API_CPP%"

REM Output
>>"%RSP%" echo /Fe:"%SCRIPTS_OUTPUT_DIR%\Scripts.dll"

REM Script sources (recursive)
for /R "%SCRIPTS_ASSETS_BIN_DIR%" %%F in (*.cpp) do (
    >>"%RSP%" echo "%%F"
)

REM -----------------------------
REM 6) Build
REM -----------------------------
cl @"%RSP%"
if errorlevel 1 (
    echo [ERROR] Script compilation failed
    exit /b 1
)

echo [OK] Scripts compiled successfully: "%SCRIPTS_OUTPUT_DIR%\Scripts.dll"
exit /b 0
