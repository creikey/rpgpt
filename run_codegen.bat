@echo off

echo Asset packs which must be bought and unzipped into root directory before running this script:
echo https://rafaelmatos.itch.io/epic-rpg-world-pack-ancient-ruins
echo https://sventhole.itch.io/undead-pixel-art-characters

rmdir /S /q assets\copyrighted
mkdir assets\copyrighted
copy "EPIC RPG World Pack - Ancient Ruins V 1.7\EPIC RPG World Pack - Ancient Ruins V 1.7\Characters\NPC Merchant-idle.png" "assets\copyrighted\merchant.png" || goto :error
copy "EPIC RPG World Pack - Ancient Ruins V 1.7\EPIC RPG World Pack - Ancient Ruins V 1.7\Tilesets\wall-1 - 3 tiles tall.png" "assets\copyrighted\wall-1 - 3 tiles tall.png" || goto :error
copy "EPIC RPG World Pack - Ancient Ruins V 1.7\EPIC RPG World Pack - Ancient Ruins V 1.7\Tilesets\Tileset-Animated Terrains-16 frames.png" "assets\copyrighted\animated_terrain.png" || goto :error
copy "EPIC RPG World Pack - Ancient Ruins V 1.7\EPIC RPG World Pack - Ancient Ruins V 1.7\TiledMap Editor\Ancient Ruins-Animated Terrains-16 frames.tsx" "assets\copyrighted\ruins_animated.tsx" || goto :error
copy "EPIC RPG World Pack - Ancient Ruins V 1.7\EPIC RPG World Pack - Ancient Ruins V 1.7\TiledMap Editor\Terrain - Ancient Ruins.tsx" "assets\copyrighted\ruins_ancient.tsx" || goto :error
copy "EPIC RPG World Pack - Ancient Ruins V 1.7\EPIC RPG World Pack - Ancient Ruins V 1.7\Tilesets\Tileset-Terrain.png" "assets\copyrighted\ruins_ancient.png" || goto :error
copy "EPIC RPG World Pack - Ancient Ruins V 1.7\EPIC RPG World Pack - Ancient Ruins V 1.7\Props\Atlas-Props.png" "assets\copyrighted\props.png" || goto :error
copy "EPIC RPG World Pack - Ancient Ruins V 1.7\EPIC RPG World Pack - Ancient Ruins V 1.7\Characters\Moose\moose1-all animations-347x192.png" "assets\copyrighted\moose.png" || goto :error
copy "Undead - Pixel Art Characters\Undead - Pixel Art Characters\Sprites\Wraith_Red.png" "assets\copyrighted\wraith.png" || goto :error
copy "Undead - Pixel Art Characters\Undead - Pixel Art Characters\Sprites\Skeleton_Blue.png" "assets\copyrighted\skeleton.png" || goto :error

rmdir /S /q gen
mkdir gen

pushd server
python training_data_convert.py
popd

@REM shaders
thirdparty\sokol-shdc.exe --input quad.glsl --output gen\quad-sapp.glsl.h --slang glsl100:hlsl5:metal_macos || goto :error

@REM metadesk codegen
cl /Ithirdparty /W3 /Zi /WX codegen.c || goto :error
codegen || goto :error

goto :EOF

:error
echo Codegen failed
exit /B %ERRORLEVEL%
