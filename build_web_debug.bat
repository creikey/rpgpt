@echo off

rmdir /S /q build_web
mkdir build_web

call run_codegen.bat || goto :error

emcc -O2 -s ALLOW_MEMORY_GROWTH --source-map-base . -gsource-map -DDEVTOOLS -Ithirdparty -Igen main.c -o build_web\index.html --preload-file assets --shell-file web_template.html || goto :error

goto :EOF

:error
echo Failed to build
exit /B %ERRORLEVEL%
