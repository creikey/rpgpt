@echo off

rmdir /S /q build_web
mkdir build_web

call run_codegen.bat || goto :error

emcc -sEXPORTED_FUNCTIONS=_main,_end_text_input -sEXPORTED_RUNTIME_METHODS=ccall,cwrap -O0 -s ALLOW_MEMORY_GROWTH --source-map-base . -gsource-map -DDEVTOOLS -Ithirdparty -Igen main.c -o build_web\index.html --preload-file assets --shell-file web_template.html || goto :error

goto :EOF

:error
echo Failed to build
exit /B %ERRORLEVEL%
