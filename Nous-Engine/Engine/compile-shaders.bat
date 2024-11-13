@echo off

REM Create Library Directory for SPIR-V shader code
if not exist "%cd%\Library\Shaders\" mkdir "%cd%\Library\Shaders\"

echo "Compiling shaders..."

echo.

echo "Assets/Shaders/Nous.ObjectShader.vert.glsl --> Library/Shaders/Nous.ObjectShader.vert.spv"
%VULKAN_SDK%\Bin\glslc.exe -fshader-stage=vert Assets/Shaders/Nous.ObjectShader.vert.glsl -o Library/Shaders/Nous.ObjectShader.vert.spv
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit)

echo "Assets/Shaders/Nous.ObjectShader.frag.glsl --> Library/Shaders/Nous.ObjectShader.frag.spv"
%VULKAN_SDK%\Bin\glslc.exe -fshader-stage=frag Assets/Shaders/Nous.ObjectShader.frag.glsl -o Library/Shaders/Nous.ObjectShader.frag.spv
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit)

echo.

echo "Shaders Compilation Successful."

echo.

pause