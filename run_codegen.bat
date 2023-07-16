@echo on

echo Running codegen...

if exist gen\ (
  echo "Codegen folder already exists, not deleting because that messes with vscode intellisense..."
) else (
  mkdir gen
)

@REM shaders
thirdparty\sokol-shdc.exe --input threedee.glsl --output gen\threedee.glsl.h --slang glsl100:hlsl5:metal_macos:glsl330 || goto :error

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
