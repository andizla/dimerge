@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 || (echo vcvars failed & exit /b 1)
cd /d "%~dp0"
cl /nologo /O2 /EHsc /W3 /utf-8 dienum.cpp /Fe:dienum.exe /link dinput8.lib ole32.lib user32.lib
