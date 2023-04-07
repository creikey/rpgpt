// you will die someday
#define CURRENT_VERSION 7 // wehenver you change Entity increment this boz

#define SOKOL_IMPL
#if defined(WIN32) || defined(_WIN32)
#define DESKTOP
#define SOKOL_D3D11
#endif

#if defined(__EMSCRIPTEN__)
#define WEB
#define SOKOL_GLES2
#endif

#include "buff.h"
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "sokol_audio.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include "HandmadeMath.h"
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#pragma warning(disable : 4996) // fopen is safe. I don't care about fopen_s

#include <math.h>

#ifdef DEVTOOLS
#ifdef DESKTOP
#define PROFILING
#define PROFILING_IMPL
#endif
#endif
#include "profiling.h"

#define ENTITIES_ITER(ents) for(Entity *it = ents; it < ents + ARRLEN(ents); it++) if(it->exists)

double clamp(double d, double min, double max)
{
  const double t = d < min ? min : d;
  return t > max ? max : t;
}
float clampf(float d, float min, float max)
{
  const float t = d < min ? min : d;
  return t > max ? max : t;
}

float clamp01(float f)
{
 return clampf(f, 0.0f, 1.0f);
}

// so can be grep'd and removed
#define dbgprint(...) { printf("Debug | %s:%d | ", __FILE__, __LINE__); printf(__VA_ARGS__); }
Vec2 RotateV2(Vec2 v, float theta)
{
  return V2( 
      v.X * cosf(theta) - v.Y * sinf(theta),
      v.X * sinf(theta) + v.Y * cosf(theta)
    );
}

Vec2 ReflectV2(Vec2 v, Vec2 normal)
{
 assert(fabsf(LenV2(normal) - 1.0f) < 0.01f); // must be normalized
 Vec2 to_return = SubV2(v, MulV2F(normal, 2.0f * DotV2(v, normal)));

 assert(!isnan(to_return.x));
 assert(!isnan(to_return.y));
 return to_return;
}

typedef struct AABB
{
 Vec2 upper_left;
 Vec2 lower_right;
} AABB;

typedef struct Quad
{
 union
 {
  struct
  {
   Vec2 ul; // upper left
   Vec2 ur; // upper right
   Vec2 lr; // lower right
   Vec2 ll; // lower left
  };
  Vec2 points[4];
 };
} Quad;

typedef struct TileInstance
{
 uint16_t kind;
} TileInstance;

typedef struct AnimatedTile
{
 uint16_t id_from;
 bool exists;
 int num_frames;
 uint16_t frames[32];
} AnimatedTile;

typedef struct TileSet
{
 sg_image *img;
 uint16_t first_gid;
 AnimatedTile animated[128];
} TileSet;

typedef struct AnimatedSprite
{
 sg_image *img;
 double time_per_frame;
 int num_frames;
 Vec2 start;
 Vec2 offset;
 float horizontal_diff_btwn_frames;
 Vec2 region_size;
 bool no_wrap; // does not wrap when playing
} AnimatedSprite;

#ifdef DEVTOOLS
#define SERVER_URL "http://localhost:8090"
#else
#define SERVER_URL "https://rpgpt.duckdns.org"
#endif

#include "makeprompt.h"

Sentence from_str(char *s)
{
 Sentence to_return = {0};
 while(*s != '\0')
 {
  BUFF_APPEND(&to_return, *s);
  s++;
 }
 return to_return;
}


typedef struct Overlap
{
 bool is_tile; // in which case e will be null, naturally
 TileInstance t;
 Entity *e;
} Overlap;


typedef BUFF(Overlap, 16) Overlapping;

#define LEVEL_TILES 150
#define LAYERS 3
#define TILE_SIZE 32 // in pixels
#define MAX_ENTITIES 128
#define PLAYER_SPEED 3.5f // in meters per second
#define PLAYER_ROLL_SPEED 7.0f
typedef struct Level
{
 TileInstance tiles[LAYERS][LEVEL_TILES][LEVEL_TILES];
 Entity initial_entities[MAX_ENTITIES]; // shouldn't be directly modified, only used to initialize gs.entities on loading of level
} Level;

typedef struct TileCoord
{
 int x; // column
 int y; // row
} TileCoord;

// no alignment etc because lazy
typedef struct Arena
{
 char *data;
 size_t data_size;
 size_t cur;
} Arena;

Entity *player = NULL; // up here, used in text backend callback


typedef struct AudioSample
{
 float *pcm_data; // allocated by loader, must be freed
 uint64_t pcm_data_length;
} AudioSample;

typedef struct AudioPlayer
{
 AudioSample *sample; // if not 0, exists
 double volume; // ZII, 1.0 + this again
 double pitch; // zero initialized, the pitch used to play is 1.0 + this
 double cursor_time; // in seconds, current audio sample is cursor_time * sample_rate
} AudioPlayer;

AudioPlayer playing_audio[128] = {0};

#define SAMPLE_RATE 44100

AudioSample load_wav_audio(const char *path)
{
 unsigned int channels;
 unsigned int sampleRate;
 AudioSample to_return = {0};
 to_return.pcm_data = drwav_open_file_and_read_pcm_frames_f32(path, &channels, &sampleRate, &to_return.pcm_data_length, 0);
 assert(channels == 1);
 assert(sampleRate == SAMPLE_RATE);
 return to_return;
}

uint64_t cursor_pcm(AudioPlayer *p)
{
 return (uint64_t)(p->cursor_time * SAMPLE_RATE);
}
float float_rand( float min, float max )
{
 float scale = rand() / (float) RAND_MAX; /* [0, 1.0] */
 return min + scale * ( max - min );      /* [min, max] */
}
void play_audio(AudioSample *sample, float volume)
{
 AudioPlayer *to_use = 0;
 for(int i = 0; i < ARRLEN(playing_audio); i++)
 {
  if(playing_audio[i].sample == 0)
  {
   to_use = &playing_audio[i];
   break;
  }
 }
 assert(to_use);
 *to_use = (AudioPlayer){0};
 to_use->sample = sample;
 to_use->volume = volume;
 to_use->pitch = float_rand(0.9f, 1.1f);
}
// keydown needs to be referenced when begin text input,
// on web it disables event handling so the button up event isn't received
bool keydown[SAPP_KEYCODE_MENU] = {0};


bool in_dialog()
{
 return player->state == CHARACTER_TALKING;
}

#ifdef DESKTOP
bool receiving_text_input = false;
Sentence text_input_buffer = {0};
#else
#ifdef WEB
EMSCRIPTEN_KEEPALIVE
void stop_controlling_input()
{
 _sapp_emsc_unregister_eventhandlers(); // stop getting input, hand it off to text input
}

EMSCRIPTEN_KEEPALIVE
void start_controlling_input()
{
 memset(keydown, 0, ARRLEN(keydown));
 _sapp_emsc_register_eventhandlers();
}
#else
#error "No platform defined for text input!
#endif // web

#endif // desktop



Vec2 FloorV2(Vec2 v)
{
 return V2(floorf(v.x), floorf(v.y));
}

Arena make_arena(size_t max_size)
{
 return (Arena)
 {
  .data = calloc(max_size, 1),
  .data_size = max_size,
  .cur = 0,
 };
}
void reset(Arena *a)
{
 memset(a->data, 0, a->data_size);
 a->cur = 0;
}
char *get(Arena *a, size_t of_size)
{
 assert(a->data != NULL);
 char *to_return = a->data + a->cur;
 a->cur += of_size;
 assert(a->cur < a->data_size);
 return to_return;
}

Arena scratch = {0};

char *tprint(const char *format, ...)
{
 va_list argptr;
 va_start(argptr, format);

 int size = vsnprintf(NULL, 0, format, argptr) + 1; // for null terminator

 char *to_return = get(&scratch, size);

 vsnprintf(to_return, size, format, argptr);

  va_end(argptr);

  return to_return;
}

AABB entity_sword_aabb(Entity *e, float width, float height)
{
 if(e->facing_left)
 {
  return (AABB){
    .upper_left = AddV2(e->pos, V2(-width, height)),
    .lower_right = AddV2(e->pos, V2(0.0, -height)),
  };
 }
 else
 {
  return (AABB){
    .upper_left = AddV2(e->pos, V2(0.0, height)),
    .lower_right = AddV2(e->pos, V2(width, -height)),
  };
 }
}

// aabb advice by iRadEntertainment
Vec2 entity_aabb_size(Entity *e)
{
 if(e->is_character)
 {
  return V2(TILE_SIZE*0.9f, TILE_SIZE*0.5f);
 }
 else if(e->is_npc)
 {
  if(npc_is_knight_sprite(e))
  {
   return V2(TILE_SIZE*0.5f, TILE_SIZE*0.5f);
  }
  else if(e->npc_kind == NPC_GodRock)
  {
   return V2(TILE_SIZE*0.5f, TILE_SIZE*0.5f);
  }
  else if(e->npc_kind == NPC_OldMan)
  {
   return V2(TILE_SIZE*0.5f, TILE_SIZE*0.5f);
  }
  else if(e->npc_kind == NPC_Death)
  {
   return V2(TILE_SIZE*1.10f, TILE_SIZE*1.10f);
  }
  else if(e->npc_kind == NPC_Skeleton)
  {
   return V2(TILE_SIZE*1.0f, TILE_SIZE*1.0f);
  }
  else if(e->npc_kind == NPC_MOOSE)
  {
   return V2(TILE_SIZE*1.0f, TILE_SIZE*1.0f);
  }
  else if(e->npc_kind == NPC_Blocky)
  {
   return V2(TILE_SIZE*0.5f, TILE_SIZE*0.5f);
  }
  else
  {
   assert(false);
   return (Vec2){0};
  }
 }
 else if(e->is_bullet)
 {
  return V2(TILE_SIZE*0.25f, TILE_SIZE*0.25f);
 }
 else if(e->is_prop)
 {
  return V2(TILE_SIZE*0.5f, TILE_SIZE*0.5f);
 }
 else if(e->is_item)
 {
  return V2(TILE_SIZE*0.5f, TILE_SIZE*0.5f);
 }
 else
 {
  assert(false);
  return (Vec2){0};
 }
}

bool is_tile_solid(TileInstance t)
{
 uint16_t tile_id = t.kind;
 uint16_t collideable[] = {
  57 , 58 , 59 ,
  121, 122, 123,
  185, 186, 187,
  249, 250, 251,
  313, 314, 315,
  377, 378, 379,
 };
 for(int i = 0; i < ARRLEN(collideable); i++)
 {
  if(tile_id == collideable[i]+1) return true;
 }
 return false;
 //return tile_id == 53 || tile_id == 0 || tile_id == 367 || tile_id == 317 || tile_id == 313 || tile_id == 366 || tile_id == 368;
}
// tilecoord is integer tile position, not like tile coord
Vec2 tilecoord_to_world(TileCoord t)
{
 return V2( (float)t.x * (float)TILE_SIZE * 1.0f, -(float)t.y * (float)TILE_SIZE * 1.0f );
}

// points from tiled editor have their own strange and alien coordinate system (local to the tilemap Y+ down)
Vec2 tilepoint_to_world(Vec2 tilepoint)
{
 Vec2 tilecoord = MulV2F(tilepoint, 1.0/TILE_SIZE);
 return tilecoord_to_world((TileCoord){(int)tilecoord.X, (int)tilecoord.Y});
}

TileCoord world_to_tilecoord(Vec2 w)
{
 // world = V2(tilecoord.x * tile_size, -tilecoord.y * tile_size)
 // world.x = tilecoord.x * tile_size
 // world.x / tile_size = tilecoord.x
 // world.y = -tilecoord.y * tile_size
 // - world.y / tile_size = tilecoord.y
 return (TileCoord){ (int)floorf(w.X / TILE_SIZE), (int)floorf(-w.Y / TILE_SIZE) };
}


AABB tile_aabb(TileCoord t)
{
 return (AABB)
 {
  .upper_left = tilecoord_to_world(t),
   .lower_right = AddV2(tilecoord_to_world(t), V2(TILE_SIZE, -TILE_SIZE)),
 };
}

Vec2 rotate_counter_clockwise(Vec2 v)
{
 return V2(-v.Y, v.X);
}

Vec2 rotate_clockwise(Vec2 v)
{
 return V2(v.y, -v.x);
}


Vec2 aabb_center(AABB aabb)
{
 return MulV2F(AddV2(aabb.upper_left, aabb.lower_right), 0.5f);
}

AABB centered_aabb(Vec2 at, Vec2 size)
{
 return (AABB){
  .upper_left  = AddV2(at, V2(-size.X/2.0f, size.Y/2.0f)),
   .lower_right = AddV2(at, V2( size.X/2.0f, -size.Y/2.0f)),
 };
}

AABB entity_aabb(Entity *e)
{
 Vec2 at = e->pos;
/* following doesn't work because in move_and_slide I'm not using this function
 if(e->is_character) // aabb near feet
 {
  at = AddV2(at, V2(0.0f, -50.0f));
 }
*/
 return centered_aabb(at, entity_aabb_size(e));
}

TileInstance get_tile_layer(Level *l, int layer, TileCoord t)
{
 bool out_of_bounds = false;
 out_of_bounds |= t.x < 0;
 out_of_bounds |= t.x >= LEVEL_TILES;
 out_of_bounds |= t.y < 0;
 out_of_bounds |= t.y >= LEVEL_TILES;
 //assert(!out_of_bounds);
 if(out_of_bounds) return (TileInstance){0};
 return l->tiles[layer][t.y][t.x];
}

TileInstance get_tile(Level *l, TileCoord t)
{
 return get_tile_layer(l, 0, t);
}

sg_image load_image(const char *path)
{
 sg_image to_return = {0};

 int png_width, png_height, num_channels;
 const int desired_channels = 4;
 stbi_uc* pixels = stbi_load(
   path,
   &png_width, &png_height,
   &num_channels, 0);
 assert(pixels);
 Log("Pah %s | Loading image with dimensions %d %d\n", path, png_width, png_height);
 to_return = sg_make_image(&(sg_image_desc)
   {
   .width = png_width,
   .height = png_height,
   .pixel_format = SG_PIXELFORMAT_RGBA8,
   .min_filter = SG_FILTER_NEAREST,
   .num_mipmaps = 0,
   .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
   .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
   .mag_filter = SG_FILTER_NEAREST,
   .data.subimage[0][0] =
   {
   .ptr = pixels,
   .size = (size_t)(png_width * png_height * 4),
   }
   });
 stbi_image_free(pixels);
 return to_return;
}

#include "quad-sapp.glsl.h"
#include "assets.gen.c"

AABB level_aabb = { .upper_left = {0.0f, 0.0f}, .lower_right = {TILE_SIZE * LEVEL_TILES, -(TILE_SIZE * LEVEL_TILES)} };
typedef struct GameState {
 int version;
 Entity entities[MAX_ENTITIES];
} GameState;
GameState gs = {0};
double unprocessed_gameplay_time = 0.0;
#define MINIMUM_TIMESTEP (1.0/60.0)

EntityRef frome(Entity *e)
{
 EntityRef to_return = {
  .index = (int)(e - gs.entities),
  .generation = e->generation,
 };
 assert(to_return.index >= 0);
 assert(to_return.index < ARRLEN(gs.entities));
 return to_return;
}

Entity *gete(EntityRef ref)
{
 if(ref.generation == 0) return 0;
 Entity *to_return = &gs.entities[ref.index];
 if(!to_return->exists || to_return->generation != ref.generation)
 {
  return 0;
 }
 else
 {
  return to_return;
 }
}

bool eq(EntityRef ref1, EntityRef ref2)
{
 return ref1.index == ref2.index && ref1.generation == ref2.generation;
}

Entity *new_entity()
{
 for(int i = 0; i < ARRLEN(gs.entities); i++)
 {
  if(!gs.entities[i].exists)
  {
   Entity *to_return = &gs.entities[i];
   int gen = to_return->generation;
   *to_return = (Entity){0};
   to_return->exists = true;
   to_return->generation = gen + 1;
   return to_return;
  }
 }
 assert(false);
 return NULL;
}

void update_player_from_entities()
{
 player = 0;
 ENTITIES_ITER(gs.entities)
 {
  if(it->is_character)
  {
   assert(player == 0);
   player = it;
  }
 }
 assert(player != 0);
}

void reset_level()
{
 // load level
 Level *to_load = &level_level0;
 {
  assert(ARRLEN(to_load->initial_entities) == ARRLEN(gs.entities));
  memcpy(gs.entities, to_load->initial_entities, sizeof(Entity) * MAX_ENTITIES);
  gs.version = CURRENT_VERSION;
  ENTITIES_ITER(gs.entities)
  {
   if(it->generation == 0) it->generation = 1; // zero value generation means doesn't exist
  }
 }
 update_player_from_entities();
}


#ifdef WEB
EMSCRIPTEN_KEEPALIVE
void dump_save_data()
{
 EM_ASM({
   save_game_data = new Int8Array(Module.HEAP8.buffer, $0, $1);
 }, (char*)(&gs), sizeof(gs));
}
EMSCRIPTEN_KEEPALIVE
void read_from_save_data(char *data, size_t length)
{
 GameState read_data = {0};
 memcpy((char*)(&read_data), data, length);
 if(read_data.version != CURRENT_VERSION)
 {
  Log("Bad gamestate, has version %d expected version %d\n", read_data.version, CURRENT_VERSION);
 }
 else
 {
  gs = read_data;
  update_player_from_entities();
 }
}
#endif

// a callback, when 'text backend' has finished making text. End dialog
void end_text_input(char *what_player_said)
{
 // avoid double ending text input
 if(player->state != CHARACTER_TALKING)
 {
  return;
 }
 player->state = CHARACTER_IDLE;

 size_t player_said_len = strlen(what_player_said);
 int actual_len = 0;
 for(int i = 0; i < player_said_len; i++) if(what_player_said[i] != '\n') actual_len++;
 if(actual_len == 0) 
 {
  // this just means cancel the dialog
 }
 else
 {
  Sentence what_player_said_sentence = {0};
  assert(player_said_len < ARRLEN(what_player_said_sentence.data));
  for(int i = 0; i < player_said_len; i++)
  {
   char c = what_player_said[i];
   if(c == '\n') break;
   BUFF_APPEND(&what_player_said_sentence, c);
  }

  Entity *talking = gete(player->talking_to);
  assert(talking);
  ItemKind player_holding = ITEM_none;
  if(gete(player->holding_item) != 0)
  {
   player_holding = gete(player->holding_item)->item_kind;
  }
  if(talking->last_seen_holding_kind != player_holding)
  {
   process_perception(talking, (Perception){.type = PlayerHeldItemChanged, .holding = player_holding,});
  }
  process_perception(talking, (Perception){.type = PlayerDialog, .player_dialog = what_player_said_sentence,});
 }
}

AnimatedSprite knight_idle =
{
 .img = &image_new_knight_idle,
 .time_per_frame = 0.4,
 .num_frames = 6,
 .start = {0.0f, 0.0f},
 .horizontal_diff_btwn_frames = 64.0,
 .region_size = {64.0f, 64.0f},
};

AnimatedSprite knight_running =
{
 .img = &image_new_knight_run,
 .time_per_frame = 0.1,
 .num_frames = 7,
 .start = {64.0f*10, 0.0f},
 .horizontal_diff_btwn_frames = 64.0,
 .region_size = {64.0f, 64.0f},
};

AnimatedSprite knight_rolling =
{
 .img = &image_knight_roll,
 .time_per_frame = 0.04,
 .num_frames = 12,
 .start = {19.0f, 0.0f},
 .horizontal_diff_btwn_frames = 120.0,
 .region_size = {80.0f, 80.0f},
 .no_wrap = true,
};


AnimatedSprite knight_attack = 
{
 .img = &image_new_knight_attack,
 .time_per_frame = 0.06,
 .num_frames = 7,
 .start = {0.0f, 0.0f},
 .horizontal_diff_btwn_frames = 64.0,
 .region_size = {64.0f, 64.0f},
 .no_wrap = true,
};

AnimatedSprite old_man_idle = 
{
 .img = &image_old_man,
 .time_per_frame = 0.4,
 .num_frames = 4,
 .start = {0.0, 0.0},
 .horizontal_diff_btwn_frames = 16.0f,
 .region_size = {16.0f, 16.0f},
};
AnimatedSprite death_idle = 
{
 .img = &image_death,
 .time_per_frame = 0.15,
 .num_frames = 10,
 .start = {0.0, 0.0},
 .horizontal_diff_btwn_frames = 100.0f,
 .region_size = {100.0f, 100.0f},
};
AnimatedSprite skeleton_idle =
{
 .img = &image_skeleton,
 .time_per_frame = 0.15,
 .num_frames = 6,
 .start = {0.0f, 0.0f},
 .horizontal_diff_btwn_frames = 80.0,
 .offset = {0.0f, 20.0f},
 .region_size = {80.0f, 80.0f},
};
AnimatedSprite skeleton_swing_sword =
{
 .img = &image_skeleton,
 .time_per_frame = 0.10,
 .num_frames = 6,
 .start = {0.0f, 240.0f},
 .horizontal_diff_btwn_frames = 80.0,
 .offset = {0.0f, 20.0f},
 .region_size = {80.0f, 80.0f},
 .no_wrap = true,
};
AnimatedSprite skeleton_run =
{
 .img = &image_skeleton,
 .time_per_frame = 0.07,
 .num_frames = 8,
 .start = {0.0f, 160.0f},
 .horizontal_diff_btwn_frames = 80.0,
 .offset = {0.0f, 20.0f},
 .region_size = {80.0f, 80.0f},
};
AnimatedSprite skeleton_die =
{
 .img = &image_skeleton,
 .time_per_frame = 0.10,
 .num_frames = 13,
 .start = {0.0f, 400.0f},
 .horizontal_diff_btwn_frames = 80.0,
 .offset = {0.0f, 20.0f},
 .region_size = {80.0f, 80.0f},
 .no_wrap = true,
};
AnimatedSprite merchant_idle = 
{
 .img = &image_merchant,
 .time_per_frame = 0.15,
 .num_frames = 8,
 .start = {0.0, 0.0},
 .horizontal_diff_btwn_frames = 110.0f,
 .region_size = {110.0f, 110.0f},
 .offset = {0.0f, -20.0f},
};
/*
AnimatedSprite moose_idle = 
{
 .img = &image_moose,
 .time_per_frame = 0.15,
 .num_frames = 8,
 .start = {0.0, 0.0},
 .horizontal_diff_btwn_frames = 347.0f,
 .region_size = {347.0f, 160.0f},
 .offset = {-1.5f, -10.0f},
};
*/


sg_image image_font = {0};
float font_line_advance = 0.0f;
const float font_size = 32.0;
stbtt_bakedchar cdata[96]; // ASCII 32..126 is 95 glyphs


static struct
{
 sg_pass_action pass_action;
 sg_pipeline pip;
 sg_bindings bind;
} state;



void audio_stream_callback(float *buffer, int num_frames, int num_channels)
{
 assert(num_channels == 1);
 const int num_samples = num_frames * num_channels;
 double time_to_play = (double)num_frames / (double)SAMPLE_RATE;
 double time_per_sample = 1.0 / (double)SAMPLE_RATE;
 for(int i = 0; i < num_samples; i++)
 {
  float output_frame = 0.0f;
  for(int audio_i = 0; audio_i < ARRLEN(playing_audio); audio_i++)
  {
   AudioPlayer *it = &playing_audio[audio_i];
   if(it->sample != 0)
   {
    if(cursor_pcm(it) >= it->sample->pcm_data_length)
    {
     it->sample = 0;
    }
    else
    {
     output_frame += it->sample->pcm_data[cursor_pcm(it)]*(float)(it->volume + 1.0);
     it->cursor_time += time_per_sample*(it->pitch + 1.0);
    }
   }
  }
  buffer[i] = output_frame;
 }
}


typedef BUFF(Entity*, 16) SwordToDamage;
SwordToDamage entity_sword_to_do_damage(Entity *from, Overlapping o)
{
 SwordToDamage to_return = {0};
 BUFF_ITER(Overlap, &o)
 {
  if(!it->is_tile && it->e != from)
  {
   bool done_damage = false;
   Entity *looking_for = it->e;
   BUFF_ITER(EntityRef, &from->done_damage_to_this_swing)
   {
    EntityRef ref = *it;
    Entity *it = gete(ref);
    if(it == looking_for) done_damage = true;
   }
   if(!done_damage)
   {
    if(!BUFF_HAS_SPACE(&from->done_damage_to_this_swing))
    {
     BUFF_REMOVE_FRONT(&from->done_damage_to_this_swing);
     Log("Too many things to do damage to...\n");
     assert(false);
    }
    BUFF_APPEND(&to_return, looking_for);
    BUFF_APPEND(&from->done_damage_to_this_swing, frome(looking_for));
   }
  }
 }
 return to_return;
}

typedef Vec4 Color;

#define WHITE (Color){1.0f, 1.0f, 1.0f, 1.0f}
#define BLACK (Color){0.0f, 0.0f, 0.0f, 1.0f}
#define RED   (Color){1.0f, 0.0f, 0.0f, 1.0f}
#define GREEN (Color){0.0f, 1.0f, 0.0f, 1.0f}

Color colhex(uint32_t hex)
{
 int r = (hex & 0xff0000) >> 16;
 int g = (hex & 0x00ff00) >> 8;
 int b = (hex & 0x0000ff) >> 0;

 return (Color){ (float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, 1.0f };
}

Color blendalpha(Color c, float alpha)
{
 Color to_return = c;
 to_return.a = alpha;
 return to_return;
}



void init(void)
{
#ifdef WEB
 EM_ASM({
   set_server_url(UTF8ToString($0));
 }, SERVER_URL);
#endif
 Log("Size of entity struct: %zu\n", sizeof(Entity));
 Log("Size of %d gs.entities: %zu kb\n", (int)ARRLEN(gs.entities), sizeof(gs.entities)/1024);
 sg_setup(&(sg_desc){
   .context = sapp_sgcontext(),
 });
 stm_setup();
 saudio_setup(&(saudio_desc){
   .stream_cb = audio_stream_callback,
  .logger.func = slog_func,
 });

 scratch = make_arena(1024 * 10);

 load_assets();
 reset_level();

#ifdef WEB
 EM_ASM({
   load_all();
 });
#endif

 // load font
 {
  FILE* fontFile = fopen("assets/orange kid.ttf", "rb");
  fseek(fontFile, 0, SEEK_END);
  size_t size = ftell(fontFile); /* how long is the file ? */
  fseek(fontFile, 0, SEEK_SET); /* reset */

  unsigned char *fontBuffer = malloc(size);

  fread(fontBuffer, size, 1, fontFile);
  fclose(fontFile);

  unsigned char *font_bitmap = calloc(1, 512*512);
  stbtt_BakeFontBitmap(fontBuffer, 0, font_size, font_bitmap, 512, 512, 32, 96, cdata);

  unsigned char *font_bitmap_rgba = malloc(4 * 512 * 512); // stack would be too big if allocated on stack (stack overflow)
  for(int i = 0; i < 512 * 512; i++)
  {
   font_bitmap_rgba[i*4 + 0] = 255;
   font_bitmap_rgba[i*4 + 1] = 255;
   font_bitmap_rgba[i*4 + 2] = 255;
   font_bitmap_rgba[i*4 + 3] = font_bitmap[i];
  }

  image_font = sg_make_image( &(sg_image_desc){
    .width = 512,
    .height = 512,
    .pixel_format = SG_PIXELFORMAT_RGBA8,
    .min_filter = SG_FILTER_NEAREST,
    .mag_filter = SG_FILTER_NEAREST,
    .data.subimage[0][0] =
    {
    .ptr = font_bitmap_rgba,
    .size = (size_t)(512 * 512 * 4),
    }
  } );

  stbtt_fontinfo font;
  stbtt_InitFont(&font, fontBuffer, 0);
  int ascent = 0;
  int descent = 0;
  int lineGap = 0;
  float scale = stbtt_ScaleForPixelHeight(&font, font_size);
  stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
  font_line_advance = (float)(ascent - descent + lineGap) * scale * 0.75f;

  free(font_bitmap_rgba);
  free(fontBuffer);
 }

 state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc)
   {
    .usage = SG_USAGE_STREAM,
    //.data = SG_RANGE(vertices),
    .size = 1024*500,
    .label = "quad-vertices"
   });

 const sg_shader_desc *desc = quad_program_shader_desc(sg_query_backend());
 assert(desc);
 sg_shader shd = sg_make_shader(desc);

 Color clearcol = colhex(0x98734c);
 state.pip = sg_make_pipeline(&(sg_pipeline_desc)
   {
    .shader = shd,
    .depth = {
     .compare = SG_COMPAREFUNC_LESS_EQUAL,
     .write_enabled = true
    },
    .layout = {
     .attrs =
     {
      [ATTR_quad_vs_position].format = SG_VERTEXFORMAT_FLOAT3,
      [ATTR_quad_vs_texcoord0].format   = SG_VERTEXFORMAT_FLOAT2,
     }
    },
    .colors[0].blend = (sg_blend_state) { // allow transparency
     .enabled = true,
     .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
     .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
     .op_rgb = SG_BLENDOP_ADD,
     .src_factor_alpha = SG_BLENDFACTOR_ONE,
     .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
     .op_alpha = SG_BLENDOP_ADD,
    },
    .label = "quad-pipeline",
   });

 state.pass_action = (sg_pass_action)
 {
  //.colors[0] = { .action=SG_ACTION_CLEAR, .value={12.5f/255.0f, 12.5f/255.0f, 12.5f/255.0f, 1.0f } }
  //.colors[0] = { .action=SG_ACTION_CLEAR, .value={255.5f/255.0f, 255.5f/255.0f, 255.5f/255.0f, 1.0f } }
  // 0x898989 is the color in tiled
  .colors[0] =
  { .action=SG_ACTION_CLEAR, .value={clearcol.r, clearcol.g, clearcol.b, 1.0f } }
 };
}

Vec2 screen_size()
{
 return V2((float)sapp_width(), (float)sapp_height());
}

typedef struct Camera
{
 Vec2 pos;
 float scale;
} Camera;

bool mobile_controls = false;
Vec2 thumbstick_base_pos = {0};
Vec2 thumbstick_nub_pos = {0};
typedef struct TouchMemory
{
 // need this because uintptr_t = 0 *doesn't* mean no touching!
 bool active;
 uintptr_t identifier;
} TouchMemory;
TouchMemory activate(uintptr_t by)
{
 //Log("Activating %ld\n", by);
 return (TouchMemory){.active = true, .identifier = by};
}
// returns if deactivated
bool maybe_deactivate(TouchMemory *memory, uintptr_t ended_identifier)
{
 if(memory->active)
 {
  if(memory->identifier == ended_identifier)
  {
   //Log("Deactivating %ld\n", memory->identifier);
   *memory = (TouchMemory){0};
   return true;
  }
 }
 else
 {
  return false;
 }
 return false;
}
TouchMemory movement_touch = {0};
TouchMemory roll_pressed_by = {0};
TouchMemory attack_pressed_by = {0};
TouchMemory interact_pressed_by = {0};
bool mobile_roll_pressed = false;
bool mobile_attack_pressed = false;
bool mobile_interact_pressed = false;

float thumbstick_base_size()
{
 if(screen_size().x < screen_size().y)
 {
  return screen_size().x * 0.24f;
 }
 else
 {
  return screen_size().x * 0.14f;
 }
}

float mobile_button_size()
{
 if(screen_size().x < screen_size().y)
 {
  return screen_size().x * 0.2f;
 }
 else
 {
  return screen_size().x * 0.09f;
 }
}

Vec2 roll_button_pos()
{
 return V2(screen_size().x - mobile_button_size(), screen_size().y * 0.4f);
}

Vec2 interact_button_pos()
{
 return V2(screen_size().x - mobile_button_size()*2.0f, screen_size().y * (0.4f + (0.4f - 0.25f)));
}
Vec2 attack_button_pos()
{
 return V2(screen_size().x - mobile_button_size()*2.0f, screen_size().y * 0.25f);
}

// everything is in pixels in world space, 43 pixels is approx 1 meter measured from 
// merchant sprite being 5'6"
const float pixels_per_meter = 43.0f;
Camera cam = {.scale = 2.0f };

Vec2 cam_offset()
{
 Vec2 to_return = AddV2(cam.pos, MulV2F(screen_size(), 0.5f));
 to_return = FloorV2(to_return); // avoid pixel glitching on tilemap atlas
 return to_return; 
}

// in pixels
Vec2 img_size(sg_image img)
{
 sg_image_info info = sg_query_image_info(img);
 return V2((float)info.width, (float)info.height);
}

#define IMG(img) img, full_region(img)

// full region in pixels
AABB full_region(sg_image img)
{
 return (AABB)
 {
  .upper_left = V2(0.0f, 0.0f),
   .lower_right = img_size(img),
 };
}

// screen coords are in pixels counting from bottom left as (0,0), Y+ is up
Vec2 world_to_screen(Vec2 world)
{
 Vec2 to_return = world;
 to_return = MulV2F(to_return, cam.scale);
 to_return = AddV2(to_return, cam_offset());
 return to_return;
}

Vec2 screen_to_world(Vec2 screen)
{
 Vec2 to_return = screen;
 to_return = SubV2(to_return, cam_offset());
 to_return = MulV2F(to_return, 1.0f/cam.scale);
 return to_return;
}

AABB aabb_at(Vec2 at, Vec2 size)
{
 return (AABB){
  .upper_left = at,
  .lower_right = AddV2(at, V2(size.x, -size.y)),
 };
}

AABB aabb_at_yplusdown(Vec2 at, Vec2 size)
{
 return (AABB){
  .upper_left = at,
  .lower_right = AddV2(at, V2(size.x, size.y)),
 };
}

Quad quad_at(Vec2 at, Vec2 size)
{
 Quad to_return;

 to_return.points[0] = V2(0.0, 0.0);
 to_return.points[1] = V2(size.X, 0.0);
 to_return.points[2] = V2(size.X, -size.Y);
 to_return.points[3] = V2(0.0, -size.Y);

 for(int i = 0; i < 4; i++)
 {
  to_return.points[i] = AddV2(to_return.points[i], at);
 }
 return to_return;
}

Quad tile_quad(TileCoord coord)
{
 Quad to_return = quad_at(tilecoord_to_world(coord), V2(TILE_SIZE, TILE_SIZE));

 return to_return;
}

// out must be of at least length 4
Quad quad_centered(Vec2 at, Vec2 size)
{
 Quad to_return = quad_at(at, size);
 for(int i = 0; i < 4; i++)
 {
  to_return.points[i] = AddV2(to_return.points[i], V2(-size.X*0.5f, size.Y*0.5f));
 }
 return to_return;
}

bool aabb_is_valid(AABB aabb)
{
 Vec2 size_vec = SubV2(aabb.lower_right, aabb.upper_left); // negative in vertical direction
 return size_vec.Y < 0.0f && size_vec.X > 0.0f;
}

// positive in both directions
Vec2 aabb_size(AABB aabb)
{
 assert(aabb_is_valid(aabb));
 Vec2 size_vec = SubV2(aabb.lower_right, aabb.upper_left); // negative in vertical direction
 size_vec.y *= -1.0;
 return size_vec;
}

Quad quad_aabb(AABB aabb)
{
 Vec2 size_vec = SubV2(aabb.lower_right, aabb.upper_left); // negative in vertical direction

 assert(aabb_is_valid(aabb));
 return (Quad) {
  .ul = aabb.upper_left,
  .ur = AddV2(aabb.upper_left, V2(size_vec.X, 0.0f)),
  .lr = AddV2(aabb.upper_left, size_vec),
  .ll = AddV2(aabb.upper_left, V2(0.0f, size_vec.Y)),
 };
}


// both segment_a and segment_b must be arrays of length 2
bool segments_overlapping(float *a_segment, float *b_segment)
{
 assert(a_segment[1] >= a_segment[0]);
 assert(b_segment[1] >= b_segment[0]);
 float total_length = (a_segment[1] - a_segment[0]) + (b_segment[1] - b_segment[0]);
 float farthest_to_left = fminf(a_segment[0], b_segment[0]);
 float farthest_to_right = fmaxf(a_segment[1], b_segment[1]);
 if (farthest_to_right - farthest_to_left < total_length)
 {
  return true;
 } else
 {
  return false;
 }
}

bool overlapping(AABB a, AABB b)
{
 // x axis

 {
  float a_segment[2] =
  { a.upper_left.X, a.lower_right.X };
  float b_segment[2] =
  { b.upper_left.X, b.lower_right.X };
  if(segments_overlapping(a_segment, b_segment))
  {
  } else
  {
   return false;
  }
 }

 // y axis

 {
  float a_segment[2] =
  { a.lower_right.Y, a.upper_left.Y };
  float b_segment[2] =
  { b.lower_right.Y, b.upper_left.Y };
  if(segments_overlapping(a_segment, b_segment))
  {
  } else
  {
   return false;
  }
 }

 return true; // both segments overlapping
}

bool has_point(AABB aabb, Vec2 point)
{
 return
  (aabb.upper_left.X < point.X && point.X < aabb.lower_right.X) &&
  (aabb.upper_left.Y > point.Y && point.Y > aabb.lower_right.Y);
}

AABB screen_cam_aabb()
{
 return (AABB){ .upper_left = V2(0.0, screen_size().Y), .lower_right = V2(screen_size().X, 0.0) };
}

AABB world_cam_aabb()
{
 AABB to_return = screen_cam_aabb();
 to_return.upper_left = screen_to_world(to_return.upper_left);
 to_return.lower_right = screen_to_world(to_return.lower_right);
 return to_return;
}

int num_draw_calls = 0;

#define FLOATS_PER_VERTEX (3 + 2)
float cur_batch_data[1024*10] = {0};
int cur_batch_data_index = 0;
// @TODO check last tint as well, do this when factor into drawing parameters
sg_image cur_batch_image = {0};
quad_fs_params_t cur_batch_params = {0};
void flush_quad_batch()
{
 if(cur_batch_image.id == 0 || cur_batch_data_index == 0) return; // flush called when image changes, image starts out null!
 state.bind.vertex_buffer_offsets[0] = sg_append_buffer(state.bind.vertex_buffers[0], &(sg_range){cur_batch_data, cur_batch_data_index*sizeof(*cur_batch_data)});
 state.bind.fs_images[SLOT_quad_tex] = cur_batch_image;
 sg_apply_bindings(&state.bind);
 sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_quad_fs_params, &SG_RANGE(cur_batch_params));
 assert(cur_batch_data_index % FLOATS_PER_VERTEX == 0);
 sg_draw(0, cur_batch_data_index/FLOATS_PER_VERTEX, 1);
 num_draw_calls += 1;
 memset(cur_batch_data, 0, cur_batch_data_index*sizeof(*cur_batch_data));
 cur_batch_data_index = 0;
}


#define Y_COORD_IN_BACK (-1.0f)
#define Y_COORD_IN_FRONT (3.0f)
typedef struct DrawParams
{
 bool world_space;
 Quad quad;
 sg_image image;
 AABB image_region;
 Color tint;

 AABB clip_to; // if world space is in world space, if screen space is in screen space - Lao Tzu
 float y_coord_sorting; // Y_COORD_IN_BACK, or the smallest value, is all the way in the back, Y_COORD_IN_FRONT is in the front
 float alpha_clip_threshold;
 bool queue_for_translucent;
} DrawParams;

BUFF(DrawParams, 1024) translucent_queue = {0};

Vec2 into_clip_space(Vec2 screen_space_point)
{
  Vec2 zero_to_one = DivV2(screen_space_point, screen_size());
  Vec2 in_clip_space = SubV2(MulV2F(zero_to_one, 2.0), V2(1.0, 1.0));
  return in_clip_space;
}

// The image region is in pixel space of the image
void draw_quad(DrawParams d)
{
 quad_fs_params_t params = {0};
 params.tint[0] = d.tint.R;
 params.tint[1] = d.tint.G;
 params.tint[2] = d.tint.B;
 params.tint[3] = d.tint.A;
 params.alpha_clip_threshold = d.alpha_clip_threshold;
 if(aabb_is_valid(d.clip_to) && LenV2(aabb_size(d.clip_to)) > 0.1)
 {
  if(d.world_space)
  {
   d.clip_to.upper_left = world_to_screen(d.clip_to.upper_left);
   d.clip_to.lower_right = world_to_screen(d.clip_to.lower_right);
  }
  Vec2 aabb_clip_ul = into_clip_space(d.clip_to.upper_left);
  Vec2 aabb_clip_lr = into_clip_space(d.clip_to.lower_right);
  params.clip_ul[0] = aabb_clip_ul.x;
  params.clip_ul[1] = aabb_clip_ul.y;
  params.clip_lr[0] = aabb_clip_lr.x;
  params.clip_lr[1] = aabb_clip_lr.y;
 }
 else
 {
  params.clip_ul[0] = -1.0;
  params.clip_ul[1] = 1.0;
  params.clip_lr[0] = 1.0;
  params.clip_lr[1] = -1.0;
 }

 Vec2 *points = d.quad.points;
 if(d.world_space)
 {
  for(int i = 0; i < 4; i++)
  {
   points[i] = world_to_screen(points[i]);
  }
 }
 // we've aplied the world space transform
 d.world_space = false;

 if(d.queue_for_translucent)
 {
  BUFF_APPEND(&translucent_queue, d);
  return;
 }

 PROFILE_SCOPE("Draw quad")
 {



 // if the rendering call is different, and the batch must be flushed
 if(d.image.id != cur_batch_image.id || memcmp(&params,&cur_batch_params,sizeof(params)) != 0 )
 {
  flush_quad_batch();
  cur_batch_image = d.image;
  cur_batch_params = params;
 }


 AABB cam_aabb = screen_cam_aabb();
 AABB points_bounding_box = { .upper_left = V2(INFINITY, -INFINITY), .lower_right = V2(-INFINITY, INFINITY) };

 for(int i = 0; i < 4; i++)
 {
  points_bounding_box.upper_left.X = fminf(points_bounding_box.upper_left.X, points[i].X);
  points_bounding_box.upper_left.Y = fmaxf(points_bounding_box.upper_left.Y, points[i].Y);

  points_bounding_box.lower_right.X = fmaxf(points_bounding_box.lower_right.X, points[i].X);
  points_bounding_box.lower_right.Y = fminf(points_bounding_box.lower_right.Y, points[i].Y);
 }
 if(!overlapping(cam_aabb, points_bounding_box))
 {
  //dbgprint("Out of screen, cam aabb %f %f %f %f\n", cam_aabb.upper_left.X, cam_aabb.upper_left.Y, cam_aabb.lower_right.X, cam_aabb.lower_right.Y);
  //dbgprint("Points boundig box %f %f %f %f\n", points_bounding_box.upper_left.X, points_bounding_box.upper_left.Y, points_bounding_box.lower_right.X, points_bounding_box.lower_right.Y);
  continue; // cull out of screen quads
 }

 float new_vertices[ FLOATS_PER_VERTEX*4 ] = {0};
 Vec2 region_size = SubV2(d.image_region.lower_right, d.image_region.upper_left);
 assert(region_size.X > 0.0);
 assert(region_size.Y > 0.0);
 Vec2 tex_coords[4] =
 {
  AddV2(d.image_region.upper_left, V2(0.0,                     0.0)),
  AddV2(d.image_region.upper_left, V2(region_size.X,           0.0)),
  AddV2(d.image_region.upper_left, V2(region_size.X, region_size.Y)),
  AddV2(d.image_region.upper_left, V2(0.0,           region_size.Y)),
 };

 // convert to uv space
 sg_image_info info = sg_query_image_info(d.image);
 for(int i = 0; i < 4; i++)
 {
  tex_coords[i] = DivV2(tex_coords[i], V2((float)info.width, (float)info.height));
 }
 for(int i = 0; i < 4; i++)
 {
  Vec2 in_clip_space = into_clip_space(points[i]);
  new_vertices[i*FLOATS_PER_VERTEX + 0] = in_clip_space.X;
  new_vertices[i*FLOATS_PER_VERTEX + 1] = in_clip_space.Y;
  // update Y_COORD_IN_BACK, Y_COORD_IN_FRONT when this changes
  float unmapped = (clampf(d.y_coord_sorting, -1.0f, 2.0f));
  float mapped = (unmapped + 1.0f)/3.0f;
  new_vertices[i*FLOATS_PER_VERTEX + 2] = 1.0f - (float)clamp(mapped, 0.0, 1.0);
  new_vertices[i*FLOATS_PER_VERTEX + 3] = tex_coords[i].X;
  new_vertices[i*FLOATS_PER_VERTEX + 4] = tex_coords[i].Y;
 }

 // two triangles drawn, six vertices
 size_t total_size = 6*FLOATS_PER_VERTEX;

 // batched a little too close to the sun
 if(cur_batch_data_index + total_size >= ARRLEN(cur_batch_data))
 {
  flush_quad_batch();
  cur_batch_image = d.image;
  cur_batch_params = params;
 }

#define PUSH_VERTEX(vert) { memcpy(&cur_batch_data[cur_batch_data_index], &vert, FLOATS_PER_VERTEX*sizeof(float)); cur_batch_data_index += FLOATS_PER_VERTEX; }
 PUSH_VERTEX(new_vertices[0*FLOATS_PER_VERTEX]);
 PUSH_VERTEX(new_vertices[1*FLOATS_PER_VERTEX]);
 PUSH_VERTEX(new_vertices[2*FLOATS_PER_VERTEX]);
 PUSH_VERTEX(new_vertices[0*FLOATS_PER_VERTEX]);
 PUSH_VERTEX(new_vertices[2*FLOATS_PER_VERTEX]);
 PUSH_VERTEX(new_vertices[3*FLOATS_PER_VERTEX]);
#undef PUSH_VERTEX

}
}

int translucent_compare(const void *a, const void *b)
{
 DrawParams *a_draw = (DrawParams*)a;
 DrawParams *b_draw = (DrawParams*)b;

 return (int)((a_draw->y_coord_sorting - b_draw->y_coord_sorting)*1000.0f);
}

void draw_all_translucent()
{
 qsort(&translucent_queue.data[0], translucent_queue.cur_index, sizeof(translucent_queue.data[0]), translucent_compare);
 BUFF_ITER(DrawParams, &translucent_queue)
 {
  it->queue_for_translucent = false;
  draw_quad(*it);
 }
 BUFF_CLEAR(&translucent_queue);
}

void swap(Vec2 *p1, Vec2 *p2)
{
 Vec2 tmp = *p1;
 *p1 = *p2;
 *p2 = tmp;
}

double anim_sprite_duration(AnimatedSprite *s)
{
 return s->num_frames * s->time_per_frame;
}




Vec2 tile_id_to_coord(sg_image tileset_image, Vec2 tile_size, uint16_t tile_id)
{
 int tiles_per_row = (int)(img_size(tileset_image).X / tile_size.X);
 int tile_index = tile_id - 1;
 int tile_image_row = tile_index / tiles_per_row;
 int tile_image_col = tile_index - tile_image_row*tiles_per_row;
 Vec2 tile_image_coord = V2((float)tile_image_col * tile_size.X, (float)tile_image_row*tile_size.Y);
 return tile_image_coord;
}

void colorquad(bool world_space, Quad q, Color col)
{
 bool queue = false;
 if(col.A < 1.0f)
 {
  queue = true;
 }
 // y coord sorting for colorquad puts it below text for dialog panel
 draw_quad((DrawParams){world_space, q, image_white_square, full_region(image_white_square), col, .y_coord_sorting = Y_COORD_IN_FRONT - 0.05f, .queue_for_translucent = queue});
}


// in world coordinates
void line(Vec2 from, Vec2 to, float line_width, Color color)
{
 Vec2 normal = rotate_counter_clockwise(NormV2(SubV2(to, from)));
 Quad line_quad = {
  .points = {
   AddV2(from, MulV2F(normal, line_width)),  // upper left
   AddV2(to, MulV2F(normal, line_width)),    // upper right
   AddV2(to, MulV2F(normal, -line_width)),   // lower right
   AddV2(from, MulV2F(normal, -line_width)), // lower left
  }
 };
 colorquad(true, line_quad, color);
}

#ifdef DEVTOOLS
bool show_devtools = true;
#ifdef PROFILING
extern bool profiling;
#else
bool profiling;
#endif
#endif

void dbgsquare(Vec2 at)
{
#ifdef DEVTOOLS
 if(!show_devtools) return;
 colorquad(true, quad_centered(at, V2(10.0, 10.0)), RED);
#else
 (void)at;
#endif
}

void dbgline(Vec2 from, Vec2 to)
{
#ifdef DEVTOOLS
 if(!show_devtools) return;
 line(from, to, 0.5f, RED);
#else
 (void)from;
 (void)to;
#endif
}

void dbgvec(Vec2 from, Vec2 vec)
{
 Vec2 to = AddV2(from, vec);
 dbgline(from, to);
}

// in world space
void dbgrect(AABB rect)
{
#ifdef DEVTOOLS
 if(!show_devtools) return;
 const float line_width = 0.5;
 const Color col = RED;
 Quad q = quad_aabb(rect);
 line(q.ul, q.ur, line_width, col);
 line(q.ur, q.lr, line_width, col);
 line(q.lr, q.ll, line_width, col);
 line(q.ll, q.ul, line_width, col);
#else
 (void)rect;
#endif
}

// from_point is for knockback
void request_do_damage(Entity *to, Entity *from, float damage)
{
 Vec2 from_point = from->pos;
 if(to == NULL) return;
 if(to->is_bullet)
 {
  Vec2 norm = NormV2(SubV2(to->pos, from_point));
  dbgvec(from_point, norm);
  to->vel = ReflectV2(to->vel, norm);
  dbgprint("deflecitng\n");
 }
 else if(true)
 {
  // damage processing is done in process perception so in training, has accurate values for
  // NPC health
  if(from->is_character)
  {
   process_perception(to, (Perception){.type = PlayerAction, .player_action_type = ACT_hits_npc, .damage_done = damage,});
  }
  else
  {
   process_perception(to, (Perception){.type = EnemyAction, .enemy_action_type = ACT_hits_npc, .damage_done = damage,});
  }
  to->vel = MulV2F(NormV2(SubV2(to->pos, from_point)), 15.0f);
 }
 else
 {
  Log("Can't do damage to npc...\n");
 }
}


typedef struct TextParams
{
 bool world_space;
 bool dry_run;
 const char *text;
 Vec2 pos;
 Color color;
 float scale;
 AABB clip_to; // if in world space, in world space. In space of pos given
 Color *colors; // color per character, if not null must be array of same length as text
} TextParams;

// returns bounds. To measure text you can set dry run to true and get the bounds
AABB draw_text(TextParams t)
{
 size_t text_len = strlen(t.text);
 AABB bounds = {0};
 float y = 0.0;
 float x = 0.0;
 for(int i = 0; i < text_len; i++)
 {
  stbtt_aligned_quad q;
  float old_y = y;
  stbtt_GetBakedQuad(cdata, 512, 512, t.text[i]-32, &x, &y, &q, 1);
  float difference = y - old_y;
  y = old_y + difference;

  Vec2 size = V2(q.x1 - q.x0, q.y1 - q.y0);
  if(t.text[i] == '\n')
  {
#ifdef DEVTOOLS
   y += font_size*0.75f; // arbitrary, only debug t.text has newlines
   x = 0.0;
#else
   assert(false);
#endif
  }
  if(size.Y > 0.0 && size.X > 0.0)
  { // spaces (and maybe other characters) produce quads of size 0
   Quad to_draw = {
    .points = {
      AddV2(V2(q.x0, -q.y0), V2(0.0f, 0.0f)),
      AddV2(V2(q.x0, -q.y0), V2(size.X, 0.0f)),
      AddV2(V2(q.x0, -q.y0), V2(size.X, -size.Y)),
      AddV2(V2(q.x0, -q.y0), V2(0.0f, -size.Y)),
    },
   };

   for(int i = 0; i < 4; i++)
   {
    to_draw.points[i] = MulV2F(to_draw.points[i], t.scale);
   }

   AABB font_atlas_region = (AABB)
   {
    .upper_left  = V2(q.s0, q.t0),
     .lower_right = V2(q.s1, q.t1),
   };
   font_atlas_region.upper_left.X *= img_size(image_font).X;
   font_atlas_region.lower_right.X *= img_size(image_font).X;
   font_atlas_region.upper_left.Y *= img_size(image_font).Y;
   font_atlas_region.lower_right.Y *= img_size(image_font).Y;

   for(int i = 0; i < 4; i++)
   {
    bounds.upper_left.X = fminf(bounds.upper_left.X, to_draw.points[i].X);
    bounds.upper_left.Y = fmaxf(bounds.upper_left.Y, to_draw.points[i].Y);
    bounds.lower_right.X = fmaxf(bounds.lower_right.X, to_draw.points[i].X);
    bounds.lower_right.Y = fminf(bounds.lower_right.Y, to_draw.points[i].Y);
   }

   for(int i = 0; i < 4; i++)
   {
    to_draw.points[i] = AddV2(to_draw.points[i], t.pos);
   }

   if(!t.dry_run)
   {
    Color col = t.color;
    if(t.colors)
    {
     col = t.colors[i];
    }
    if(false) // drop shadow, don't really like it
    if(t.world_space)
    {
     Quad shadow_quad = to_draw;
     for(int i = 0; i < 4; i++)
     {
      shadow_quad.points[i] = AddV2(shadow_quad.points[i], V2(0.0, -1.0));
     }
     draw_quad((DrawParams){t.world_space, shadow_quad, image_font, font_atlas_region, (Color){0.0f,0.0f,0.0f,0.4f}, t.clip_to, .y_coord_sorting = Y_COORD_IN_FRONT, .queue_for_translucent = true});
    }
    draw_quad((DrawParams){t.world_space, to_draw, image_font, font_atlas_region, col, t.clip_to, .y_coord_sorting = Y_COORD_IN_FRONT, .queue_for_translucent = true});
   }
  }
 }

 bounds.upper_left = AddV2(bounds.upper_left, t.pos);
 bounds.lower_right = AddV2(bounds.lower_right, t.pos);
 return bounds;
}

float y_coord_sorting_at(Vec2 pos)
{
 float y_coord_sorting = world_to_screen(pos).y / screen_size().y;

 y_coord_sorting = 1.0f - y_coord_sorting;

 // debug draw the y cord sorting value
#if 0
 char *to_draw = tprint("%f", y_coord_sorting);
 draw_text((TextParams){true, false, to_draw, pos, BLACK, 1.0f});
#endif
 
 return y_coord_sorting;
}

void draw_shadow_for(DrawParams d)
{
 Quad sheared_quad = d.quad;
 float height = d.quad.ur.y - d.quad.lr.y;
 Vec2 shear_addition = V2(-height*0.35f, -height*0.2f);
 sheared_quad.ul = AddV2(sheared_quad.ul, shear_addition);
 sheared_quad.ur = AddV2(sheared_quad.ur, shear_addition);
 d.quad = sheared_quad;
 d.tint = (Color){0,0,0,0.2f};
 d.y_coord_sorting -= 0.05f;
 d.alpha_clip_threshold = 0.0f;
 d.queue_for_translucent = true;
 dbgline(sheared_quad.ul, sheared_quad.ur);
 dbgline(sheared_quad.ur, sheared_quad.lr);
 dbgline(sheared_quad.lr, sheared_quad.ll);
 dbgline(sheared_quad.ll, sheared_quad.ul);
 draw_quad(d);
}

void draw_animated_sprite(AnimatedSprite *s, double elapsed_time, bool flipped, Vec2 pos, Color tint)
{
 float y_sort_pos = y_coord_sorting_at(pos);
 pos = AddV2(pos, s->offset);
 sg_image spritesheet_img = *s->img;
 int index = (int)floor(elapsed_time/s->time_per_frame) % s->num_frames;
 if(s->no_wrap)
 {
  index = (int)floor(elapsed_time/s->time_per_frame);
  if(index >= s->num_frames) index = s->num_frames - 1;
 }

 Quad q = quad_centered(pos, s->region_size);

 if(flipped)
 {
  swap(&q.points[0], &q.points[1]);
  swap(&q.points[3], &q.points[2]);
 }

 AABB region;
 region.upper_left = AddV2(s->start, V2(index * s->horizontal_diff_btwn_frames, 0.0f));
 float width = img_size(spritesheet_img).X;
 while(region.upper_left.X >= width)
 {
  region.upper_left.X -= width;
  region.upper_left.Y += s->region_size.Y;
 }
 region.lower_right = AddV2(region.upper_left, s->region_size);

 DrawParams d = (DrawParams){true, q, spritesheet_img, region, tint, .y_coord_sorting = y_sort_pos, .alpha_clip_threshold = 0.2f};
 draw_shadow_for(d);
 draw_quad(d);
}

// gets aabbs overlapping the input aabb, including gs.entities and tiles
Overlapping get_overlapping(Level *l, AABB aabb)
{
 Overlapping to_return = {0};
 
 Quad q = quad_aabb(aabb);
 // the corners, jessie
 for(int i = 0; i < 4; i++)
 {
  TileInstance t = get_tile(l, world_to_tilecoord(q.points[i]));
  if(is_tile_solid(t))
  {
   Overlap element = ((Overlap){.is_tile = true, .t = t});
   //{ (&to_return)[(&to_return)->cur_index++] = element; assert((&to_return)->cur_index < ARRLEN((&to_return)->data)); }
   BUFF_APPEND(&to_return, element);
  }
 }

 // the gs.entities jessie
 ENTITIES_ITER(gs.entities)
 {
  if(!(it->is_character && it->is_rolling) && overlapping(aabb, entity_aabb(it)))
  {
   BUFF_APPEND(&to_return, (Overlap){.e = it});
  }
 }

 return to_return;
}

typedef struct CollisionInfo
{
 bool happened;
 Vec2 normal;
}CollisionInfo;

typedef struct MoveSlideParams
{
 Entity *from;
 Vec2 position;
 Vec2 movement_this_frame;

 // optional
 bool dont_collide_with_entities;
 CollisionInfo *col_info_out;
} MoveSlideParams;


// returns new pos after moving and sliding against collidable things
Vec2 move_and_slide(MoveSlideParams p)
{
 Vec2 collision_aabb_size = entity_aabb_size(p.from);
 Vec2 new_pos = AddV2(p.position, p.movement_this_frame);
 assert(collision_aabb_size.x > 0.0f);
 assert(collision_aabb_size.y > 0.0f);
 AABB at_new = centered_aabb(new_pos, collision_aabb_size);
 dbgrect(at_new);
 BUFF(AABB, 256) to_check = {0};

 // add tilemap boxes
 {
  Vec2 at_new_size_vector = SubV2(at_new.lower_right, at_new.upper_left);
  Vec2 points_to_check[] = {
   AddV2(at_new.upper_left, V2(0.0, 0.0)),
   AddV2(at_new.upper_left, V2(at_new_size_vector.X, 0.0)),
   AddV2(at_new.upper_left, V2(at_new_size_vector.X, at_new_size_vector.Y)),
   AddV2(at_new.upper_left, V2(0.0, at_new_size_vector.Y)),
  };
  for(int i = 0; i < ARRLEN(points_to_check); i++)
  {
   Vec2 *it = &points_to_check[i];
   TileCoord tilecoord_to_check = world_to_tilecoord(*it);

   if(is_tile_solid(get_tile_layer(&level_level0, 2, tilecoord_to_check)))
   
    BUFF_APPEND(&to_check, tile_aabb(tilecoord_to_check));
  }
 }

 // add entity boxes
 if(!p.dont_collide_with_entities && !(p.from->is_character && p.from->is_rolling))
 {
  ENTITIES_ITER(gs.entities)
  {
   if(!(it->is_character && it->is_rolling) && it != p.from && !(it->is_npc && it->dead) && !it->is_item)
   {
    BUFF_APPEND(&to_check, centered_aabb(it->pos, entity_aabb_size(it)));
   }
  }
 }

 // here we do some janky C stuff to resolve collisions with the closest
 // box first, because doing so is a simple heuristic to avoid depenetrating and losing
 // sideways velocity. It's visual and I can't put diagrams in code so uh oh!

 BUFF(AABB, 32) actually_overlapping = {0};
 BUFF_ITER(AABB, &to_check)
 {
  if(overlapping(at_new, *it))
  {
   BUFF_APPEND(&actually_overlapping, *it);
  }
 }

 float smallest_distance = FLT_MAX;
 int smallest_aabb_index = 0;
 int i = 0;
 BUFF_ITER(AABB, &actually_overlapping)
 {
  float cur_dist = LenV2(SubV2(aabb_center(at_new), aabb_center(*it)));
  if(cur_dist < smallest_distance){
   smallest_distance = cur_dist;
   smallest_aabb_index = i;
  }
  i++;
 }
 

 i = 0;
 BUFF(AABB, 32) overlapping_smallest_first = {0};
 if(actually_overlapping.cur_index > 0)
 {
  BUFF_APPEND(&overlapping_smallest_first, actually_overlapping.data[smallest_aabb_index]);
 }
 BUFF_ITER(AABB, &actually_overlapping)
 {
  if(i == smallest_aabb_index)
  {
   continue;
  }
  else
  {
   BUFF_APPEND(&overlapping_smallest_first, *it);
  }
  i++;
 }

 CollisionInfo info = {0};
 for(int col_iter_i = 0; col_iter_i < 1; col_iter_i++)
 BUFF_ITER(AABB, &overlapping_smallest_first)
 {
  AABB to_depenetrate_from = *it;
  dbgrect(to_depenetrate_from);
  int iters_tried_to_push_apart = 0;
  while(overlapping(to_depenetrate_from, at_new) && iters_tried_to_push_apart < 500)
  { 
   //dbgsquare(to_depenetrate_from.upper_left);
   //dbgsquare(to_depenetrate_from.lower_right);
   const float move_dist = 0.05f;

   info.happened = true;
   Vec2 from_point = aabb_center(to_depenetrate_from);
   Vec2 to_player = NormV2(SubV2(aabb_center(at_new), from_point));
   Vec2 compass_dirs[4] = {
    V2( 1.0, 0.0),
    V2(-1.0, 0.0),
    V2(0.0,  1.0),
    V2(0.0, -1.0),
   };
   int closest_index = -1;
   float closest_dot = -99999999.0f;
   for(int i = 0; i < 4; i++)
   {
    float dot = DotV2(compass_dirs[i], to_player);
    if(dot > closest_dot)
    {
     closest_index = i;
     closest_dot = dot;
    }
   }
   assert(closest_index != -1);
   Vec2 move_dir = compass_dirs[closest_index];
   info.normal = move_dir;
   dbgvec(from_point, MulV2F(move_dir, 30.0f));
   Vec2 move = MulV2F(move_dir, move_dist);
   at_new.upper_left = AddV2(at_new.upper_left,move);
   at_new.lower_right = AddV2(at_new.lower_right,move);
   iters_tried_to_push_apart++;
  }
 }

 if(p.col_info_out) *p.col_info_out = info;

 Vec2 result_pos = aabb_center(at_new);
 dbgrect(centered_aabb(result_pos, collision_aabb_size));
 return result_pos;
}

// returns next vertical cursor position
float draw_wrapped_text(bool dry_run, Vec2 at_point, float max_width, char *text, Color *colors, float text_scale, bool going_up, AABB clip_to)
{
 char *sentence_to_draw = text;
 size_t sentence_len = strlen(sentence_to_draw);

 Vec2 cursor = at_point;
 while(sentence_len > 0)
 {
  char line_to_draw[MAX_SENTENCE_LENGTH] = {0};
  Color colors_to_draw[MAX_SENTENCE_LENGTH] = {0};
  size_t chars_from_sentence = 0;
  AABB line_bounds = {0};
  while(chars_from_sentence <= sentence_len)
  {
   memset(line_to_draw, 0, MAX_SENTENCE_LENGTH);
   memcpy(line_to_draw, sentence_to_draw, chars_from_sentence);

   line_bounds = draw_text((TextParams){true, true, line_to_draw, cursor, BLACK, text_scale, clip_to});
   if(line_bounds.lower_right.X > at_point.X + max_width)
   {
    // too big
    assert(chars_from_sentence > 0);
    chars_from_sentence -= 1;
    break;
   }

   chars_from_sentence += 1;
  }
  if(chars_from_sentence > sentence_len) chars_from_sentence--;
  memset(line_to_draw, 0, MAX_SENTENCE_LENGTH);
  memcpy(line_to_draw, sentence_to_draw, chars_from_sentence);
  memcpy(colors_to_draw, colors, chars_from_sentence*sizeof(Color));

  //float line_height = line_bounds.upper_left.Y - line_bounds.lower_right.Y;
  float line_height = font_line_advance * text_scale;
  AABB drawn_bounds = draw_text((TextParams){true, dry_run, line_to_draw, AddV2(cursor, V2(0.0f, -line_height)), BLACK, text_scale, clip_to, colors_to_draw});
  if(!dry_run) dbgrect(drawn_bounds);

  sentence_len -= chars_from_sentence;
  sentence_to_draw += chars_from_sentence;
  colors += chars_from_sentence;
  cursor = V2(drawn_bounds.upper_left.X, drawn_bounds.lower_right.Y);
 
  // caught a random infinite loop in the debugger, maybe this will stop it. Need to test and make sure it doesn't early out on valid text cases
  if(!has_point(clip_to, cursor))
  {
   break;
  }
 }

 return cursor.Y;
}

Sentence *last_said_sentence(Entity *npc)
{
 int i = 0;
 BUFF_ITER(Perception, &npc->remembered_perceptions)
 {
  bool is_last_said = i == npc->remembered_perceptions.cur_index - 1;
  if(is_last_said && it->type == NPCDialog)
  {
   return &it->npc_dialog;
  }
  i += 1;
 }
 return 0;
}


void draw_dialog_panel(Entity *talking_to, float alpha)
{
 float panel_width = 250.0f;
 float panel_height = 150.0f;
 float panel_vert_offset = 30.0f;
 AABB dialog_panel = (AABB){
  .upper_left = AddV2(talking_to->pos, V2(-panel_width/2.0f, panel_vert_offset+panel_height)),
   .lower_right = AddV2(talking_to->pos, V2(panel_width/2.0f, panel_vert_offset)),
 };
 AABB constrict_to = world_cam_aabb();
 dialog_panel.upper_left.x = fmaxf(constrict_to.upper_left.x, dialog_panel.upper_left.x);
 dialog_panel.lower_right.y = fmaxf(constrict_to.lower_right.y, dialog_panel.lower_right.y);
 dialog_panel.upper_left.y = fminf(constrict_to.upper_left.y, dialog_panel.upper_left.y);
 dialog_panel.lower_right.x = fminf(constrict_to.lower_right.x, dialog_panel.lower_right.x);

 if(aabb_is_valid(dialog_panel))
 {
  Quad dialog_quad = quad_aabb(dialog_panel);
  float line_width = 2.0f;
  Quad panel_quad = dialog_quad;
  {
   float inset = line_width;
   panel_quad.ul = AddV2(panel_quad.ul, V2(inset,  -inset));
   panel_quad.ll = AddV2(panel_quad.ll, V2(inset,   inset));
   panel_quad.lr = AddV2(panel_quad.lr, V2(-inset,  inset));
   panel_quad.ur = AddV2(panel_quad.ur, V2(-inset, -inset));
  }
  colorquad(true, panel_quad, (Color){1.0f, 1.0f, 1.0f, 0.7f*alpha});
  Color line_color  = (Color){0,0,0,alpha};
  line(AddV2(dialog_quad.ul, V2(-line_width,0.0)), AddV2(dialog_quad.ur, V2(line_width,0.0)), line_width, line_color);
  line(dialog_quad.ur, dialog_quad.lr, line_width, line_color);
  line(AddV2(dialog_quad.lr, V2(line_width,0.0)), AddV2(dialog_quad.ll, V2(-line_width,0.0)), line_width, line_color);
  line(dialog_quad.ll, dialog_quad.ul, line_width, line_color);

  float padding = 5.0f;
  dialog_panel.upper_left = AddV2(dialog_panel.upper_left, V2(padding, -padding));
  dialog_panel.lower_right = AddV2(dialog_panel.lower_right, V2(-padding, padding));
  
  if(aabb_is_valid(dialog_panel))
  {
   float new_line_height = dialog_panel.lower_right.Y;

   typedef struct 
   {
    Sentence s;
    bool is_player;
   } DialogElement;

   BUFF(DialogElement, 32) dialog = {0};
   int i = 0;
   BUFF_ITER(Perception, &talking_to->remembered_perceptions)
   {
    if(it->type == NPCDialog)
    {
     Sentence to_say = it->npc_dialog;
     Sentence *last_said = last_said_sentence(talking_to);
     if(last_said == &it->npc_dialog)
     {
      to_say = (Sentence){0};
      for(int i = 0; i < min(it->npc_dialog.cur_index, (int)talking_to->characters_said); i++)
      {
       BUFF_APPEND(&to_say, it->npc_dialog.data[i]);
      }
     }
     BUFF_APPEND(&dialog, ((DialogElement){ .s = to_say, .is_player = false }) );
    }
    else if(it->type == PlayerDialog)
    {
     BUFF_APPEND(&dialog, ((DialogElement){ .s = it->player_dialog, .is_player = true }) );
    }
   }
   if(dialog.cur_index > 0)
   {
    for(int i = dialog.cur_index - 1; i >= 0; i--)
    {
     DialogElement *it = &dialog.data[i];
     {
      Color *colors = calloc(sizeof(*colors), it->s.cur_index);
      for(int char_i = 0; char_i < it->s.cur_index; char_i++)
      {
       if(it->is_player)
       {
        colors[char_i] = BLACK;
       }
       else
       {
        colors[char_i] = colhex(0x345e22);
       }
       colors[char_i] = blendalpha(colors[char_i], alpha);
      }
      float measured_line_height = draw_wrapped_text(true, V2(dialog_panel.upper_left.X, new_line_height), dialog_panel.lower_right.X - dialog_panel.upper_left.X, it->s.data, colors, 0.5f, true, dialog_panel);
      new_line_height += (new_line_height - measured_line_height);
      draw_wrapped_text(false, V2(dialog_panel.upper_left.X, new_line_height), dialog_panel.lower_right.X - dialog_panel.upper_left.X, it->s.data, colors, 0.5f, true, dialog_panel);

      free(colors);
     }
    }
   }

   dbgrect(dialog_panel);
  }
 }

}

#define ROLL_KEY SAPP_KEYCODE_LEFT_SHIFT
double elapsed_time = 0.0;
double last_frame_processing_time = 0.0;
uint64_t last_frame_time;
Vec2 mouse_pos = {0}; // in screen space
bool interact_just_pressed = false;
float learned_shift = 0.0;
float learned_space = 0.0;
float learned_e = 0.0;
#ifdef DEVTOOLS
bool mouse_frozen = false;
#endif
void frame(void)
{
 // elapsed_time
 double dt_double = 0.0;
 {
  dt_double = stm_sec(stm_diff(stm_now(), last_frame_time));
  dt_double = fmin(dt_double, 5.0 / 60.0); // clamp dt at maximum 5 frames, avoid super huge dt
  elapsed_time += dt_double;
  last_frame_time = stm_now();
 }
 float dt = (float)dt_double;


#if 0
 {
  printf("Frametime: %.1f ms\n", dt*1000.0);
  sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
  sg_apply_pipeline(state.pip);

  //colorquad(false, quad_at(V2(0.0, 100.0), V2(100.0f, 100.0f)), RED);
  sg_image img = image_white_square;
  AABB region = full_region(img);
  //region.lower_right.X *= 0.5f;
  draw_quad((DrawParams){false,quad_at(V2(0.0, 100.0), V2(100.0f, 100.0f)), img, region, WHITE});


  flush_quad_batch();
  sg_end_pass();
  sg_commit();
  reset(&scratch);
 }
 return;
#endif

 PROFILE_SCOPE("frame")
 {

#ifdef DESKTOP
  if(!receiving_text_input && in_dialog())
  {
   receiving_text_input = true;
   BUFF_CLEAR(&text_input_buffer);
  }
#endif

  // better for vertical aspect ratios
  if(screen_size().x < 0.7f*screen_size().y)
  {
   cam.scale = 2.3f;
  }
  else
  {
   cam.scale = 2.0f;
  }


  uint64_t time_start_frame = stm_now();

  Vec2 movement = {0};
  bool attack = false;
  bool roll = false;
  bool interact = false;
  if(mobile_controls)
  {
   movement = SubV2(thumbstick_nub_pos, thumbstick_base_pos);
   if(LenV2(movement) > 0.0f)
   {
    movement = MulV2F(NormV2(movement), LenV2(movement)/(thumbstick_base_size()*0.5f));
   }
   attack = mobile_attack_pressed;
   roll = mobile_roll_pressed;
   interact = interact_just_pressed;
  }
  else
  {
   movement = V2(
     (float)keydown[SAPP_KEYCODE_D] - (float)keydown[SAPP_KEYCODE_A],
     (float)keydown[SAPP_KEYCODE_W] - (float)keydown[SAPP_KEYCODE_S]
     );
   attack = keydown[SAPP_KEYCODE_SPACE];
#ifdef DEVTOOLS
   attack = attack || keydown[SAPP_KEYCODE_LEFT_CONTROL]; 
#endif
   roll = keydown[ROLL_KEY];
   interact = interact_just_pressed;
  }
  if(LenV2(movement) > 1.0)
  {
   movement = NormV2(movement);
  }

  sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
  sg_apply_pipeline(state.pip);

  Level *cur_level = &level_level0;

  // Draw Tilemap draw tilemap tilemap drawing
#if 1
  PROFILE_SCOPE("tilemap")
  {
   Vec2 starting_world = AddV2(world_cam_aabb().upper_left, V2(-TILE_SIZE, TILE_SIZE));
   Vec2 ending_world = AddV2(world_cam_aabb().lower_right, V2(TILE_SIZE, -TILE_SIZE));

   TileCoord starting_point = world_to_tilecoord(starting_world);
   TileCoord ending_point = world_to_tilecoord(ending_world);

   int starting_row = starting_point.y;
   int ending_row = ending_point.y;

   int starting_col = starting_point.x;
   int ending_col = ending_point.x;

   for(int layer = 0; layer < LAYERS; layer++)
   {
    for(int row = starting_row; row < ending_row; row++)
    {
     for(int col = starting_col; col < ending_col; col++)
     {
      TileCoord cur_coord = { col, row };
      TileInstance cur = get_tile_layer(cur_level, layer, cur_coord);

      int tileset_i = 0;
      uint16_t max_gid = 0;
      for(int i = 0; i < ARRLEN(tilesets); i++)
      {
       TileSet tileset = tilesets[i];
       if(cur.kind > tileset.first_gid && tileset.first_gid > max_gid)
       {
        tileset_i = i;
        max_gid = tileset.first_gid;
       }
      }

      TileSet tileset = tilesets[tileset_i];
      cur.kind -= tileset.first_gid - 1;

      if(cur.kind != 0)
      {
       Vec2 tile_size = V2(TILE_SIZE, TILE_SIZE);

       sg_image tileset_image = *tileset.img;

       Vec2 tile_image_coord = tile_id_to_coord(tileset_image, tile_size, cur.kind);

       AnimatedTile *anim = NULL;
       for(int i = 0; i < sizeof(tileset.animated)/sizeof(*tileset.animated); i++)
       {
        if(tileset.animated[i].exists && tileset.animated[i].id_from == cur.kind-1)
        {
         anim = &tileset.animated[i];
        }
       }
       if(anim)
       {
        double time_per_frame = 0.1;
        int frame_index = (int)(elapsed_time/time_per_frame) % anim->num_frames;
        tile_image_coord = tile_id_to_coord(tileset_image, tile_size, anim->frames[frame_index]+1);
       }

       AABB region;
       region.upper_left = tile_image_coord;
       region.lower_right = AddV2(region.upper_left, tile_size);

       draw_quad((DrawParams){true, tile_quad(cur_coord), tileset_image, region, WHITE, .y_coord_sorting = Y_COORD_IN_BACK});
      }
     }
    }
   }
  }
#endif

  assert(player != NULL);

  // gameplay processing loop, do multiple if lagging
  static Entity *interacting_with = 0; // used by rendering to figure out who to draw dialog box on
  const float dialog_interact_size = 2.5f * TILE_SIZE;
  int num_timestep_loops = 0;
  {
   unprocessed_gameplay_time += dt;
   float timestep = fminf(dt, (float)MINIMUM_TIMESTEP);
   while(unprocessed_gameplay_time >= timestep)
   {
    num_timestep_loops++;
    unprocessed_gameplay_time -= timestep;
    float dt = timestep;

    // process gs.entities
    PROFILE_SCOPE("entity processing")
    {
     ENTITIES_ITER(gs.entities)
     {
      assert(!(it->exists && it->generation == 0));
#ifdef WEB
      if(it->is_npc)
      {
       if(it->gen_request_id != 0)
       {
        assert(it->gen_request_id > 0);
        int status = EM_ASM_INT({
          return get_generation_request_status($0);
          }, it->gen_request_id);
        if(status == 0)
        {
         // simply not done yet
        }
        else
        {
         if(status == 1)
         {
          // done! we can get the string
          char sentence_str[MAX_SENTENCE_LENGTH] = {0};
          EM_ASM({
            let generation = get_generation_request_content($0);
            stringToUTF8(generation, $1, $2);
            }, it->gen_request_id, sentence_str, ARRLEN(sentence_str));


          // parse out from the sentence NPC action and dialog
          Perception out = {0};
          bool text_was_well_formatted = parse_ai_response(it, sentence_str, &out);

          if(text_was_well_formatted)
          {
           process_perception(it, out);
          }
          else
          {
           it->perceptions_dirty = true; // on poorly formatted AI, just retry request.
          }

          EM_ASM({
            done_with_generation_request($0);
            }, it->gen_request_id);
         }
         else if(status == 2)
         {
          Log("Failed to generate dialog! Fuck!\n");
          // need somethin better here. Maybe each sentence has to know if it's player or NPC, that way I can remove the player's dialog
          process_perception(it, (Perception){.type = NPCDialog, .npc_action_type = ACT_none, .npc_dialog = SENTENCE_CONST("I'm not sure...")});
         }
         else if(status == -1)
         {
          Log("Generation request doesn't exist anymore, that's fine...\n");
         }
         else
         {
          Log("Unknown generation request status: %d\n", status);
         }
         it->gen_request_id = 0;
        }
       }
      }
#endif
      if(fabsf(it->vel.x) > 0.01f)
       it->facing_left = it->vel.x < 0.0f;

      if(it->dead)
      {
       it->dead_time += dt;
      }

      if(it->is_npc)
      {
       // character speech animation text input
       if(true)
       {
        const float characters_per_sec = 35.0f;
        double before = it->characters_said;
        
        int length = 0;
        if(last_said_sentence(it)) length = last_said_sentence(it)->cur_index;
        if((int)before < length)
        {
         it->characters_said += characters_per_sec*dt;
        }
        else
        {
         it->characters_said = (double)length;
        }
        
        if( (int)it->characters_said > (int)before )
        {
         float dist = LenV2(SubV2(it->pos, player->pos));
         float volume = Lerp(-0.6f, clamp01(dist/70.0f), -1.0f);
         play_audio(&sound_simple_talk, volume);
        }
       }
       if(it->standing == STANDING_FIGHTING || it->standing == STANDING_JOINED)
       {
        Entity *targeting = player;

        Vec2 to_player = NormV2(SubV2(targeting->pos, it->pos));
        Vec2 rotate_direction;
        if(it->direction_of_spiral_pattern)
        {
         rotate_direction = rotate_counter_clockwise(to_player);
        }
        else
        {
         rotate_direction = rotate_clockwise(to_player);
        }
        Vec2 target_vel = NormV2(AddV2(rotate_direction, MulV2F(to_player, 0.5f)));
        target_vel = MulV2F(target_vel, 3.0f);
        it->vel = LerpV2(it->vel, 15.0f * dt, target_vel);
        CollisionInfo col = {0};
        it->pos = move_and_slide((MoveSlideParams){it, it->pos, MulV2F(it->vel, pixels_per_meter * dt), .col_info_out = &col});
        if(col.happened)
        {
         it->direction_of_spiral_pattern = !it->direction_of_spiral_pattern;
        }

        if(it->standing == STANDING_FIGHTING)
        {
         it->shotgun_timer += dt;
         Vec2 to_player = NormV2(SubV2(targeting->pos, it->pos));
         if(it->shotgun_timer >= 1.0f)
         {
          it->shotgun_timer = 0.0f;
          const float spread = (float)PI/4.0f;
          // shoot shotgun
          int num_bullets = 5;
          for(int i = 0; i < num_bullets; i++)
          {
           Vec2 dir = to_player;
           float theta = Lerp(-spread/2.0f, ((float)i / (float)(num_bullets - 1)), spread/2.0f);
           dir = RotateV2(dir, theta);
           Entity *new_bullet = new_entity();
           new_bullet->is_bullet = true;
           new_bullet->pos = AddV2(it->pos, MulV2F(dir, 20.0f));
           new_bullet->vel = MulV2F(dir, 15.0f);
           it->vel = AddV2(it->vel, MulV2F(dir, -3.0f));
          }
         }

        }
       }
       if(it->npc_kind == NPC_OldMan)
       {
        /*
           draw_dialog_panel(it);
           Entity *targeting = player;
           it->shotgun_timer += dt;
           Vec2 to_player = NormV2(SubV2(targeting->pos, it->pos));
           if(it->shotgun_timer >= 1.0f)
           {
           it->shotgun_timer = 0.0f;
           const float spread = (float)PI/4.0f;
        // shoot shotgun
        int num_bullets = 5;
        for(int i = 0; i < num_bullets; i++)
        {
        Vec2 dir = to_player;
        float theta = Lerp(-spread/2.0f, ((float)i / (float)(num_bullets - 1)), spread/2.0f);
        dir = RotateV2(dir, theta);
        Entity *new_bullet = new_entity();
        new_bullet->is_bullet = true;
        new_bullet->pos = AddV2(it->pos, MulV2F(dir, 20.0f));
        new_bullet->vel = MulV2F(dir, 15.0f);
        it->vel = AddV2(it->vel, MulV2F(dir, -3.0f));
        }
        }

        Vec2 target_vel = NormV2(AddV2(rotate_counter_clockwise(to_player), MulV2F(to_player, 0.5f)));
        target_vel = MulV2F(target_vel, 3.0f);
        it->vel = LerpV2(it->vel, 15.0f * dt, target_vel);
        it->pos = move_and_slide((MoveSlideParams){it, it->pos, MulV2F(it->vel, pixels_per_meter * dt)});
        */
       }

       else if(it->npc_kind == NPC_Skeleton)
       {
        if(it->dead)
        {
        }
        else
        {
         if(fabsf(it->vel.x) > 0.01f)
          it->facing_left = it->vel.x < 0.0f;

         it->pos = move_and_slide((MoveSlideParams){it, it->pos, MulV2F(it->vel, pixels_per_meter * dt)});
         AABB weapon_aabb = entity_sword_aabb(it, 30.0f, 18.0f);
         dbgrect(weapon_aabb);
         Vec2 target_vel = {0};
         it->pos = AddV2(it->pos, MulV2F(it->vel, dt));
         Overlapping overlapping_weapon = get_overlapping(cur_level, weapon_aabb);
         if(it->swing_timer > 0.0)
         {
          it->swing_timer += dt;
          if(it->swing_timer >= anim_sprite_duration(&skeleton_swing_sword))
          {
           it->swing_timer = 0.0;
          }
          if(it->swing_timer >= 0.4f)
          {
           SwordToDamage to_damage = entity_sword_to_do_damage(it, overlapping_weapon);
           Entity *from = it;
           BUFF_ITER(Entity *, &to_damage)
           {
            request_do_damage(*it, from, DAMAGE_SWORD);
           }
          }
         }
         else
         {
          // in huntin' range
          it->walking = LenV2(SubV2(player->pos, it->pos)) < 250.0f;
          if(it->walking)
          {
           Entity *skele = it;
           BUFF_ITER(Overlap, &overlapping_weapon)
           {
            if(it->e && it->e->is_character)
            {
             skele->swing_timer += dt;
             BUFF_CLEAR(&skele->done_damage_to_this_swing);
            }
           }
           target_vel = MulV2F(NormV2(SubV2(player->pos, it->pos)), 4.0f);
          }
          else
          {
          }
         }
         it->vel = LerpV2(it->vel, dt*8.0f, target_vel);
        }
       }
       else if(it->npc_kind == NPC_Death)
       {
       }
#if 0
       else if(it->npc_kind == DEATH)
       {
        draw_animated_sprite(&death_idle, elapsed_time, true, AddV2(it->pos, V2(0, 30.0f)), col);
       }
       else if(it->npc_kind == MERCHANT)
       {
        draw_animated_sprite(&merchant_idle, elapsed_time, true, AddV2(it->pos, V2(0, 30.0f)), col);
       }
#endif
       else if(it->npc_kind == NPC_Max)
       {
       }
       else if(it->npc_kind == NPC_Hunter)
       {
       }
       else if(it->npc_kind == NPC_John)
       {
       }
       else if(it->npc_kind == NPC_MOOSE)
       {
       }
       else if(it->npc_kind == NPC_GodRock)
       {
       }
       else if(it->npc_kind == NPC_Edeline)
       {
       }
       else if(it->npc_kind == NPC_Blocky)
       {
        if(it->moved)
        {
         it->walking = true;
         Vec2 towards = SubV2(it->target_goto, it->pos);
         if(LenV2(towards) > 1.0f)
         {
          it->pos = LerpV2(it->pos, dt*5.0f, it->target_goto);
         }
        }
        else
        {
         it->walking = false;
        }
       }
       else
       {
        assert(false);
       }
       if(it->damage >= 1.0)
       {
        if(it->npc_kind == NPC_Skeleton)
        {
         it->dead = true;
        }
        else
        {
         it->destroy = true;
        }
       }
      }
      else if (it->is_item)
      {
       if(it->held_by_player)
       {
        Vec2 held_spot = V2(15.0f * (player->facing_left ? -1.0f : 1.0f), 7.0f);
        it->pos = AddV2(player->pos, held_spot);
       }
       else
       {
        it->vel = LerpV2(it->vel, dt*7.0f, V2(0.0f,0.0f));
        CollisionInfo info = {0};
        it->pos = move_and_slide((MoveSlideParams){it, it->pos, MulV2F(it->vel, pixels_per_meter * dt), .dont_collide_with_entities = true, .col_info_out = &info});
        if(info.happened) it->vel = ReflectV2(it->vel, info.normal);
       }

       //draw_quad((DrawParams){true, it->pos, IMG(image_white_square)
      }
      else if (it->is_bullet)
      {
       it->pos = AddV2(it->pos, MulV2F(it->vel, pixels_per_meter * dt));
       dbgvec(it->pos, it->vel);
       Overlapping over = get_overlapping(cur_level, entity_aabb(it));
       Entity *from_bullet = it;
       bool destroy_bullet = false;
       BUFF_ITER(Overlap, &over) if(it->e != from_bullet)
       {
        if(!it->is_tile && !(it->e->is_bullet))
        {
         // knockback and damage
         request_do_damage(it->e, from_bullet, DAMAGE_BULLET);
         destroy_bullet = true;
        }
       }
       if(destroy_bullet) *from_bullet = (Entity){0};
       if(!has_point(level_aabb, it->pos)) *it = (Entity){0};
      }
      else if(it->is_character)
      {
      }
      else if(it->is_prop)
      {
      }
      else
      {
       assert(false);
      }
      }
     }

     PROFILE_SCOPE("Destroy gs.entities, maybe send generation requests")
     {
      ENTITIES_ITER(gs.entities)
      {
       if(it->destroy)
       {
        int gen = it->generation;
        *it = (Entity){0};
        it->generation = gen;
       }
       if(it->perceptions_dirty && !npc_does_dialog(it))
       {
        it->perceptions_dirty = false;
       }
       if(it->perceptions_dirty)
       {
        PromptBuff prompt = {0};
        generate_prompt(it, &prompt);
        Log("Sending request with prompt `%s`\n", prompt.data);

#ifdef WEB
        // fire off generation request, save id
        BUFF(char, 512) completion_server_url = {0};
        printf_buff(&completion_server_url, "%s/completion", SERVER_URL);
        int req_id = EM_ASM_INT({
          return make_generation_request(UTF8ToString($1), UTF8ToString($0));
          }, completion_server_url.data, prompt.data);
        it->gen_request_id = req_id;
#endif

#ifdef DESKTOP
        BUFF(char, 1024) mocked_ai_response = {0};
#define SAY(act, txt) { int index = action_to_index(it, act); printf_buff(&mocked_ai_response, " %d \"%s\"", index, txt); }
        if(it->npc_kind == NPC_Blocky)
        {
         if(it->last_seen_holding_kind == ITEM_Tripod && !it->moved)
         {
          SAY(ACT_allows_player_to_pass, "Here you go");
         }
         else
         {
          SAY(ACT_none, "You passed");
         }
        }
        else
        {
         //SAY(ACT_joins_player, "I am an NPC");
         SAY(ACT_none, "I am an NPC. Bla bla bl alb djsfklalfkdsaj. Did you know shortcake?");
        }
        Perception p = {0};
        assert(parse_ai_response(it, mocked_ai_response.data, &p));
        process_perception(it, p);
#undef SAY
#endif
        it->perceptions_dirty = false;
       }
      }
     }

     PROFILE_SCOPE("process player")
     {
      // do dialog
      Entity *closest_interact_with = 0;
      {
       // find closest to talk to
       {
        AABB dialog_rect = centered_aabb(player->pos, V2(dialog_interact_size , dialog_interact_size));
        dbgrect(dialog_rect);
        Overlapping possible_dialogs = get_overlapping(cur_level, dialog_rect);
        float closest_interact_with_dist = INFINITY;
        BUFF_ITER(Overlap, &possible_dialogs)
        {
         bool entity_talkable = true;
         if(entity_talkable) entity_talkable = entity_talkable && !it->is_tile;
         if(entity_talkable) entity_talkable = entity_talkable && it->e->is_npc;
         if(entity_talkable) entity_talkable = entity_talkable && !(it->e->npc_kind == NPC_Skeleton);
#ifdef WEB
         if(entity_talkable) entity_talkable = entity_talkable && it->e->gen_request_id == 0;
#endif

         bool entity_pickupable = !it->is_tile && !gete(player->holding_item) && it->e->is_item;

         if(entity_talkable || entity_pickupable)
         {
          float dist = LenV2(SubV2(it->e->pos, player->pos));
          if(dist < closest_interact_with_dist)
          {
           closest_interact_with_dist = dist;
           closest_interact_with = it->e;
          }
         }
        }
       }


       interacting_with = closest_interact_with;
       if(player->state == CHARACTER_TALKING)
       {
        interacting_with = gete(player->talking_to);
        assert(interacting_with);
       }


       // maybe get rid of talking to
       if(player->state == CHARACTER_TALKING)
       {
        if(gete(player->talking_to) == 0)
        {
         player->state = CHARACTER_IDLE;
        }
       }
      }

      if(interact)
      {
       if(closest_interact_with)
       {
        if(closest_interact_with->is_npc)
        {
         // begin dialog with closest npc
         player->state = CHARACTER_TALKING;
         player->talking_to = frome(closest_interact_with);
        }
        else if(closest_interact_with->is_item)
        {
         // pick up item
         closest_interact_with->held_by_player = true;
         player->holding_item = frome(closest_interact_with);
        }
        else
        {
         assert(false);
        }

       }
       else
       {
        if(gete(player->holding_item))
        {
         // throw item if not talking to somebody with item
         Entity *thrown = gete(player->holding_item);
         assert(thrown);
         thrown->vel = MulV2F(player->to_throw_direction, 20.0f);
         thrown->held_by_player = false;
         player->holding_item = (EntityRef){0};
        }
       }
      }
      if(roll && !player->is_rolling && player->time_not_rolling > 0.3f && (player->state == CHARACTER_IDLE || player->state == CHARACTER_WALKING))
      {
       player->is_rolling = true;
       player->roll_progress = 0.0;
      }

      if(attack && (player->state == CHARACTER_IDLE || player->state == CHARACTER_WALKING))
      {
       player->state = CHARACTER_ATTACK;
       BUFF_CLEAR(&player->done_damage_to_this_swing); 
       player->swing_progress = 0.0;
      }
      // roll processing
      {
       if(player->state != CHARACTER_IDLE && player->state != CHARACTER_WALKING)
       {
        player->roll_progress = 0.0;
        player->is_rolling = false;
       }
       if(player->is_rolling)
       {
        player->time_not_rolling = 0.0f;
        player->roll_progress += dt;
        if(player->roll_progress > anim_sprite_duration(&knight_rolling))
        {
         player->is_rolling = false;
        }
       }
       if(!player->is_rolling) player->time_not_rolling += dt;
      }

      Vec2 target_vel = {0};
      float speed = 0.0f;

      if(LenV2(movement) > 0.01f) player->to_throw_direction = NormV2(movement);
      if(player->state == CHARACTER_WALKING)
      {
       speed = PLAYER_SPEED;
       if(player->is_rolling) speed = PLAYER_ROLL_SPEED;

       if(gete(player->holding_item) && gete(player->holding_item)->item_kind == ITEM_Boots)
       {
        speed *= 2.0f;
       }

       if(LenV2(movement) == 0.0)
       {
        player->state = CHARACTER_IDLE;
       }
       else
       {
       }
      }
      else if(player->state == CHARACTER_IDLE)
      {
       if(LenV2(movement) > 0.01) player->state = CHARACTER_WALKING;
      }
      else if(player->state == CHARACTER_ATTACK)
      {
       AABB weapon_aabb = entity_sword_aabb(player, 40.0f, 25.0f);
       dbgrect(weapon_aabb);
       SwordToDamage to_damage = entity_sword_to_do_damage(player, get_overlapping(cur_level, weapon_aabb));
       BUFF_ITER(Entity*, &to_damage)
       {
        request_do_damage(*it, player, DAMAGE_SWORD);
       }
       player->swing_progress += dt;
       if(player->swing_progress > anim_sprite_duration(&knight_attack))
       {
        player->state = CHARACTER_IDLE;
       }
      }
      else if(player->state == CHARACTER_TALKING)
      {
      }
      else
      {
       assert(false); // unknown character state? not defined how to process
      }

      // velocity processing
      {
       Vec2 target_vel = MulV2F(movement, dt * pixels_per_meter * speed);
       player->vel = LerpV2(player->vel, dt * 15.0f, target_vel);
       player->pos = move_and_slide((MoveSlideParams){player, player->pos, player->vel});
      }
      // health
      if(player->damage >= 1.0)
      {
       reset_level();
      }
     }
     interact_just_pressed = false;
    } // while loop
   }

  PROFILE_SCOPE("render player")
  {
   Vec2 character_sprite_pos = AddV2(player->pos, V2(0.0, 20.0f));
   // if somebody, show their dialog panel
   if(interacting_with) 
   {
    // interaction keyboard hint
    if(!mobile_controls)
    {
     float size = 100.0f;
     Vec2 midpoint = MulV2F(AddV2(interacting_with->pos, player->pos), 0.5f);
     draw_quad((DrawParams){true, quad_centered(AddV2(midpoint, V2(0.0, 5.0f + sinf((float)elapsed_time*3.0f)*5.0f)), V2(size, size)), IMG(image_e_icon), blendalpha(WHITE, clamp01(1.0f - learned_e)), .y_coord_sorting = Y_COORD_IN_FRONT, .queue_for_translucent = true});
    }

    // interaction circle
    draw_quad((DrawParams){true, quad_centered(interacting_with->pos, V2(TILE_SIZE, TILE_SIZE)), image_dialog_circle, full_region(image_dialog_circle), WHITE});
   }

   if(player->state == CHARACTER_WALKING)
   {
    if(player->is_rolling)
    {
     draw_animated_sprite(&knight_rolling, player->roll_progress, player->facing_left, character_sprite_pos, WHITE);
    }
    else
    {
     draw_animated_sprite(&knight_running, elapsed_time, player->facing_left, character_sprite_pos, WHITE);
    }
   }
   else if(player->state == CHARACTER_IDLE)
   {
    if(player->is_rolling)
    {
     draw_animated_sprite(&knight_rolling, player->roll_progress, player->facing_left, character_sprite_pos, WHITE);
    }
    else
    {
     draw_animated_sprite(&knight_idle, elapsed_time, player->facing_left, character_sprite_pos, WHITE);
    }
   }
   else if(player->state == CHARACTER_ATTACK)
   {
    draw_animated_sprite(&knight_attack, player->swing_progress, player->facing_left, character_sprite_pos, WHITE);
   }
   else if(player->state == CHARACTER_TALKING)
   {
    draw_animated_sprite(&knight_idle, elapsed_time, player->facing_left, character_sprite_pos, WHITE);
   }
   else
   {
    assert(false); // unknown character state? not defined how to draw
   }

   // hurt vignette
   if(player->damage > 0.0)
   {
    draw_quad((DrawParams){false, (Quad){.ul=V2(0.0f, screen_size().Y), .ur = screen_size(), .lr = V2(screen_size().X, 0.0f)}, image_hurt_vignette, full_region(image_hurt_vignette), (Color){1.0f, 1.0f, 1.0f, player->damage}, .y_coord_sorting = Y_COORD_IN_FRONT, .queue_for_translucent = true});
   }
  }

  // render gs.entities render entities
  PROFILE_SCOPE("entity rendering")
  ENTITIES_ITER(gs.entities)
  {
#ifdef WEB
   if(it->gen_request_id != 0)
   {
    draw_quad((DrawParams){true, quad_centered(AddV2(it->pos, V2(0.0, 50.0)), V2(100.0,100.0)), IMG(image_thinking), WHITE});
   }
#endif

   // draw drop shadow
   //if(it->is_character || it->is_npc || it->is_prop)
   if(false)
   {
    //if(it->npc_kind != DEATH)
    {
     float shadow_size = knight_rolling.region_size.x * 0.5f;
     Vec2 shadow_offset = V2(0.0f, -20.0f);
     if(npc_is_knight_sprite(it))
     {
      shadow_offset = V2(0.5f, -10.0f);
     }
#if 0
     if(it->npc_kind == MERCHANT)
     {
      shadow_offset = V2(-4.5f, -15.0f);
     }
     else if(it->npc_kind == OLD_MAN)
     {
      shadow_offset = V2(-1.5f, -8.0f);
      shadow_size *= 0.5f;
     }
#endif
     if(it->is_prop)
     {
      shadow_size *= 2.5f;
      shadow_offset = V2(-5.0f, -8.0f);
     }
     draw_quad((DrawParams){true, quad_centered(AddV2(it->pos, shadow_offset), V2(shadow_size, shadow_size)),IMG(image_drop_shadow), (Color){1.0f,1.0f,1.0f,0.5f}});
    }
   }

   Color col = LerpV4(WHITE, it->damage, RED);
   if(it->is_npc)
   {
    if(it->is_npc)
    {
     float dist = LenV2(SubV2(it->pos, player->pos));
     dist -= 10.0f; // radius around point where dialog is completely opaque
     float max_dist = dialog_interact_size/2.0f;
     float alpha = 1.0f - (float)clamp(dist/max_dist, 0.0, 1.0);
     if(gete(player->talking_to) == it && player->state == CHARACTER_TALKING) alpha = 1.0f;
     draw_dialog_panel(it, alpha);
    }

    if(it->npc_kind == NPC_OldMan)
    {
     bool face_left =SubV2(player->pos, it->pos).x < 0.0f;
     draw_animated_sprite(&old_man_idle, elapsed_time, face_left, it->pos, col);
    }
    else if(it->npc_kind == NPC_Skeleton)
    {
     Color col = WHITE;
     if(it->dead)
     {
      draw_animated_sprite(&skeleton_die, it->dead_time, it->facing_left, it->pos, col);
     }
     else
     {
      if(it->swing_timer > 0.0)
      {
       // swinging sword
       draw_animated_sprite(&skeleton_swing_sword, it->swing_timer, it->facing_left, it->pos, col);
      }
      else
      {
       if(it->walking)
       {
        draw_animated_sprite(&skeleton_run, elapsed_time, it->facing_left, it->pos, col);
       }
       else
       {
        draw_animated_sprite(&skeleton_idle, elapsed_time, it->facing_left, it->pos, col);
       }
      }
     }
    }
    else if(it->npc_kind == NPC_Death)
    {
     draw_animated_sprite(&death_idle, elapsed_time, true, AddV2(it->pos, V2(0, 30.0f)), col);
    }
    else if(it->npc_kind == NPC_GodRock)
    {
     Vec2 prop_size = V2(46.0f, 40.0f);
     DrawParams d = (DrawParams){true, quad_centered(AddV2(it->pos, V2(-0.0f, 0.0)), prop_size), image_props_atlas, aabb_at_yplusdown(V2(15.0f, 219.0f), prop_size), WHITE, .y_coord_sorting = y_coord_sorting_at(AddV2(it->pos, V2(0.0f, 20.0f))), .alpha_clip_threshold = 0.7f};
     draw_shadow_for(d);
     draw_quad(d);
    }
    else if(npc_is_knight_sprite(it))
    {
     Color tint = WHITE;
     if(it->npc_kind == NPC_Max)
     {
      tint = colhex(0xfc8803);
     }
     else if(it->npc_kind == NPC_Hunter)
     {
      tint = colhex(0x4ac918);
     }
     else if(it->npc_kind == NPC_Blocky)
     {
      tint = colhex(0xa84032);
     }
     else if(it->npc_kind == NPC_John)
     {
      tint = colhex(0x16c7a1);
     }
     else if(it->npc_kind == NPC_Edeline)
     {
      tint = colhex(0x8c34eb);
     }
     else
     {
      assert(false);
     }
     draw_animated_sprite(&knight_idle, elapsed_time, true, AddV2(it->pos, V2(0, 30.0f)), tint);
    }
    else if(it->npc_kind == NPC_MOOSE)
    {
     //draw_animated_sprite(&moose_idle, elapsed_time, true, AddV2(it->pos, V2(0, 30.0f)), col);
    }
    else
    {
     assert(false);
    }
   }
   else if (it->is_item)
   {
    Quad drawn = quad_centered(it->pos, V2(15.0f, 15.0f));
    if(it->item_kind == ITEM_Tripod)
    {
     draw_quad((DrawParams){true, drawn, IMG(image_tripod), WHITE});
    }
    else if(it->item_kind == ITEM_Boots)
    {
     draw_quad((DrawParams){true, drawn, IMG(image_boots), WHITE});
    }
    else if(it->item_kind == ITEM_WhiteSquare)
    {
     colorquad(true, drawn, WHITE);
    }
    else
    {
     assert(false);
    }
   }
   else if (it->is_bullet)
   {
    AABB normal_aabb = entity_aabb(it);
    Quad drawn = quad_centered(aabb_center(normal_aabb), MulV2F(aabb_size(normal_aabb), 1.5f));
    draw_quad((DrawParams){true, drawn, IMG(image_bullet), WHITE});
   }
   else if(it->is_character)
   {
   }
   else if(it->is_prop)
   {
    DrawParams d = {0};
    if(it->prop_kind == TREE0)
    {
     Vec2 prop_size = V2(74.0f, 122.0f);
     d = (DrawParams){true, quad_centered(AddV2(it->pos, V2(-5.0f, 45.0)), prop_size), image_props_atlas, aabb_at_yplusdown(V2(2.0f, 4.0f), prop_size), WHITE, .y_coord_sorting = y_coord_sorting_at(AddV2(it->pos, V2(0.0f, 20.0f))), .alpha_clip_threshold = 0.7f};
    }
    else if(it->prop_kind == TREE1)
    {
     Vec2 prop_size = V2(94.0f, 120.0f);
     d = ((DrawParams){true, quad_centered(AddV2(it->pos, V2(-4.0f, 55.0)), prop_size), image_props_atlas, aabb_at_yplusdown(V2(105.0f, 4.0f), prop_size), WHITE, .y_coord_sorting = y_coord_sorting_at(AddV2(it->pos, V2(0.0f, 20.0f))), .alpha_clip_threshold = 0.4f});
    }
    else if(it->prop_kind == TREE2)
    {
     Vec2 prop_size = V2(128.0f, 192.0f);
     d = ((DrawParams){true, quad_centered(AddV2(it->pos, V2(-2.5f, 70.0)), prop_size), image_props_atlas, aabb_at_yplusdown(V2(385.0f, 479.0f), prop_size), WHITE, .y_coord_sorting = y_coord_sorting_at(AddV2(it->pos, V2(0.0f, 20.0f))), .alpha_clip_threshold = 0.4f});
    }
    else if(it->prop_kind == ROCK0)
    {
     Vec2 prop_size = V2(30.0f, 22.0f);
     d = (DrawParams){true, quad_centered(AddV2(it->pos, V2(0.0f, 25.0)), prop_size), image_props_atlas, aabb_at_yplusdown(V2(66.0f, 235.0f), prop_size), WHITE, .y_coord_sorting = y_coord_sorting_at(AddV2(it->pos, V2(0.0f, 0.0f))), .alpha_clip_threshold = 0.7f};
    }
    else
    {
     assert(false);
    }
    draw_shadow_for(d);
    draw_quad(d);
   }
   else
   {
    assert(false);
   }
  }
   
   // translucent
  draw_all_translucent();

  // ui
#define HELPER_SIZE 250.0f
  if(!mobile_controls)
  {
   float total_height = HELPER_SIZE * 2.0f;
   float vertical_spacing = HELPER_SIZE/2.0f;
   total_height -= (total_height - (vertical_spacing + HELPER_SIZE));
   const float padding = 50.0f;
   float y = screen_size().y/2.0f + total_height/2.0f;
   draw_quad((DrawParams){false, quad_at(V2(padding, y), V2(HELPER_SIZE,HELPER_SIZE)), IMG(image_shift_icon), (Color){1.0f,1.0f,1.0f,fmaxf(0.0f, 1.0f-learned_shift)}, .y_coord_sorting = Y_COORD_IN_FRONT});
   y -= vertical_spacing;
   draw_quad((DrawParams){false, quad_at(V2(padding, y), V2(HELPER_SIZE,HELPER_SIZE)), IMG(image_space_icon), (Color){1.0f,1.0f,1.0f,fmaxf(0.0f, 1.0f-learned_space)}, .y_coord_sorting = Y_COORD_IN_FRONT});
  }


  if(mobile_controls)
  {
   float thumbstick_nub_size = (img_size(image_mobile_thumbstick_nub).x / img_size(image_mobile_thumbstick_base).x) * thumbstick_base_size();
   draw_quad((DrawParams){false, quad_centered(thumbstick_base_pos, V2(thumbstick_base_size(), thumbstick_base_size())), IMG(image_mobile_thumbstick_base), WHITE, .y_coord_sorting = Y_COORD_IN_FRONT});
   draw_quad((DrawParams){false, quad_centered(thumbstick_nub_pos, V2(thumbstick_nub_size, thumbstick_nub_size)), IMG(image_mobile_thumbstick_nub), WHITE, .y_coord_sorting = Y_COORD_IN_FRONT});

   if(interacting_with || gete(player->holding_item))
   {
    draw_quad((DrawParams){false, quad_centered(interact_button_pos(), V2(mobile_button_size(), mobile_button_size())), IMG(image_mobile_button), WHITE, .y_coord_sorting = Y_COORD_IN_FRONT});
   }
   draw_quad((DrawParams){false, quad_centered(roll_button_pos(), V2(mobile_button_size(), mobile_button_size())), IMG(image_mobile_button), WHITE, .y_coord_sorting = Y_COORD_IN_FRONT});
   draw_quad((DrawParams){false, quad_centered(attack_button_pos(), V2(mobile_button_size(), mobile_button_size())), IMG(image_mobile_button), WHITE, .y_coord_sorting = Y_COORD_IN_FRONT});
  }

#ifdef DEVTOOLS
  dbgsquare(screen_to_world(mouse_pos));

  // tile coord
  if(show_devtools)
  {
   TileCoord hovering = world_to_tilecoord(screen_to_world(mouse_pos));
   Vec2 points[4] ={0};
   AABB q = tile_aabb(hovering);
   dbgrect(q);
   draw_text((TextParams){false, false, tprint("%d", get_tile(&level_level0, hovering).kind), world_to_screen(tilecoord_to_world(hovering)), BLACK, 1.0f});
  }

  // debug draw font image
  {
   draw_quad((DrawParams){true, quad_centered(V2(0.0, 0.0), V2(250.0, 250.0)), image_font,full_region(image_font), WHITE});
  }

  // statistics
  if(show_devtools)
  PROFILE_SCOPE("statistics")
  {
   Vec2 pos = V2(0.0, screen_size().Y);
   int num_entities = 0;
   ENTITIES_ITER(gs.entities) num_entities++;
   char *stats = tprint("Frametime: %.1f ms\nProcessing: %.1f ms\nEntities: %d\nDraw calls: %d\nProfiling: %s\nNumber gameplay processing loops: %d\n", dt*1000.0, last_frame_processing_time*1000.0, num_entities, num_draw_calls, profiling ? "yes" : "no", num_timestep_loops);
   AABB bounds = draw_text((TextParams){false, true, stats, pos, BLACK, 1.0f});
   pos.Y -= bounds.upper_left.Y - screen_size().Y;
   bounds = draw_text((TextParams){false, true, stats, pos, BLACK, 1.0f});
   // background panel
   colorquad(false, quad_aabb(bounds), (Color){1.0, 1.0, 1.0, 0.3f});
   draw_text((TextParams){false, false, stats, pos, BLACK, 1.0f});
   num_draw_calls = 0;
  }
#endif // devtools


  // update camera position
  {
   Vec2 target = MulV2F(player->pos, -1.0f * cam.scale);
   if(LenV2(SubV2(target, cam.pos)) <= 0.2)
   {
    cam.pos = target;
   }
   else
   {
    cam.pos = LerpV2(cam.pos, dt*8.0f, target);
   }
  }

  PROFILE_SCOPE("flush rendering")
  {
   flush_quad_batch();
   sg_end_pass();
   sg_commit();
  }

  last_frame_processing_time = stm_sec(stm_diff(stm_now(),time_start_frame));

  reset(&scratch);
 }
}

void cleanup(void)
{
 sg_shutdown();
 Log("Cleaning up\n");
}

void event(const sapp_event *e)
{
 if(e->key_repeat) return;
 if(e->type == SAPP_EVENTTYPE_TOUCHES_BEGAN)
 {
  if(!mobile_controls)
  {
   thumbstick_base_pos = V2(screen_size().x * 0.25f, screen_size().y * 0.25f);
   thumbstick_nub_pos = thumbstick_base_pos;
  }
  mobile_controls = true;
 }

#ifdef DESKTOP
 // the desktop text backend, for debugging purposes
 if(receiving_text_input)
 {
  if(e->type == SAPP_EVENTTYPE_CHAR)
  {
   if(BUFF_HAS_SPACE(&text_input_buffer))
   {
    BUFF_APPEND(&text_input_buffer, (char)e->char_code);
   }
  }
  if(e->type == SAPP_EVENTTYPE_KEY_DOWN && e->key_code == SAPP_KEYCODE_ENTER)
  {
   receiving_text_input = false;
   end_text_input(text_input_buffer.data);
  }
 }
#endif
 
 // mobile handling touch controls handling touch input
 if(mobile_controls)
 {
  if(e->type == SAPP_EVENTTYPE_TOUCHES_BEGAN)
  {
#define TOUCHPOINT_SCREEN(point) V2(point.pos_x, screen_size().y - point.pos_y)
   for(int i = 0; i < e->num_touches; i++)
   {
    sapp_touchpoint point = e->touches[i];
    Vec2 touchpoint_screen_pos = TOUCHPOINT_SCREEN(point);
    if(touchpoint_screen_pos.x < screen_size().x*0.4f)
    {
     if(!movement_touch.active)
     {
      //if(LenV2(SubV2(touchpoint_screen_pos, thumbstick_base_pos)) > 1.25f * thumbstick_base_size())
      if(true)
      {
       thumbstick_base_pos = touchpoint_screen_pos;
      }
      movement_touch = activate(point.identifier);
      thumbstick_nub_pos = thumbstick_base_pos;
     }
    }
    if(LenV2(SubV2(touchpoint_screen_pos, roll_button_pos())) < mobile_button_size()*0.5f)
    {
     roll_pressed_by = activate(point.identifier);
     mobile_roll_pressed = true;
    }
    if(LenV2(SubV2(touchpoint_screen_pos, interact_button_pos())) < mobile_button_size()*0.5f)
    {
     interact_pressed_by = activate(point.identifier);
     mobile_interact_pressed = true;
     interact_just_pressed = true;
    }
    if(LenV2(SubV2(touchpoint_screen_pos, attack_button_pos())) < mobile_button_size()*0.5f)
    {
     attack_pressed_by = activate(point.identifier);
     mobile_attack_pressed = true;
    }
   }
  }
  if(e->type == SAPP_EVENTTYPE_TOUCHES_MOVED)
  {
   for(int i = 0; i < e->num_touches; i++)
   {
    if(movement_touch.active)
    {
     if(e->touches[i].identifier == movement_touch.identifier)
     {
      thumbstick_nub_pos = TOUCHPOINT_SCREEN(e->touches[i]);
      Vec2 move_vec = SubV2(thumbstick_nub_pos, thumbstick_base_pos);
      float clampto_size = thumbstick_base_size()/2.0f;
      if(LenV2(move_vec) > clampto_size)
      {
       thumbstick_nub_pos = AddV2(thumbstick_base_pos, MulV2F(NormV2(move_vec), clampto_size));
      }
     }
    }
   }
  }
  if(e->type == SAPP_EVENTTYPE_TOUCHES_ENDED)
  {
   for(int i = 0; i < e->num_touches; i++)
   if(e->touches[i].changed) // only some of the touch events are released
   {
    if(maybe_deactivate(&interact_pressed_by, e->touches[i].identifier))
    {
     mobile_interact_pressed = false;
    }
    if(maybe_deactivate(&roll_pressed_by, e->touches[i].identifier))
    {
     mobile_roll_pressed = false;
    }
    if(maybe_deactivate(&attack_pressed_by, e->touches[i].identifier))
    {
     mobile_attack_pressed = false;
    }
    if(maybe_deactivate(&movement_touch, e->touches[i].identifier))
    {
     thumbstick_nub_pos = thumbstick_base_pos;
    }
   }
  }
 }

 if(e->type == SAPP_EVENTTYPE_KEY_DOWN)
 {
  mobile_controls = false;
  assert(e->key_code < sizeof(keydown)/sizeof(*keydown));
  keydown[e->key_code] = true;
  
  if(e->key_code == SAPP_KEYCODE_E)
  {
   interact_just_pressed = true;
  }

  if(e->key_code == SAPP_KEYCODE_LEFT_SHIFT)
  {
   learned_shift += 0.15f;
  }
  if(e->key_code == SAPP_KEYCODE_SPACE)
  {
   learned_space += 0.15f;
  }
  if(e->key_code == SAPP_KEYCODE_E)
  {
   learned_e += 0.15f;
  }
#ifdef DESKTOP // very nice for my run from cmdline workflow, escape to quit
  if(e->key_code == SAPP_KEYCODE_ESCAPE)
  {
   sapp_quit();
  }
#endif
#ifdef DEVTOOLS
  if(e->key_code == SAPP_KEYCODE_T)
  {
   mouse_frozen = !mouse_frozen;
  }
  if(e->key_code == SAPP_KEYCODE_M)
  {
   mobile_controls = true;
  }
  if(e->key_code == SAPP_KEYCODE_P)
  {
   profiling = !profiling;
   if(profiling)
   {
    init_profiling("rpgpt.spall");
    init_profiling_mythread(0);
   }
   else
   {
    end_profiling_mythread();
    end_profiling();
   }
  }
  if(e->key_code == SAPP_KEYCODE_7)
  {
   show_devtools = !show_devtools;
  }
#endif
 }
 if(e->type == SAPP_EVENTTYPE_KEY_UP)
 {
  keydown[e->key_code] = false;
 }
 if(e->type == SAPP_EVENTTYPE_MOUSE_MOVE)
 {
  bool ignore_movement = false;
#ifdef DEVTOOLS
  if(mouse_frozen) ignore_movement = true;
#endif
  if(!ignore_movement) mouse_pos = V2(e->mouse_x, (float)sapp_height() - e->mouse_y);
 }
}

sapp_desc sokol_main(int argc, char* argv[])
{
 (void)argc; (void)argv;
 return (sapp_desc){
  .init_cb = init,
   .frame_cb = frame,
   .cleanup_cb = cleanup,
   .event_cb = event,
   .width = 800,
   .height = 600,
   //.gl_force_gles2 = true, not sure why this was here in example, look into
   .window_title = "RPGPT",
   .win32_console_attach = true,
   .win32_console_create = true,
   .icon.sokol_default = true,
 };
}
