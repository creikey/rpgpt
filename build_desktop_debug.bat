@echo off

@REM https://learn.microsoft.com/en-us/cpp/build/reference/compiler-options-listed-by-category?view=msvc-170

setlocal enableDelayedExpansion

set "should_do_blender_export=0"
set "should_do_codegen=0"
for %%A in (%*) do (
    if "%%~A"=="blender_export" ( set "should_do_blender_export=1" )
    if "%%~A"=="codegen" ( set "should_do_codegen=1" )
)

if "%should_do_blender_export%"=="1" ( call blender_export.bat || goto :error )
if "%should_do_codegen%"=="1" ( call run_codegen.bat || goto :error )

@REM start /B zig cc -DDEVTOOLS -Igen -Ithirdparty -lDbghelp -lGdi32 -lD3D11 -lOle32 -lwinhttp -gfull -gcodeview -o main_zig.exe main.c
cl /diagnostics:caret /DDEVTOOLS /Igen /Ithirdparty /Wall /FC /Zi /WX Dbghelp.lib winhttp.lib main.c || goto :error

goto :EOF

:error
echo Failed to build
exit /B %ERRORLEVEL%
