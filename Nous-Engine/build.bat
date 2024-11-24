@echo off
REM Define versioned folder name
set VERSIONED_FOLDER=Build\Nous-Engine-v0.1
set ZIP_FILE=Build\Nous-Engine-v0.1.zip
set SHORTCUT_FILE=Build\Nous-Engine.exe

REM Check if the versioned folder exists
if not exist %VERSIONED_FOLDER% (
    echo Creating versioned folder: %VERSIONED_FOLDER%...
    mkdir %VERSIONED_FOLDER%
) else (
    echo Versioned folder already exists.
)

REM Copy the entire Library directory
echo Copying Library directory to %VERSIONED_FOLDER%...
xcopy Engine\Library %VERSIONED_FOLDER%\Library /E /I /Y

REM Copy only .dll files from Engine directory (not subdirectories)
echo Copying .dll files from Engine to %VERSIONED_FOLDER%...
for %%F in (Engine\*.dll) do (
    copy "%%F" %VERSIONED_FOLDER%\
)

REM Copy Nous-Engine.exe if it exists
if exist x64\Release\Nous-Engine.exe (
    echo Copying Nous-Engine.exe to %VERSIONED_FOLDER%...
    copy x64\Release\Nous-Engine.exe %VERSIONED_FOLDER%\
) else (
    echo Nous-Engine.exe not found, skipping...
)

REM Create a zip file of the Nous-Engine-v0.1 folder
echo Creating zip file: %ZIP_FILE%...
powershell -Command ^
    "Compress-Archive -Path '%VERSIONED_FOLDER%' -DestinationPath '%ZIP_FILE%' -Force"

echo Build process complete!
pause