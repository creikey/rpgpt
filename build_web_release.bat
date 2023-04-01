@echo off

rmdir /S /q build_web_release
mkdir build_web_release

call run_codegen.bat || goto :error

@REM GO FUCK YOURSELF
set FLAGS=-s TOTAL_STACK=5242880 

copy marketing_page\favicon.ico build_web_release\favicon.ico

echo Building release
@echo on
emcc -sEXPORTED_FUNCTIONS=_main,_end_text_input,_stop_controlling_input,_start_controlling_input,_read_from_save_data,_dump_save_data,_in_dialog -sEXPORTED_RUNTIME_METHODS=ccall,cwrap -DNDEBUG -O2 -s ALLOW_MEMORY_GROWTH %FLAGS% -Ithirdparty -Igen main.c -o build_web_release\index.html --preload-file assets --shell-file web_template.html || goto :error
@echo off
echo Built

goto :EOF

:error
echo Failed to build
exit /B %ERRORLEVEL%


