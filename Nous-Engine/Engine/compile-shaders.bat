@echo off

REM Create Library Directory for SPIR-V shader code
if not exist "%cd%\Library\Shaders\" mkdir "%cd%\Library\Shaders\"

echo "Compiling shaders..."

echo "Assets/Shaders/BuiltIn.ObjectShader.vert.glsl --> Library/Shaders/BuiltIn.ObjectShader.vert.spv"
%VULKAN_SDK%\Bin\glslc.exe -fshader-stage=vert Assets/Shaders/BuiltIn.ObjectShader.vert.glsl -o Library/Shaders/BuiltIn.ObjectShader.vert.spv
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && pause)

echo "Assets/Shaders/BuiltIn.ObjectShader.frag.glsl --> Library/Shaders/BuiltIn.ObjectShader.frag.spv"
%VULKAN_SDK%\Bin\glslc.exe -fshader-stage=frag Assets/Shaders/BuiltIn.ObjectShader.frag.glsl -o Library/Shaders/BuiltIn.ObjectShader.frag.spv
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && pause)

echo "Shaders Compilation Successful."