@echo off
REM === Setup MSVC environment ===
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

REM === Rebuild the Scripts target ===
cmake --build ../ --target Scripts