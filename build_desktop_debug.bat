@echo off

@REM https://learn.microsoft.com/en-us/cpp/build/reference/compiler-options-listed-by-category?view=msvc-170


FOR /F "tokens=*" %%g IN ('rg -g *.c -g !thirdparty break') do (SET VAR=%%g)

echo %g%

remedybg.exe stop-debugging
if "%1" == "codegen" ( call run_codegen.bat || goto :error )
cl /diagnostics:caret /DDEVTOOLS /Igen /Ithirdparty /W3 /Zi /WX main.c || goto :error
@REM cl /Igen /Ithirdparty /W3 /Zi /WX main.c || goto :error
remedybg.exe start-debugging
goto :EOF

:error
echo Failed to build
exit /B %ERRORLEVEL%
