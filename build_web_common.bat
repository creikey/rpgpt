setlocal enableDelayedExpansion
set "do_blender_export=0"
set "do_codegen=0"
for %%A in (%*) do (
    if "%%~A"=="blender_export" ( set "do_blender_export=1" )
    if "%%~A"=="codegen"        ( set "do_codegen=1" )
)
if "%do_blender_export%"=="1" ( call blender_export.bat || goto :error )
if "%do_codegen%"=="1"        ( call run_codegen.bat || goto :error )

copy marketing_page\favicon.ico %OUTPUT_FOLDER%\favicon.ico
@REM copy main.c %OUTPUT_FOLDER%\main.c || goto :error

@echo off
emcc ^
 -sEXPORTED_FUNCTIONS=_main,_end_text_input,_stop_controlling_input,_start_controlling_input,_read_from_save_data,_dump_save_data,_is_receiving_text_input^
 -sEXPORTED_RUNTIME_METHODS=ccall,cwrap^
 -s INITIAL_MEMORY=62914560^
 -s ALLOW_MEMORY_GROWTH -s TOTAL_STACK=15728640^
 %FLAGS%^
 -Ithirdparty -Igen main.c -o %OUTPUT_FOLDER%\index.html --preload-file assets --shell-file web_template.html || goto :error
@echo off

goto :EOF

:error
echo Failed to build
exit /B %ERRORLEVEL%
