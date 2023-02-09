@echo off
thirdparty\sokol-shdc.exe --input quad.glsl --output quad-sapp.glsl.h --slang glsl330:hlsl5:metal_macos || goto :error

goto :EOF

:error
exit /B %ERRORLEVEL%
