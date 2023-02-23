@echo off

rmdir /S /q build_web
mkdir build_web

call run_codegen.bat || goto :error

emcc -s ALLOW_MEMORY_GROWTH -DDEVTOOLS -Ithirdparty -Igen main.c -o build_web\index.html --preload-file assets --shell-file web_template.html || goto :error

goto :EOF

:error
echo Failed to build
exit /B %ERRORLEVEL%
