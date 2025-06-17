@echo off
REM Define versioned paths
set BASE_FOLDER=Build\Nous-Engine-v0.3
set ENGINE_FOLDER=%BASE_FOLDER%\Nous-Engine
set ZIP_FILE=Build\Nous-Engine-v0.3.zip

REM Create parent and nested folders
if not exist %ENGINE_FOLDER% (
    echo Creating engine folder: %ENGINE_FOLDER%...
    mkdir %ENGINE_FOLDER%
) else (
    echo Engine folder already exists.
)

REM Copy Assets
echo Copying Assets directory...
xcopy Engine\Assets %ENGINE_FOLDER%\Assets /E /I /Y

REM Copy only .dll files from Engine (no subdirectories)
echo Copying .dll files from Engine...
for %%F in (Engine\*.dll) do (
    copy "%%F" %ENGINE_FOLDER%\
)

REM Copy Nous-Engine.exe
if exist x64\Debug\Nous-Engine.exe (
    echo Copying Nous-Engine.exe...
    copy x64\Debug\Nous-Engine.exe %ENGINE_FOLDER%\
) else (
    echo Nous-Engine.exe not found, skipping...
)

REM Copy imgui.ini
if exist Engine\imgui.ini (
    echo Copying imgui.ini...
    copy Engine\imgui.ini %ENGINE_FOLDER%\
) else (
    echo imgui.ini not found, skipping...
)

REM Copy compile-shaders.bat
if exist Engine\compile-shaders.bat (
    echo Copying compile-shaders.bat...
    copy Engine\compile-shaders.bat %ENGINE_FOLDER%\
) else (
    echo compile-shaders.bat not found, skipping...
)

REM Copy README.md and LICENSE to root of versioned folder (not inside Nous-Engine)
if exist ..\README.md (
    echo Copying README.md to versioned root...
    copy ..\README.md %BASE_FOLDER%\ >nul
) else (
    echo README.md not found, skipping...
)

if exist ..\LICENSE (
    echo Copying LICENSE to versioned root...
    copy ..\LICENSE %BASE_FOLDER%\ >nul
) else (
    echo LICENSE not found, skipping...
)

REM Create the zip file
echo Creating zip file: %ZIP_FILE%...
powershell -Command ^
    "Compress-Archive -Path '%BASE_FOLDER%' -DestinationPath '%ZIP_FILE%' -Force"

echo Build process complete!
pause
