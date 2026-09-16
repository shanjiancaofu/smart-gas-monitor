@echo off
rem Wrapper for run_host_tests.py on a developer shell: vcvars64 must be loaded
rem first or cl.exe fails with "fatal error C1034: stdio.h". Paths are derived
rem from this script's location so the checkout can live anywhere.
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d "%~dp0.."
python tools\run_host_tests.py --cc cl %*
