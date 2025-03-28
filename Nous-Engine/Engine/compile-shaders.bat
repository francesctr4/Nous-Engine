@echo off

REM Create Library Directory for SPIR-V shader code
if not exist "%cd%\Library\Shaders\" mkdir "%cd%\Library\Shaders\"

echo "Compiling shaders..."

echo "Assets/Shaders/BuiltIn.MaterialShader.vert.glsl --> Library/Shaders/BuiltIn.MaterialShader.vert.spv"
%VULKAN_SDK%\Bin\glslc.exe -fshader-stage=vert Assets/Shaders/BuiltIn.MaterialShader.vert.glsl -o Library/Shaders/BuiltIn.MaterialShader.vert.spv
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && pause)

echo "Assets/Shaders/BuiltIn.MaterialShader.frag.glsl --> Library/Shaders/BuiltIn.MaterialShader.frag.spv"
%VULKAN_SDK%\Bin\glslc.exe -fshader-stage=frag Assets/Shaders/BuiltIn.MaterialShader.frag.glsl -o Library/Shaders/BuiltIn.MaterialShader.frag.spv
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && pause)


echo "Assets/Shaders/BuiltIn.UIShader.vert.glsl --> Library/Shaders/BuiltIn.UIShader.vert.spv"
%VULKAN_SDK%\Bin\glslc.exe -fshader-stage=vert Assets/Shaders/BuiltIn.UIShader.vert.glsl -o Library/Shaders/BuiltIn.UIShader.vert.spv
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && pause)

echo "Assets/Shaders/BuiltIn.UIShader.frag.glsl --> Library/Shaders/BuiltIn.UIShader.frag.spv"
%VULKAN_SDK%\Bin\glslc.exe -fshader-stage=frag Assets/Shaders/BuiltIn.UIShader.frag.glsl -o Library/Shaders/BuiltIn.UIShader.frag.spv
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && pause)

echo "Shaders Compilation Successful."