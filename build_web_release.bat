@echo off

rmdir /S /q build_web_release
mkdir build_web_release

call run_codegen.bat || goto :error

set FLAGS=-O0 -DNDEBUG
set OUTPUT_FOLDER=build_web_release

call build_web_common.bat || goto :error

goto :EOF

:error
echo Failed to build
exit /B %ERRORLEVEL%
