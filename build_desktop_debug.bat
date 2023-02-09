@echo off

@REM https://learn.microsoft.com/en-us/cpp/build/reference/compiler-options-listed-by-category?view=msvc-170

call shadergen.bat || goto :error
cl /Ithirdparty /W3 /WX main.c || goto :error
goto :EOF

:error
echo Failed to build
exit /B %ERRORLEVEL%
