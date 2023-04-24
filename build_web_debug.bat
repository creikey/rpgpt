@echo off

rmdir /S /q build_web
mkdir build_web

@REM set FLAGS=-fsanitize=undefined -fsanitize=address
set FLAGS=-O0 --source-map-base http://localhost:8000/ -g3 -gdwarf -DDEVTOOLS
set OUTPUT_FOLDER=build_web

call build_web_common.bat || goto :error

goto :EOF

:error
echo Failed to build
exit /B %ERRORLEVEL%
