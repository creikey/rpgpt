@echo off

rmdir /S /q build_web
mkdir build_web

@REM set FLAGS=-fsanitize=undefined -fsanitize=address
set FLAGS=-O0 -g -DDEVTOOLS
set OUTPUT_FOLDER=build_web


if "%1" == "NO_VALIDATION" (
    echo Disabling graphics validation...
    set FLAGS=%FLAGS% -DNDEBUG
)

call build_web_common.bat || goto :error

@echo off

if "%1" == "NO_VALIDATION" (
    echo Validation turned off
) else (
    echo If you want to turn graphics validation off to make web debug build faster, provide a command line argument called "NO_VALIDATION" to this build script
)

goto :EOF

:error
echo Failed to build
exit /B %ERRORLEVEL%
