@echo off

@REM https://learn.microsoft.com/en-us/cpp/build/reference/compiler-options-listed-by-category?view=msvc-170


if "%1" == "codegen" ( call run_codegen.bat || goto :error ) else ( echo NOT RUNNING CODEGEN )
@REM cl /diagnostics:caret /DDEVTOOLS /Igen /Ithirdparty /W3 /Zi /WX Dbghelp.lib main.c || goto :error
zig cc -DDEVTOOLS -Igen -Ithirdparty -lDbghelp -lGdi32 -lD3D11 -lOle32 -gfull -gcodeview -o main.exe main.c || goto :error
@REM cl /Igen /Ithirdparty /W3 /Zi /WX main.c || goto :error

goto :EOF

:error
echo Failed to build
exit /B %ERRORLEVEL%
