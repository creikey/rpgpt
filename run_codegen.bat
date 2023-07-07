@echo off

echo Running codegen...

rmdir /S /q assets\copyrighted

rmdir /S /q "assets\exported_3d"
mkdir "assets\exported_3d" || goto :error
copy "art\exported\*" "assets\exported_3d\" || goto :error
copy "art\gigatexture.png" "assets\exported_3d\gigatexture.png" || goto :error

rmdir /S /q gen
mkdir gen

@REM shaders
thirdparty\sokol-shdc.exe --input quad.glsl --output gen\quad-sapp.glsl.h --slang glsl100:hlsl5:metal_macos:glsl330 || goto :error
thirdparty\sokol-shdc.exe --input threedee.glsl --output gen\threedee.glsl.h --slang glsl100:hlsl5:metal_macos:glsl330 || goto :error
thirdparty\sokol-shdc.exe --input shadow_mapper.glsl --output gen\shadow_mapper.glsl.h --slang glsl100:hlsl5:metal_macos:glsl330 || goto :error

@REM metadesk codegen
cl /Ithirdparty /W3 /Zi /WX codegen.c || goto :error
@REM zig cc -Ithirdparty -gfull -gcodeview codegen.c -o codegen.exe || goto error
codegen || goto :error

@REM cl /Ithirdparty /Igen /W3 /Zi /WX maketraining.c || goto :error
@REM maketraining || goto :error

goto :EOF

:error
echo Codegen failed
exit /B %ERRORLEVEL%
