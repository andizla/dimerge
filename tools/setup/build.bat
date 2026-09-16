@echo off
rem Builds dimerge-setup.exe (x64, static CRT). Needs Visual Studio 2022 Community.
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 || (echo vcvars64 failed & exit /b 1)
cd /d "%~dp0"
cl /nologo /O2 /EHsc /W4 /std:c++17 /utf-8 /MT dimerge-setup.cpp pak_slots.cpp /Fe:dimerge-setup.exe /link dinput8.lib ole32.lib user32.lib advapi32.lib shlwapi.lib
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
echo BUILD OK: dimerge-setup.exe
