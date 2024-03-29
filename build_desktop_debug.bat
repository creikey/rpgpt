@echo off

@REM https://learn.microsoft.com/en-us/cpp/build/reference/compiler-options-listed-by-category?view=msvc-170

setlocal enableDelayedExpansion
set "do_blender_export=0"
set "do_codegen=0"
for %%A in (%*) do (
    if "%%~A"=="blender_export" ( set "do_blender_export=1" )
    if "%%~A"=="codegen"        ( set "do_codegen=1" )
)
if "%do_blender_export%"=="1" ( call blender_export.bat || goto :error )
if "%do_codegen%"=="1"        ( call run_codegen.bat || goto :error )

@REM start /B zig cc -DDEVTOOLS -Igen -Ithirdparty -lDbghelp -lGdi32 -lD3D11 -lOle32 -lwinhttp -gfull -gcodeview -o main_zig.exe main.c
cl /nologo /diagnostics:caret /DDEVTOOLS /Igen /Ithirdparty /Wall /FC /Zi /WX main.c /link /noimplib /noexp || goto :error

goto :EOF

:error
echo Failed to build
exit /B %ERRORLEVEL%
