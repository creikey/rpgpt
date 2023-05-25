@echo off

echo Running codegen...

echo To build the game right now you must also get access to the rpgpt_private_assets megasync folder from creikey, and put it in this repo under that exact folder name. Use megasync to have it automatically update with the master of the repo. Contact him for access
echo Unfortunately this does mean you're a little boned when you check out to an old build that has different assets. We need to do something about this.

rmdir /S /q assets\copyrighted
mkdir assets\copyrighted
@echo on
copy "rpgpt_private_assets\EPIC RPG World Pack - Ancient Ruins V 1.8.1\Characters\NPC merchant\NPC Merchant-idle.png" "assets\copyrighted\merchant.png" || goto :error
copy "rpgpt_private_assets\EPIC RPG World Pack - Ancient Ruins V 1.8.1\Tilesets\wall-1 - 3 tiles tall.png" "assets\copyrighted\wall-1 - 3 tiles tall.png" || goto :error
copy "rpgpt_private_assets\EPIC RPG World Pack - Ancient Ruins V 1.8.1\Tilesets\Tileset-Animated Terrains-16 frames.png" "assets\copyrighted\animated_terrain.png" || goto :error
copy "rpgpt_private_assets\EPIC RPG World Pack - Ancient Ruins V 1.8.1\TiledMap Editor NEW\Tilesets\Ancient Ruins-Animated Terrains-16 frames.tsx" "assets\copyrighted\ruins_animated.tsx" || goto :error
copy "rpgpt_private_assets\EPIC RPG World Pack - Ancient Ruins V 1.8.1\TiledMap Editor NEW\Tilesets\Terrain - Ancient Ruins.tsx" "assets\copyrighted\ruins_ancient.tsx" || goto :error
copy "rpgpt_private_assets\EPIC RPG World Pack - Ancient Ruins V 1.8.1\Tilesets\Tileset-Terrain.png" "assets\copyrighted\ruins_ancient.png" || goto :error
copy "rpgpt_private_assets\EPIC RPG World Pack - Ancient Ruins V 1.8.1\Props\Atlas-Props.png" "assets\copyrighted\props.png" || goto :error
copy "rpgpt_private_assets\EPIC RPG World Pack - Ancient Ruins V 1.8.1\Characters\Moose\moose1-all animations-347x192.png" "assets\copyrighted\moose.png" || goto :error
copy "rpgpt_private_assets\Undead - Pixel Art Characters\Sprites\Wraith_Red.png" "assets\copyrighted\wraith.png" || goto :error
copy "rpgpt_private_assets\Undead - Pixel Art Characters\Sprites\Skeleton_Blue.png" "assets\copyrighted\skeleton.png" || goto :error
copy "rpgpt_private_assets\props_modified.png" "assets\copyrighted\Props.png" || goto :error
copy "rpgpt_private_assets\ForgottenMemories\TileSet.png" "assets\copyrighted\TileSet.png" || goto :error
copy "rpgpt_private_assets\ForgottenMemories\Trees.png" "assets\copyrighted\Trees.png" || goto :error
copy "rpgpt_private_assets\ForgottenMemories\WaterTiles-6frames.png" "assets\copyrighted\WaterTiles-6frames.png" || goto :error

copy "rpgpt_private_assets\knight_idle.png" "assets\copyrighted\knight_idle.png" || goto :error
copy "rpgpt_private_assets\knight_attack.png" "assets\copyrighted\knight_attack.png" || goto :error
copy "rpgpt_private_assets\knight_run_start.png" "assets\copyrighted\knight_run_start.png" || goto :error
@echo off

rmdir /S /q gen
mkdir gen

@REM shaders
thirdparty\sokol-shdc.exe --input quad.glsl --output gen\quad-sapp.glsl.h --slang glsl100:hlsl5:metal_macos || goto :error

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
