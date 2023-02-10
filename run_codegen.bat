@echo off
rmdir /S /q gen
mkdir gen

@REM shaders
thirdparty\sokol-shdc.exe --input quad.glsl --output gen\quad-sapp.glsl.h --slang glsl330:hlsl5:metal_macos || goto :error

@REM metadesk codegen
cl /Ithirdparty /W3 /Zi /WX codegen.c || goto :error
codegen || goto :error

goto :EOF

:error
exit /B %ERRORLEVEL%
