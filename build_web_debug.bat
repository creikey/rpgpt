@echo off

rmdir /S /q build_web
mkdir build_web

call run_codegen.bat || goto :error

@REM set FLAGS=-fsanitize=undefined -fsanitize=address
@REM GO FUCK YOURSELF
set FLAGS=-s TOTAL_STACK=5242880 

copy marketing_page\favicon.ico build_web\favicon.ico
copy main.c build_web\main.c || goto :error

@echo on
@REM emcc -sEXPORTED_FUNCTIONS=_main,_end_text_input,_stop_controlling_input,_start_controlling_input,_read_from_save_data,_dump_save_data,_in_dialog -sEXPORTED_RUNTIME_METHODS=ccall,cwrap -O0 -s ALLOW_MEMORY_GROWTH %FLAGS% --source-map-base http://localhost:8000/ -gsource-map -g3 -gdwarf -DDEVTOOLS -Ithirdparty -Igen main.c -o build_web\index.html --preload-file assets --shell-file web_template.html || goto :error
emcc -sEXPORTED_FUNCTIONS=_main,_end_text_input,_stop_controlling_input,_start_controlling_input,_read_from_save_data,_dump_save_data,_in_dialog -sEXPORTED_RUNTIME_METHODS=ccall,cwrap -O0 -s ALLOW_MEMORY_GROWTH %FLAGS% --source-map-base http://localhost:8000/ -g3 -gdwarf -DDEVTOOLS -Ithirdparty -Igen main.c -o build_web\index.html --preload-file assets --shell-file web_template.html || goto :error
@echo off

goto :EOF

:error
echo Failed to build
exit /B %ERRORLEVEL%
