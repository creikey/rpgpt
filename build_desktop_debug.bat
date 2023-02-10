@echo off

@REM https://learn.microsoft.com/en-us/cpp/build/reference/compiler-options-listed-by-category?view=msvc-170

call run_codegen.bat || goto :error
cl /DDEVTOOLS /Igen /Ithirdparty /W3 /Zi /WX main.c || goto :error
goto :EOF

:error
echo Failed to build
exit /B %ERRORLEVEL%
