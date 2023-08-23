@echo off

pushd %~dp0%

rmdir /S /q build_web
mkdir build_web

@REM set FLAGS=-fsanitize=undefined -fsanitize=address
set FLAGS=-O0 -g -DDEVTOOLS
set OUTPUT_FOLDER=build_web


call build_web_common.bat %* || goto :error

@echo off

goto :success

:error
echo Failed to build

:success
set "returncode=%ERRORLEVEL%"
popd
exit /B %returncode%
