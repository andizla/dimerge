@echo off
rem Builds dimerge as dinput8.dll (x64, static CRT). Run from any shell; needs Visual Studio 2022 Community.
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 || (echo vcvars64 failed & exit /b 1)
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /EHsc /W4 /std:c++17 /utf-8 /MT /LD /Fo:out\ dimerge_main.cpp dimerge_config.cpp dimerge_device.cpp /Fe:out\dinput8.dll /link /DEF:dinput8.def /IMPLIB:out\dinput8_proxy.lib user32.lib ole32.lib
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
echo BUILD OK: out\dinput8.dll
