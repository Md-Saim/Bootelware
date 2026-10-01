@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
if "%~1"=="" goto build_default
%*
goto :eof

:build_default
cmake -B build -A x64
cmake --build build --config Release
