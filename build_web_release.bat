@echo off

rmdir /S /q build_web_release
mkdir build_web_release

call run_codegen.bat || goto :error

echo Building release
emcc -DNDEBUG -O2 -DDEVTOOLS -s ALLOW_MEMORY_GROWTH -Ithirdparty -Igen main.c -o build_web_release\index.html --preload-file assets --shell-file web_template.html || goto :error

goto :EOF

:error
echo Failed to build
exit /B %ERRORLEVEL%


