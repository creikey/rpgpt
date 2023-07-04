#pragma once

#define LEVEL_TILES 150 // width and height of level tiles array
#define LAYERS 3
#define TILE_SIZE 0.5f // in pixels
#define PLAYER_SPEED 0.2f // in meters per second
#define PLAYER_ROLL_SPEED 7.0f
#define PERCEPTION_HEARING_RAGE (TILE_SIZE*4.0f)
#define CHARACTERS_PER_SEC 45.0f
#define PROPAGATE_ACTIONS_RADIUS (TILE_SIZE*4.0f)
#define SWORD_SWIPE_RADIUS (TILE_SIZE*3.0f)
#define ARROW_SPEED 200.0f
#define SECONDS_PER_ARROW 1.3f

#define ARENA_SIZE (1024*1024)
#define BIG_ARENA_SIZE (ARENA_SIZE * 8)

#ifdef DEVTOOLS
// server url cannot have trailing slash
#define MOCK_AI_RESPONSE
#define SERVER_DOMAIN "localhost"
#define SERVER_PORT 8090
#define IS_SERVER_SECURE 0
#else
#define SERVER_DOMAIN "rpgpt.duckdns.org"
#define SERVER_PORT 443
#define IS_SERVER_SECURE 1
#endif

// this can never go down or else the forward compatibility of serialization breaks.
#define MAX_ENTITIES 256

// REFACTORING:: also have to update in javascript!!!!!!!!
#define MAX_SENTENCE_LENGTH 800 // LOOOK AT AGBOVE COMMENT GBEFORE CHANGING
#define SENTENCE_CONST(txt) { .data = txt, .cur_index = sizeof(txt) }
#define SENTENCE_CONST_CAST(txt) (Sentence)SENTENCE_CONST(txt)


#define REMEMBERED_MEMORIES 32
#define REMEMBERED_ERRORS 6

#define MAX_AFTERIMAGES 6
#define TIME_TO_GEN_AFTERIMAGE (0.09f)
#define AFTERIMAGE_LIFETIME (0.5f)

#define DAMAGE_SWORD 0.05f
#define DAMAGE_BULLET 0.2f

// A* tuning
#define MAX_ASTAR_NODES 512
#define TIME_BETWEEN_PATH_GENS (0.5f)



//Rendering
#define SHADOW_MAP_DIMENSION (2048)