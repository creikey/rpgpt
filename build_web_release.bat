@echo off

pushd %~dp0%

rmdir /S /q build_web_release
mkdir build_web_release

call blender_export.bat || goto :error
call run_codegen.bat || goto :error

set FLAGS=-O0 -DNDEBUG
set OUTPUT_FOLDER=build_web_release

call build_web_common.bat || goto :error

goto :success

:error
echo Failed to build

:success
set "returncode=%ERRORLEVEL%"
popd
exit /B %returncode%
