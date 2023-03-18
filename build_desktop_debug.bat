@echo off

@REM https://learn.microsoft.com/en-us/cpp/build/reference/compiler-options-listed-by-category?view=msvc-170

remedybg.exe stop-debugging
call run_codegen.bat || goto :error
cl /diagnostics:caret /DDEVTOOLS /Igen /Ithirdparty /W3 /Zi /WX main.c || goto :error
@REM cl /Igen /Ithirdparty /W3 /Zi /WX main.c || goto :error
remedybg.exe start-debugging
goto :EOF

:error
echo Failed to build
exit /B %ERRORLEVEL%
