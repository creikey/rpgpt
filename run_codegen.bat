@echo off

echo Asset packs which must be bought and unzipped into root directory before running this script:
echo https://rafaelmatos.itch.io/epic-rpg-world-pack-ancient-ruins

rmdir /S /q assets\copyrighted
mkdir assets\copyrighted
copy "EPIC RPG World Pack - Ancient Ruins V 1.7\EPIC RPG World Pack - Ancient Ruins V 1.7\Characters\NPC Merchant-idle.png" "assets\copyrighted\merchant.png" || goto :error
copy "EPIC RPG World Pack - Ancient Ruins V 1.7\EPIC RPG World Pack - Ancient Ruins V 1.7\Tilesets\wall-1 - 3 tiles tall.png" "assets\copyrighted\wall-1 - 3 tiles tall.png" || goto :error

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
