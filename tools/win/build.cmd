@echo off
rem Usage: build.cmd <preset> [test]   (preset: debug | release)
setlocal
set "PRESET=%~1"
if "%PRESET%"=="" set "PRESET=debug"
cd /d "%~dp0..\.."
call "%~dp0devenv.cmd" || exit /b 1
if not exist "out\build\%PRESET%\build.ninja" (
  cmake --preset %PRESET% || exit /b 1
)
cmake --build --preset %PRESET% || exit /b 1
if /i "%~2"=="test" (
  ctest --preset %PRESET% || exit /b 1
)
exit /b 0
