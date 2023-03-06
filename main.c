#define SOKOL_IMPL
#if defined(WIN32) || defined(_WIN32)
#define DESKTOP
#define SOKOL_D3D11
#endif

#if defined(__EMSCRIPTEN__)
#define WEB
#define SOKOL_GLES2
#endif

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "sokol_glue.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include "HandmadeMath.h"

#pragma warning(disable : 4996) // fopen is safe. I don't care about fopen_s

#include <math.h>

#define PROFILING_IMPL
#ifdef DEVTOOLS
#ifdef DESKTOP
#define PROFILING
#endif
#endif
#include "profiling.h"

#define ARRLEN(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))
#define ENTITIES_ITER(ents) for(Entity *it = ents; it < ents + ARRLEN(ents); it++) if(it->exists)

#define Log(...) { printf("Log %d | ", __LINE__); printf(__VA_ARGS__); }

// so can be grep'd and removed
#define dbgprint(...) { printf("Debug | %s:%d | ", __FILE__, __LINE__); printf(__VA_ARGS__); }
Vec2 RotateV2(Vec2 v, float theta)
{
  return V2( 
      v.X * cosf(theta) - v.Y * sinf(theta),
      v.X * sinf(theta) + v.Y * cosf(theta)
    );
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
 float horizontal_diff_btwn_frames;
 Vec2 region_size;
 bool no_wrap; // does not wrap when playing
} AnimatedSprite;

typedef enum CharacterState
{
 CHARACTER_WALKING,
 CHARACTER_IDLE,
 CHARACTER_ATTACK,
 CHARACTER_TALKING,
} CharacterState;

typedef enum EntityKind
{
 ENTITY_INVALID, // zero initialized is invalid entity

 ENTITY_PLAYER,
 ENTITY_OLD_MAN,
 ENTITY_BULLET,
} EntityKind;

#ifdef DEVTOOLS
#define SERVER_URL "http://localhost:8090"
#else
#define SERVER_URL "https://rpgpt.duckdns.org/"
#endif

// null terminator always built into buffers so can read properly from data
#define BUFF_VALID(buff_ptr) assert((buff_ptr)->cur_index <= ARRLEN((buff_ptr)->data))
#define BUFF(type, max_size) struct { int cur_index; type data[max_size]; char null_terminator; }
#define BUFF_HAS_SPACE(buff_ptr) ( (buff_ptr)->cur_index < ARRLEN((buff_ptr)->data) )
#define BUFF_EMPTY(buff_ptr) ((buff_ptr)->cur_index == 0)
#define BUFF_APPEND(buff_ptr, element)  { (buff_ptr)->data[(buff_ptr)->cur_index++] = element; BUFF_VALID(buff_ptr); }
//#define BUFF_ITER(type, buff_ptr) for(type *it = &((buff_ptr)->data[0]); it < (buff_ptr)->data + (buff_ptr)->cur_index; it++)
#define BUFF_ITER_EX(type, buff_ptr, begin_ind, cond, movement) for(type *it = &((buff_ptr)->data[begin_ind]); cond; movement)
#define BUFF_ITER(type, buff_ptr) BUFF_ITER_EX(type, (buff_ptr), 0, it < (buff_ptr)->data + (buff_ptr)->cur_index, it++)
#define BUFF_PUSH_FRONT(buff_ptr, value) { (buff_ptr)->cur_index++; BUFF_VALID(buff_ptr); for(int i = (buff_ptr)->cur_index - 1; i > 0; i--) { (buff_ptr)->data[i] = (buff_ptr)->data[i - 1]; }; (buff_ptr)->data[0] = value; }
#define BUFF_REMOVE_FRONT(buff_ptr) {if((buff_ptr)->cur_index > 0) {for(int i = 0; i < (buff_ptr)->cur_index - 1; i++) { (buff_ptr)->data[i] = (buff_ptr)->data[i+1]; }; (buff_ptr)->cur_index--;}}
#define BUFF_CLEAR(buff_ptr) {memset((buff_ptr), 0, sizeof(*(buff_ptr)));  ((buff_ptr)->cur_index = 0);}

// REFACTORING:: also have to update in javascript!!!!!!!!
#define MAX_SENTENCE_LENGTH 400 // LOOOK AT AGBOVE COMMENT GBEFORE CHANGING
typedef BUFF(char, MAX_SENTENCE_LENGTH) Sentence;
#define SENTENCE_CONST(txt) (Sentence){.data=txt, .cur_index=sizeof(txt)}


// even indexed dialogs (0,2,4) are player saying stuff, odds are the character from GPT
typedef BUFF(Sentence, 2*12) Dialog; // six back and forths. must be even number or bad things happen (I think)

typedef enum NpcKind
{
 OLD_MAN,
 DEATH,
} NpcKind;

typedef struct Entity
{
 bool exists;

 // fields for all entities
 Vec2 pos;
 Vec2 vel; // only used sometimes, like in old man and bullet
 float damage; // at 1.0, dead! zero initialized
 bool facing_left;

 bool is_bullet;

 // npcs
 bool is_npc;
 double character_say_timer;
 NpcKind npc_kind;
 Sentence sentence_to_say;
 Dialog player_dialog;
#ifdef WEB
 int gen_request_id;
#endif
 bool aggressive;
 double shotgun_timer;
 // only for death npc
 bool going_to_target;
 Vec2 target_goto;

 // character
 bool is_character;
 CharacterState state;
 struct Entity *talking_to; // Maybe should be generational index, but I dunno. No death yet
 bool is_rolling; // can only roll in idle or walk states
 float speed; // for lerping to the speed, so that roll gives speed boost which fades
 double time_not_rolling; // for cooldown for roll, so you can't just hold it and be invincible
 double roll_progress;
 double swing_progress;
} Entity;

typedef struct Overlap
{
 bool is_tile; // in which case e will be null, naturally
 TileInstance t;
 Entity *e;
} Overlap;


typedef BUFF(Overlap, 16) Overlapping;

#define LEVEL_TILES 150
#define LAYERS 2
#define TILE_SIZE 32 // in pixels
#define MAX_ENTITIES 128
#define PLAYER_SPEED 3.5f // in meters per second
#define PLAYER_ROLL_SPEED 7.0f
typedef struct Level
{
 TileInstance tiles[LAYERS][LEVEL_TILES][LEVEL_TILES];
 Entity initial_entities[MAX_ENTITIES]; // shouldn't be directly modified, only used to initialize entities on loading of level
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

void make_space_and_append(Dialog *d, Sentence *s)
{
 if(d->cur_index >= ARRLEN(d->data))
 {
  for(int remove_i = 0; remove_i < 2; remove_i++)
  {
   assert(ARRLEN(d->data) >= 1);
   for(int i = 0; i < ARRLEN(d->data) - 1; i++)
   {
    d->data[i] = d->data[i + 1];
   }
   d->cur_index--;
  }
 }

 BUFF_APPEND(d, *s);
}

void say_characters(Entity *npc, int num_characters)
{
 Sentence *sentence_to_append_to = &npc->player_dialog.data[npc->player_dialog.cur_index-1];
 for(int i = 0; i < num_characters; i++)
 {
  if(!BUFF_EMPTY(&npc->player_dialog) && !BUFF_EMPTY(&npc->sentence_to_say))
  {
   char new_character = npc->sentence_to_say.data[0];
   bool found_matching_star = false;
   BUFF(char, MAX_SENTENCE_LENGTH) match_buffer = {0};
   if(new_character == '*')
   {
    for(int ii = sentence_to_append_to->cur_index-1; ii >= 0; ii--)
    {
     if(sentence_to_append_to->data[ii] == '*')
     {
      found_matching_star = true;
      break;
     }
     BUFF_PUSH_FRONT(&match_buffer, sentence_to_append_to->data[ii]);
    }
   }
   if(found_matching_star)
   {
    if(strcmp(match_buffer.data, "fights player") == 0 && npc->npc_kind == OLD_MAN)
    {
     npc->aggressive = true;
    }
    if(strcmp(match_buffer.data, "moves") == 0 && npc->npc_kind == DEATH)
    {
     npc->going_to_target = true;
     npc->target_goto = AddV2(npc->pos, V2(0.0, -TILE_SIZE*1.5f));
    }
   }
   BUFF_APPEND(sentence_to_append_to, new_character);
   BUFF_REMOVE_FRONT(&npc->sentence_to_say);
  }
 }
}

void add_new_npc_sentence(Entity *npc, char *sentence)
{
 size_t sentence_len = strlen(sentence);
 assert(sentence_len < MAX_SENTENCE_LENGTH);
 Sentence new_sentence = {0};
 bool inside_star = false;
 for(int i = 0; i < sentence_len; i++)
 {
  if(sentence[i] == '"') break;
  if(sentence[i] == '\n') continue;
  BUFF_APPEND(&new_sentence, sentence[i]);
 }
 Sentence empty_sentence = {0};
 say_characters(npc, npc->sentence_to_say.cur_index);
 make_space_and_append(&npc->player_dialog, &empty_sentence);
 npc->sentence_to_say = new_sentence;
}

// from_point is for knockback
void request_do_damage(Entity *to, Vec2 from_point, float damage)
{
 if(to == NULL) return;
 if(to->npc_kind != DEATH)
 {
  to->damage += damage;
  to->aggressive = true;
  to->vel = MulV2F(NormV2(SubV2(to->pos, from_point)), 5.0f);
 }
}

#include "prompts.gen.h"

void begin_text_input(); // called when player engages in dialog, must say something and fill text_input_buffer
// a callback, when 'text backend' has finished making text
void end_text_input(char *what_player_said)
{
 player->state = CHARACTER_IDLE;
#ifdef WEB // hacky
 _sapp_emsc_register_eventhandlers();
#endif

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

  Dialog *to_append = &player->talking_to->player_dialog;
  make_space_and_append(to_append, &what_player_said_sentence);

  // the npc response will be appended here, or at least be async queued to be appended here
  BUFF(char, 4000) prompt_buff = {0};
  BUFF(char *, 100) to_join = {0};

  //BUFF_APPEND(&to_join, "This is dialog which takes place in a simple action RPG, where the player can only talk to NPCs, or fight. The characters influence the game world by saying specific actions from these possibilities: [*fights player*]. They don't say anything else that has '*' between them. Example dialog with an Old Man NPC:\nPlayer: \"Hello old man. Do you know that you're in a video game?\"\nOld Man: \"What are you talking about, young boy? What is a 'video game'?\"\nPlayer: \"You have no idea. You look ugly and stupid.\"\nOld Man: \"How juvenile! That's it, *fights player*\"\n\nThe NPCs exist on a small lush island, on a remote village, in a fantasy setting where monsters roam freely, posing a danger to the NPCs, and the player. They don't know about modern technology. They are very slow to say *fights player*, because doing so means killing the player, their friends, and potentially themselves. But if the situation demands it, they will not hesitate to open fire.\n");


  // characters prompt
  Entity *talking = player->talking_to;
  char *character_prompt = NULL;
  Log("doing prompt\n");
  if(talking->npc_kind == OLD_MAN)
  {
   BUFF_APPEND(&to_join, PROMPT_OLD_MAN);
   Log("doing prompt old man\n");
   character_prompt = "Old Man: \"";
  }
  else if(talking->npc_kind == DEATH)
  {
   BUFF_APPEND(&to_join, PROMPT_DEATH);
   Log("Doing prompt death\n");
   character_prompt = "Death: \"";
  }
  else
  {
   assert(false);
  }
  //BUFF_APPEND(&to_join, "The player is talking to an old man who is standing around on the island. He's eager to bestow his wisdom upon the young player, but the player must act polite, not rude. If the player acts rude, the old man will say exactly the text '*fights player*' as shown in the above example, turning the interaction into a skirmish, where the old man takes out his well concealed shotgun. The old man is also a bit of a joker.\n\n");
  //BUFF_APPEND(&to_join, "Dialog between an old man and a player in a video game. The player can only attack or talk in the game. The old man can perform these actions by saying them: [*fights player*]\n--\nPlayer: \"Who are you?\"\nOld Man: \"Why I'm just a simple old man, minding my business. What brings you here?\"\nPlayer: \"I'm not sure. What needs doing?\"\nOld Man: \"Nothing much. It's pretty boring around here. Monsters are threatening our village though.\"\nPlayer: \"Holy shit! I better get to it\"\nOld Man: \"He he, certainly! Good luck!\"\n--\nPlayer: \"Man fuck you old man\"\nOld Man: \"You better watch your tongue young man. Unless you're polite I'll be forced to attack you, peace is important around here!\"\nPlayer: \"Man fuck your peace\"\nOld Man: \"That's it! *fights player*\"\n--\n");

  // all the dialog
  int i = 0;
  BUFF_ITER(Sentence, &player->talking_to->player_dialog)
  {
   bool is_player = i % 2 == 0;
   if(is_player)
   {
    BUFF_APPEND(&to_join, "Player: \"");
   }
   else
   {
    BUFF_APPEND(&to_join, character_prompt);
   }
   BUFF_APPEND(&to_join, it->data);
   BUFF_APPEND(&to_join, "\"\n");
   i++;
  }

  BUFF_APPEND(&to_join, character_prompt);

  // concatenate into prompt_buff
  BUFF_ITER(char *, &to_join)
  {
   size_t cur_len = strlen(*it);
   for(int i = 0; i < cur_len; i++)
   {
    BUFF_APPEND(&prompt_buff, (*it)[i]);
   }
  }

  const char * prompt = prompt_buff.data;
#ifdef DEVTOOLS
  Log("Prompt: \"%s\"\n", prompt);
#endif
#ifdef WEB
  // fire off generation request, save id
  int req_id = EM_ASM_INT({
    return make_generation_request(UTF8ToString($1), UTF8ToString($0));
  }, SERVER_URL, prompt);
  player->talking_to->gen_request_id = req_id;
#endif
#ifdef DESKTOP
  if(player->talking_to->npc_kind == DEATH)
  {
      add_new_npc_sentence(player->talking_to, "test *moves* I am death, destroyer of games. Come join me in the afterlife, or continue onwards *moves*");
      //add_new_npc_sentence(player->talking_to, "test");
  }
  if(player->talking_to->npc_kind == OLD_MAN)
  {
      add_new_npc_sentence(player->talking_to, "If it's a fight you're looking for! *fights player*");
  }
#endif
 }
}

// keydown needs to be referenced when begin text input,
// on web it disables event handling so the button up event isn't received
bool keydown[SAPP_KEYCODE_MENU] = {0};

#ifdef DESKTOP
bool receiving_text_input = false;
Sentence text_input_buffer = {0};
void begin_text_input()
{
 receiving_text_input = true;
 BUFF_CLEAR(&text_input_buffer);
}
#else
#ifdef WEB
void begin_text_input()
{
 Log("Disabling event handlers\n");
 _sapp_emsc_unregister_eventhandlers(); // stop getting input, hand it off to text input
 memset(keydown, 0, ARRLEN(keydown));
 emscripten_run_script("start_dialog();");
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

Vec2 entity_aabb_size(Entity *e)
{
 if(e->is_character == ENTITY_PLAYER)
 {
  return V2(TILE_SIZE, TILE_SIZE);
 }
 else if(e->is_npc)
 {
  if(e->npc_kind == OLD_MAN)
  {
   return V2(TILE_SIZE*0.5f, TILE_SIZE*0.5f);
  }
  else if(e->npc_kind == DEATH)
  {
   return V2(TILE_SIZE*1.10f, TILE_SIZE*1.10f);
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
 else
 {
  assert(false);
  return (Vec2){0};
 }
}

bool is_tile_solid(TileInstance t)
{
 uint16_t tile_id = t.kind;
 return tile_id == 53 || tile_id == 0 || tile_id == 367 || tile_id == 317 || tile_id == 313 || tile_id == 366 || tile_id == 368;
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
 return centered_aabb(e->pos, entity_aabb_size(e));
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

AnimatedSprite knight_idle =
{
 .img = &image_knight_idle,
 .time_per_frame = 0.3,
 .num_frames = 10,
 .start = {16.0f, 0.0f},
 .horizontal_diff_btwn_frames = 120.0,
 .region_size = {80.0f, 80.0f},
};

AnimatedSprite knight_running =
{
 .img = &image_knight_run,
 .time_per_frame = 0.06,
 .num_frames = 10,
 .start = {19.0f, 0.0f},
 .horizontal_diff_btwn_frames = 120.0,
 .region_size = {80.0f, 80.0f},
};

AnimatedSprite knight_rolling =
{
 .img = &image_knight_roll,
 .time_per_frame = 0.05,
 .num_frames = 12,
 .start = {19.0f, 0.0f},
 .horizontal_diff_btwn_frames = 120.0,
 .region_size = {80.0f, 80.0f},
 .no_wrap = true,
};


AnimatedSprite knight_attack = 
{
 .img = &image_knight_attack,
 .time_per_frame = 0.06,
 .num_frames = 4,
 .start = {37.0f, 0.0f},
 .horizontal_diff_btwn_frames = 120.0,
 .region_size = {80.0f, 80.0f},
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

AABB level_aabb = { .upper_left = {0.0f, 0.0f}, .lower_right = {2000.0f, -2000.0f} };
Entity entities[MAX_ENTITIES] = {0};

Entity *new_entity()
{
 for(int i = 0; i < ARRLEN(entities); i++)
 {
  if(!entities[i].exists)
  {
   Entity *to_return = &entities[i];
   *to_return = (Entity){0};
   to_return->exists = true;
   return to_return;
  }
 }
 assert(false);
 return NULL;
}

void reset_level()
{
 // load level
 Level *to_load = &level_level0;
 {
  assert(ARRLEN(to_load->initial_entities) == ARRLEN(entities));
  memcpy(entities, to_load->initial_entities, sizeof(Entity) * MAX_ENTITIES);

  player = NULL;
  ENTITIES_ITER(entities)
  {
   if(it->is_character)
   {
    assert(player == NULL);
    player = it;
   }
  }
  assert(player != NULL); // level initial config must have player entity
 }
}


void init(void)
{
 sg_setup(&(sg_desc){
   .context = sapp_sgcontext(),
  });
 stm_setup();

 scratch = make_arena(1024 * 10);

 load_assets();
 reset_level();

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

 state.pip = sg_make_pipeline(&(sg_pipeline_desc)
   {
    .shader = shd,
    .layout = {
     .attrs =
     {
      [ATTR_quad_vs_position].format = SG_VERTEXFORMAT_FLOAT2,
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
  { .action=SG_ACTION_CLEAR, .value={137.0f/255.0f, 137.0f/255.0f, 137.0f/255.0f, 1.0f } }
 };
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

Vec2 screen_size()
{
 return V2((float)sapp_width(), (float)sapp_height());
}

typedef struct Camera
{
 Vec2 pos;
 float scale;
} Camera;


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
 return size_vec.Y <= 0.0f && size_vec.X >= 0.0f;
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
 assert(cur_batch_data_index % 4 == 0);
 sg_draw(0, cur_batch_data_index/4, 1);
 num_draw_calls += 1;
 memset(cur_batch_data, 0, cur_batch_data_index);
 cur_batch_data_index = 0;
}

typedef struct DrawParams
{
 bool world_space;
 Quad quad;
 sg_image image;
 AABB image_region;
 Color tint;

 AABB clip_to; // if world space is in world space, if screen space is in screen space - Lao Tzu
} DrawParams;


Vec2 into_clip_space(Vec2 screen_space_point)
{
  Vec2 zero_to_one = DivV2(screen_space_point, screen_size());
  Vec2 in_clip_space = SubV2(MulV2F(zero_to_one, 2.0), V2(1.0, 1.0));
  return in_clip_space;
}

// The image region is in pixel space of the image
void draw_quad(DrawParams d)
{
 PROFILE_SCOPE("quad")
 {
 quad_fs_params_t params = {0};
 params.tint[0] = d.tint.R;
 params.tint[1] = d.tint.G;
 params.tint[2] = d.tint.B;
 params.tint[3] = d.tint.A;

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


 // if the rendering call is different, and the batch must be flushed
 if(d.image.id != cur_batch_image.id || memcmp(&params,&cur_batch_params,sizeof(params)) != 0 )
 {
  flush_quad_batch();
  cur_batch_image = d.image;
  cur_batch_params = params;
 }

 Vec2 *points = d.quad.points;

 if(d.world_space)
 {
  for(int i = 0; i < 4; i++)
  {
   points[i] = world_to_screen(points[i]);
  }
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

 float new_vertices[ (2 + 2)*4 ] = {0};
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
  new_vertices[i*4] = in_clip_space.X;
  new_vertices[i*4 + 1] = in_clip_space.Y;
  new_vertices[i*4 + 2] = tex_coords[i].X;
  new_vertices[i*4 + 3] = tex_coords[i].Y;
 }

 size_t total_size = ARRLEN(new_vertices)*sizeof(new_vertices);

 // batched a little too close to the sun
 if(cur_batch_data_index + total_size >= ARRLEN(cur_batch_data))
 {
  flush_quad_batch();
  cur_batch_image = d.image;
  cur_batch_params = params;
 }

#define PUSH_VERTEX(vert) { memcpy(&cur_batch_data[cur_batch_data_index], &vert, 4*sizeof(float)); cur_batch_data_index += 4; }
 PUSH_VERTEX(new_vertices[0*4]);
 PUSH_VERTEX(new_vertices[1*4]);
 PUSH_VERTEX(new_vertices[2*4]);
 PUSH_VERTEX(new_vertices[0*4]);
 PUSH_VERTEX(new_vertices[2*4]);
 PUSH_VERTEX(new_vertices[3*4]);
#undef PUSH_VERTEX

}
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

void draw_animated_sprite(AnimatedSprite *s, double elapsed_time, bool flipped, Vec2 pos, Color tint)
{
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

 draw_quad((DrawParams){true, q, spritesheet_img, region, tint});
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
 draw_quad((DrawParams){world_space, q, image_white_square, full_region(image_white_square), col});
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
 line(from, to, 2.0f, RED);
#else
 (void)from;
 (void)to;
#endif
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
    draw_quad((DrawParams){t.world_space, to_draw, image_font, font_atlas_region, col, t.clip_to});
   }
  }
 }

 bounds.upper_left = AddV2(bounds.upper_left, t.pos);
 bounds.lower_right = AddV2(bounds.lower_right, t.pos);
 return bounds;
}

// gets aabbs overlapping the input aabb, including entities and tiles
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

 // the entities jessie
 ENTITIES_ITER(entities)
 {
  if(!(it->is_character && it->is_rolling) && overlapping(aabb, entity_aabb(it)))
  {
   BUFF_APPEND(&to_return, (Overlap){.e = it});
  }
 }

 return to_return;
}

// returns new pos after moving and sliding against collidable things
Vec2 move_and_slide(Entity *from, Vec2 position, Vec2 movement_this_frame)
{
 Vec2 collision_aabb_size = entity_aabb_size(from);
 Vec2 new_pos = AddV2(position, movement_this_frame);
 AABB at_new = centered_aabb(new_pos, collision_aabb_size);
 dbgrect(at_new);
 AABB to_check[256] = {0};
 int to_check_index = 0;

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

   if(is_tile_solid(get_tile(&level_level0, tilecoord_to_check)))
   {
    to_check[to_check_index++] = tile_aabb(tilecoord_to_check);
    assert(to_check_index < ARRLEN(to_check));
   }
  }
 }

 // add entity boxes
 if(!(from->is_character && from->is_rolling))
 {
  ENTITIES_ITER(entities)
  {
   if(!(it->is_character && it->is_rolling) && it != from)
   {
    to_check[to_check_index++] = centered_aabb(it->pos, entity_aabb_size(it));
    assert(to_check_index < ARRLEN(to_check));
   }
  }
 }
 for(int i = 0; i < to_check_index; i++)
 {
  AABB to_depenetrate_from = to_check[i];
  dbgrect(to_depenetrate_from);
  int iters_tried_to_push_apart = 0;
  while(overlapping(to_depenetrate_from, at_new) && iters_tried_to_push_apart < 500)
  { 
   //dbgsquare(to_depenetrate_from.upper_left);
   //dbgsquare(to_depenetrate_from.lower_right);
   const float move_dist = 0.05f;

   Vec2 to_player = NormV2(SubV2(aabb_center(at_new), aabb_center(to_depenetrate_from)));
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
   Vec2 move = MulV2F(move_dir, move_dist);
   at_new.upper_left = AddV2(at_new.upper_left,move);
   at_new.lower_right = AddV2(at_new.lower_right,move);
   iters_tried_to_push_apart++;
  }
 }

 return aabb_center(at_new);
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
 }

 return cursor.Y;
}

void draw_dialog_panel(Entity *talking_to)
{
 // talking to them feedback
 draw_quad((DrawParams){true, quad_centered(talking_to->pos, V2(TILE_SIZE, TILE_SIZE)), image_dialog_circle, full_region(image_dialog_circle), WHITE});
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
  colorquad(true, dialog_quad, (Color){1.0f, 1.0f, 1.0f, 0.4f});
  float width = 2.0f;
  line(AddV2(dialog_quad.ul, V2(-width,0.0)), AddV2(dialog_quad.ur, V2(width,0.0)), width, BLACK);
  line(dialog_quad.ur, dialog_quad.lr, width, BLACK);
  line(AddV2(dialog_quad.lr, V2(width,0.0)), AddV2(dialog_quad.ll, V2(-width,0.0)), width, BLACK);
  line(dialog_quad.ll, dialog_quad.ul, width, BLACK);

  float padding = 5.0f;
  dialog_panel.upper_left = AddV2(dialog_panel.upper_left, V2(padding, -padding));
  dialog_panel.lower_right = AddV2(dialog_panel.lower_right, V2(-padding, padding));
  
  if(aabb_is_valid(dialog_panel))
  {
   float new_line_height = dialog_panel.lower_right.Y;
   int i = 0;
   //BUFF_ITER(Sentence, &talking_to->player_dialog)
   BUFF_ITER_EX(Sentence, &talking_to->player_dialog, talking_to->player_dialog.cur_index-1, it >= &talking_to->player_dialog.data[0], it--)
   {
    bool player_talking = i % 2 != 0; // iterating backwards
    Color *colors = calloc(sizeof(*colors), it->cur_index);
    bool in_astrix = false;
    for(int char_i = 0; char_i < it->cur_index; char_i++)
    {
     bool set_in_astrix_false = false;
     if(it->data[char_i] == '*')
     {
      if(in_astrix)
      {
       set_in_astrix_false = true;
      }
      else
      {
       in_astrix = true;
      }
     }
     if(player_talking)
     {
      colors[char_i] = BLACK;
     }
     else
     {
      if(in_astrix)
      {
       colors[char_i] = colhex(0xffdf24);
      }
      else
      {
       colors[char_i] = colhex(0x345e22);
      }
     }
     if(set_in_astrix_false) in_astrix = false;
    }
    float measured_line_height = draw_wrapped_text(true, V2(dialog_panel.upper_left.X, new_line_height), dialog_panel.lower_right.X - dialog_panel.upper_left.X, it->data, colors, 0.5f, true, dialog_panel);
    new_line_height += (new_line_height - measured_line_height);
    draw_wrapped_text(false, V2(dialog_panel.upper_left.X, new_line_height), dialog_panel.lower_right.X - dialog_panel.upper_left.X, it->data, colors, 0.5f, true, dialog_panel);

    free(colors);
    i++;
   }

   dbgrect(dialog_panel);
  }
 }

}



#define ROLL_KEY SAPP_KEYCODE_K
double elapsed_time = 0.0;
double last_frame_processing_time = 0.0;
uint64_t last_frame_time;
Vec2 mouse_pos = {0}; // in screen space
bool roll_just_pressed = false; // to use to initiate dialog, shouldn't initiate dialog if the button is simply held down
#ifdef DEVTOOLS
bool mouse_frozen = false;
#endif
void frame(void)
{
 PROFILE_SCOPE("frame")
 {
#if 0
  {
   sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
   sg_apply_pipeline(state.pip);

   //colorquad(false, quad_at(V2(0.0, 100.0), V2(100.0f, 100.0f)), RED);
   sg_image img = image_wonky_mystery_tile;
   AABB region = full_region(img);
   //region.lower_right.X *= 0.5f;
   draw_quad((DrawParams){false,quad_at(V2(0.0, 100.0), V2(100.0f, 100.0f)), img, region, WHITE});

   sg_end_pass();
   sg_commit();
   reset(&scratch);
  }
  return;
#endif

  uint64_t time_start_frame = stm_now();
  // elapsed_time
  double dt_double = 0.0;
  {
   dt_double = stm_sec(stm_diff(stm_now(), last_frame_time));
   dt_double = fmin(dt_double, 5.0 / 60.0); // clamp dt at maximum 5 frames, avoid super huge dt
   elapsed_time += dt_double;
   last_frame_time = stm_now();
  }
  float dt = (float)dt_double;

  Vec2 movement = V2(
    (float)keydown[SAPP_KEYCODE_D] - (float)keydown[SAPP_KEYCODE_A],
    (float)keydown[SAPP_KEYCODE_W] - (float)keydown[SAPP_KEYCODE_S]
    );
  bool attack = keydown[SAPP_KEYCODE_J];
  bool roll = keydown[ROLL_KEY];
  if(LenV2(movement) > 1.0)
  {
   movement = NormV2(movement);
  }
  sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
  sg_apply_pipeline(state.pip);

  Level * cur_level = &level_level0;

  // tilemap drawing
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
        if(tileset.animated[i].id_from == cur.kind-1)
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

       draw_quad((DrawParams){true, tile_quad(cur_coord), tileset_image, region, WHITE});
      }
     }
    }
   }
  }
#endif

  assert(player != NULL);

#ifdef DEVTOOLS
  dbgsquare(screen_to_world(mouse_pos));

  // tile coord
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
  PROFILE_SCOPE("statistics")
  {
   Vec2 pos = V2(0.0, screen_size().Y);
   int num_entities = 0;
   ENTITIES_ITER(entities) num_entities++;
   char *stats = tprint("Frametime: %.1f ms\nProcessing: %.1f ms\nEntities: %d\nDraw calls: %d\nProfiling: %s\n", dt*1000.0, last_frame_processing_time*1000.0, num_entities, num_draw_calls, profiling ? "yes" : "no");
   AABB bounds = draw_text((TextParams){false, true, stats, pos, BLACK, 1.0f});
   pos.Y -= bounds.upper_left.Y - screen_size().Y;
   bounds = draw_text((TextParams){false, true, stats, pos, BLACK, 1.0f});
   // background panel
   colorquad(false, quad_aabb(bounds), (Color){1.0, 1.0, 1.0, 0.3f});
   draw_text((TextParams){false, false, stats, pos, BLACK, 1.0f});
   num_draw_calls = 0;
  }
#endif // devtools

  // process entities
  
  PROFILE_SCOPE("entity processing")
  ENTITIES_ITER(entities)
  {
#ifdef WEB
   if(it->gen_request_id != 0)
   {
    assert(it->gen_request_id > 0);
    draw_quad((DrawParams){true, quad_centered(AddV2(it->pos, V2(0.0, 50.0)), V2(100.0,100.0)), IMG(image_thinking), WHITE});
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

      add_new_npc_sentence(it, sentence_str);

      EM_ASM({
        done_with_generation_request($0);
        }, it->gen_request_id);
     }
     else if(status == 2)
     {
      Log("Failed to generate dialog! Fuck!");
      // need somethin better here. Maybe each sentence has to know if it's player or NPC, that way I can remove the player's dialog
      make_space_and_append(&it->player_dialog, &SENTENCE_CONST("I'm not sure..."));
     }
     it->gen_request_id = 0;
    }
   }
#endif

   if(it->is_npc)
   {
    if(!BUFF_EMPTY(&it->sentence_to_say))
    {
     it->character_say_timer += dt;
     const float character_say_time = 0.05f;
     while(it->character_say_timer > character_say_time)
     {
      say_characters(it, 1);
      it->character_say_timer -= character_say_time;
     }
    }
    if(it->npc_kind == DEATH && it->going_to_target)
    {
     it->pos = LerpV2(it->pos, dt*5.0f, it->target_goto);
    }
    if(it->aggressive)
    {
     draw_dialog_panel(it);
     Entity *targeting = player;
     it->shotgun_timer += dt;
     Vec2 to_player = NormV2(SubV2(targeting->pos, it->pos));
     if(it->shotgun_timer >= 1.0f)
     {
      it->shotgun_timer = 0.0f;
      const float spread = (float)PI/4.0f;
      // shoot shotgun
      for(int i = 0; i < 3; i++)
      {
       Vec2 dir = to_player;
       float theta = Lerp(-spread/2.0f, ((float)i / 2.0f), spread/2.0f);
       dir = RotateV2(dir, theta);
       Entity *new_bullet = new_entity();
       new_bullet->is_bullet = true;
       new_bullet->pos = AddV2(it->pos, MulV2F(dir, 20.0f));
       new_bullet->vel = MulV2F(dir, 10.0f);
       it->vel = AddV2(it->vel, MulV2F(dir, -3.0f));
      }
     }

     Vec2 target_vel = NormV2(AddV2(rotate_counter_clockwise(to_player), MulV2F(to_player, 0.5f)));
     target_vel = MulV2F(target_vel, 3.0f);
     it->vel = LerpV2(it->vel, 15.0f * dt, target_vel);
     it->pos = move_and_slide(it, it->pos, MulV2F(it->vel, pixels_per_meter * dt));
    }

    Color col = LerpV4(WHITE, it->damage, RED);
    if(it->npc_kind == OLD_MAN)
    {
     draw_animated_sprite(&old_man_idle, elapsed_time, false, it->pos, col);
    }
    else if(it->npc_kind == DEATH)
    {
     draw_animated_sprite(&death_idle, elapsed_time, true, AddV2(it->pos, V2(0, 30.0f)), col);
    }
    else
    {
     assert(false);
    }
    if(it->damage >= 1.0)
    {
     *it = (Entity){0};
    }
   }
   else if (it->is_bullet)
   {
    it->pos = AddV2(it->pos, MulV2F(it->vel, pixels_per_meter * dt));
    draw_quad((DrawParams){true, quad_aabb(entity_aabb(it)), image_white_square, full_region(image_white_square), WHITE});
    Overlapping over = get_overlapping(cur_level, entity_aabb(it));
    Entity *from_bullet = it;
    BUFF_ITER(Overlap, &over) if(it->e != from_bullet)
    {
     if(!it->is_tile && !(it->e->npc_kind == DEATH))
     {
      // knockback and damage
      request_do_damage(it->e, from_bullet->pos, 0.2f);
      *from_bullet = (Entity){0};
     }
    }
    if(!has_point(level_aabb, it->pos)) *it = (Entity){0};
   }
   else if(it->is_character)
   {
   }
   else
   {
    assert(false);
   }
  }

  // process player character
  PROFILE_SCOPE("process player character")
  {
   Vec2 character_sprite_pos = AddV2(player->pos, V2(0.0, 20.0f));


   // do dialog
   Entity *closest_talkto = NULL;
   {
    // find closest to talk to
    {
     AABB dialog_rect = centered_aabb(player->pos, V2(TILE_SIZE*2.0f, TILE_SIZE*2.0f));
     dbgrect(dialog_rect);
     Overlapping possible_dialogs = get_overlapping(cur_level, dialog_rect);
     float closest_talkto_dist = INFINITY;
     BUFF_ITER(Overlap, &possible_dialogs)
     {
      bool entity_talkable = true;
      if(entity_talkable) entity_talkable = entity_talkable && !it->is_tile;
      if(entity_talkable) entity_talkable = entity_talkable && it->e->is_npc;
      if(entity_talkable) entity_talkable = entity_talkable && !it->e->aggressive;
#ifdef WEB
      if(entity_talkable) entity_talkable = entity_talkable && it->e->gen_request_id == 0;
#endif
      if(entity_talkable)
      {
       float dist = LenV2(SubV2(it->e->pos, player->pos));
       if(dist < closest_talkto_dist)
       {
        closest_talkto_dist = dist;
        closest_talkto = it->e;
       }
      }
     }
    }

    Entity *talking_to = closest_talkto;
    if(player->state == CHARACTER_TALKING)
    {
     talking_to = player->talking_to;
     assert(talking_to);
    }

    // if somebody, show their dialog panel
    if(talking_to) 
    {
     draw_dialog_panel(talking_to);
    }

    // process dialog and display dialog box when talking to NPC
    if(player->state == CHARACTER_TALKING)
    {
     assert(player->talking_to != NULL);

     if(player->talking_to->aggressive || !player->exists)
     {
      player->state = CHARACTER_IDLE;
     }
    }
   }
   // roll input management, sometimes means talk to the npc
   if(player->state != CHARACTER_TALKING && roll_just_pressed && closest_talkto != NULL)
   {
    // begin dialog with closest npc
    player->state = CHARACTER_TALKING;
    player->talking_to = closest_talkto;
    begin_text_input();
   }
   else
   {
    // rolling trigger from input
    if(roll && !player->is_rolling && player->time_not_rolling > 0.3f && (player->state == CHARACTER_IDLE || player->state == CHARACTER_WALKING))
    {
     player->is_rolling = true;
     player->roll_progress = 0.0;
     player->speed = PLAYER_ROLL_SPEED;
    }
   }

   if(attack && (player->state == CHARACTER_IDLE || player->state == CHARACTER_WALKING))
   {
    player->state = CHARACTER_ATTACK;
    player->swing_progress = 0.0;
    AABB weapon_aabb = {0};
    if(player->facing_left)
    {
     weapon_aabb = (AABB){
      .upper_left = AddV2(player->pos, V2(-40.0, 25.0)),
       .lower_right = AddV2(player->pos, V2(0.0, -25.0)),
     };
    }
    else
    {
     weapon_aabb = (AABB){
      .upper_left = AddV2(player->pos, V2(0.0, 25.0)),
       .lower_right = AddV2(player->pos, V2(40.0, -25.0)),
     };
    }
    dbgrect(weapon_aabb);
    Overlapping overlapping_weapon = get_overlapping(cur_level, weapon_aabb);
    BUFF_ITER(Overlap, &overlapping_weapon)
    {
     if(!it->is_tile && it->e != player)
     {
      request_do_damage(it->e, player->pos, 0.2f);
     }
    }

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

   Vec2 target = MulV2F(player->pos, -1.0f * cam.scale);
   if(LenV2(SubV2(target, cam.pos)) <= 0.2)
   {
    cam.pos = target;
   }
   else
   {
    cam.pos = LerpV2(cam.pos, dt*8.0f, target);
   }
   if(player->state == CHARACTER_WALKING)
   {
    if(player->speed <= 0.01f) player->speed = PLAYER_SPEED;
    player->speed = Lerp(player->speed, dt * 3.0f, PLAYER_SPEED);
    player->pos = move_and_slide(player, player->pos, MulV2F(movement, dt * pixels_per_meter * player->speed));
    if(player->is_rolling)
    {
     draw_animated_sprite(&knight_rolling, player->roll_progress, player->facing_left, character_sprite_pos, WHITE);
    }
    else
    {
     draw_animated_sprite(&knight_running, elapsed_time, player->facing_left, character_sprite_pos, WHITE);
    }

    if(LenV2(movement) == 0.0)
    {
     player->state = CHARACTER_IDLE;
    }
    else
    {
     player->facing_left = movement.X < 0.0f;
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
    if(LenV2(movement) > 0.01) player->state = CHARACTER_WALKING;
   }
   else if(player->state == CHARACTER_ATTACK)
   {

    player->swing_progress += dt;
    draw_animated_sprite(&knight_attack, player->swing_progress, player->facing_left, character_sprite_pos, WHITE);
    if(player->swing_progress > anim_sprite_duration(&knight_attack))
    {
     player->state = CHARACTER_IDLE;
    }
   }
   else if(player->state == CHARACTER_TALKING)
   {
    draw_animated_sprite(&knight_idle, elapsed_time, player->facing_left, character_sprite_pos, WHITE);
   }
   else
   {
    assert(false); // unknown character state? not defined how to draw
   }

   // health
   if(player->damage >= 1.0)
   {
    reset_level();
   }
   else
   {
    draw_quad((DrawParams){false, (Quad){.ul=V2(0.0f, screen_size().Y), .ur = screen_size(), .lr = V2(screen_size().X, 0.0f)}, image_hurt_vignette, full_region(image_hurt_vignette), (Color){1.0f, 1.0f, 1.0f, player->damage}});
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
  roll_just_pressed = false;
 }
}

void cleanup(void)
{
 sg_shutdown();
 Log("Cleaning up\n");
}

void event(const sapp_event *e)
{
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
 if(e->type == SAPP_EVENTTYPE_KEY_DOWN)
 {
  assert(e->key_code < sizeof(keydown)/sizeof(*keydown));
  keydown[e->key_code] = true;
  
  if(e->key_code == ROLL_KEY)
  {
   roll_just_pressed = true;
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
   .icon.sokol_default = true,
 };
}
