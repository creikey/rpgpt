#pragma once

#define CURRENT_VERSION 12 // wehenver you change Entity increment this boz

#define LEVEL_TILES 150 // width and height of level tiles array
#define LAYERS 3
#define TILE_SIZE 32 // in pixels
#define PLAYER_SPEED 3.5f // in meters per second
#define PLAYER_ROLL_SPEED 7.0f
#define PERCEPTION_HEARING_RAGE (TILE_SIZE*4.0f)

#define ARENA_SIZE (1024*1024)

#ifdef DEVTOOLS
#define SERVER_URL "http://localhost:8090"
#else
#define SERVER_URL "https://rpgpt.duckdns.org"
#endif


