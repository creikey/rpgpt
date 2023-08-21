#pragma once

#define LEVEL_TILES 150 // width and height of level tiles array
#define LAYERS 3
#define TILE_SIZE 0.5f // in pixels
#define PLAYER_SPEED 0.15f // in meters per second
#define PERCEPTION_HEARING_RAGE (TILE_SIZE*4.0f)
#define CHARACTERS_PER_SEC 45.0f
#define PROPAGATE_ACTIONS_RADIUS 10.0f
#define SWORD_SWIPE_RADIUS (TILE_SIZE*3.0f)
#define ARROW_SPEED 200.0f
#define SECONDS_PER_ARROW 1.3f
#define DIALOG_INTERACT_SIZE 5.0f // length of the centered AABB (not halfsize) around the player of who they're interacting with

#define CAM_DISTANCE 15.0f
#define CAM_VERTICAL_TO_HORIZONTAL_RATIO 0.95f
#define DIALOG_FADE_TIME 3.0f

#define BUBBLE_WIDTH_PIXELS 400.0f
#define BUBBLE_TEXT_WIDTH_PIXELS (BUBBLE_WIDTH_PIXELS*0.8f)
#define BUBBLE_TEXT_SCALE 1.0f
#define BUBBLE_FONT default_font
#define BUBBLE_LINES_PER_PAGE 2
#define AI_MAX_BUBBLE_PAGES_IN_OUTPUT 2

#define ARENA_SIZE (1024*1024*20)
#define BIG_ARENA_SIZE (ARENA_SIZE * 4)

#ifdef DEVTOOLS
// server url cannot have trailing slash
//#define MOCK_AI_RESPONSE
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

#define MAXIMUM_THREEDEE_THINGS 1024
#define ANIMATION_BLEND_TIME 0.15f

#define REMEMBERED_MEMORIES 64
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
#define FIELD_OF_VIEW (PI32/4.0f)
#define NEAR_PLANE_DISTANCE (0.01f)
#define FAR_PLANE_DISTANCE (45.0f)
#define SHADOW_MAP_DIMENSION (2048)
