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
// server url cannot have trailing slash
#define SERVER_DOMAIN "localhost"
#define SERVER_PORT 8090
#define IS_SERVER_SECURE 0
#else
#define SERVER_DOMAIN "rpgpt.duckdns.org"
#define SERVER_PORT 443
#define IS_SERVER_SECURE 1
#endif

// REFACTORING:: also have to update in javascript!!!!!!!!
#define MAX_SENTENCE_LENGTH 400 // LOOOK AT AGBOVE COMMENT GBEFORE CHANGING
#define SENTENCE_CONST(txt) { .data = txt, .cur_index = sizeof(txt) }
#define SENTENCE_CONST_CAST(txt) (Sentence)SENTENCE_CONST(txt)

#define REMEMBERED_MEMORIES 32

#define MAX_AFTERIMAGES 6
#define TIME_TO_GEN_AFTERIMAGE (0.09f)
#define AFTERIMAGE_LIFETIME (0.5f)

#define DAMAGE_SWORD 0.05f
#define DAMAGE_BULLET 0.2f

// A* tuning
#define MAX_ASTAR_NODES 512
#define TIME_BETWEEN_PATH_GENS (0.5f)

