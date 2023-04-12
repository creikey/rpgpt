#pragma once
#include "buff.h"
#include "HandmadeMath.h" // vector types in entity struct definition
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h> // atoi
#include "character_info.h"
#include "characters.gen.h"
 NPC_Skeleton,
 NPC_MOOSE,
} NpcKind;

#define DO_CHATGPT_PARSING

#define Log(...) { printf("%s Log %d | ", __FILE__, __LINE__); printf(__VA_ARGS__); }

// REFACTORING:: also have to update in javascript!!!!!!!!
#define MAX_SENTENCE_LENGTH 400 // LOOOK AT AGBOVE COMMENT GBEFORE CHANGING
typedef BUFF(char, MAX_SENTENCE_LENGTH) Sentence;
#define SENTENCE_CONST(txt) {.data=txt, .cur_index=sizeof(txt)}
#define SENTENCE_CONST_CAST(txt) (Sentence)SENTENCE_CONST(txt)

#define REMEMBERED_PERCEPTIONS 24

#define MAX_AFTERIMAGES 6
#define TIME_TO_GEN_AFTERIMAGE (0.09f)
#define AFTERIMAGE_LIFETIME (0.5f)

#define DAMAGE_SWORD 0.2f
#define DAMAGE_BULLET 0.2f

// Never expected such a stupid stuff from such a great director. If there is 0 stari can give that or -200 to this movie. Its worst to see and unnecessary loss of money

typedef BUFF(char, 1024*10) Escaped;
Escaped escape_for_json(const char *s)
{
 Escaped to_return = {0};
 size_t len = strlen(s);
 for(int i = 0; i < len; i++)
 {
  if(s[i] == '\n')
  {
  BUFF_APPEND(&to_return, '\\');
  BUFF_APPEND(&to_return, 'n');
  }
  else if(s[i] == '"')
  {
  BUFF_APPEND(&to_return, '\\');
  BUFF_APPEND(&to_return, '"');
  }
  else
  {
   if(!(s[i] <= 126 && s[i] >= 32 ))
   {
    BUFF_APPEND(&to_return, '?');
    Log("Unknown character code %d\n", s[i]);
   }
   BUFF_APPEND(&to_return, s[i]);
  }
 }
 return to_return;
}
typedef enum PerceptionType
{
 Invalid, // so that zero value in training structs means end of perception
 PlayerAction,
 PlayerDialog,
 NPCDialog, // includes an npc action in every npc dialog. So it's often nothing
 EnemyAction, // An enemy performed an action against the NPC
 PlayerHeldItemChanged,
} PerceptionType;

typedef struct Perception
{
 PerceptionType type;

 float damage_done; // Valid in player action and enemy action
 union 
 {
  // player action
  struct
  {
   Action player_action_type;
  };

  // player dialog
  Sentence player_dialog;

  // npc dialog
  struct
  {
   Action npc_action_type;
   Sentence npc_dialog;
  };

  // enemy action
  Action enemy_action_type;

  // player holding item. MUST precede any perceptions which come after the player is holding the item
  ItemKind holding;
 };
} Perception;

typedef enum PropKind
{
 TREE0,
 TREE1,
 TREE2,
 ROCK0,
} PropKind;

typedef struct EntityRef
{
 int index;
 int generation;
} EntityRef;

typedef enum CharacterState
{
 CHARACTER_WALKING,
 CHARACTER_IDLE,
 CHARACTER_ATTACK,
 CHARACTER_TALKING,
} CharacterState;

typedef enum
{
 STANDING_INDIFFERENT,
 STANDING_JOINED,
 STANDING_FIGHTING,
} NPCPlayerStanding;


typedef Vec4 Color;

typedef struct
{
 AnimKind anim;
 double elapsed_time;
 bool flipped;
 Vec2 pos;
 Color tint;
 bool no_shadow;
} DrawnAnimatedSprite;

typedef struct
{
 DrawnAnimatedSprite drawn;
 float alive_for;
} PlayerAfterImage;

typedef struct Entity
{
 bool exists;
 bool destroy;
 int generation;

 // fields for all gs.entities
 Vec2 pos;
 Vec2 vel; // only used sometimes, like in old man and bullet
 float damage; // at 1.0, dead! zero initialized
 bool facing_left;
 double dead_time;
 bool dead;
 // multiple gs.entities have a sword swing
 BUFF(EntityRef, 8) done_damage_to_this_swing; // only do damage once, but hitbox stays around

 bool is_bullet;

 // props
 bool is_prop;
 PropKind prop_kind;

 // items
 bool is_item;
 bool held_by_player;
 ItemKind item_kind;

 // npcs
 bool is_npc;
 bool perceptions_dirty;
 BUFF(Perception, REMEMBERED_PERCEPTIONS) remembered_perceptions;
 bool direction_of_spiral_pattern;
 double characters_said;
 NPCPlayerStanding standing;
 NpcKind npc_kind;
 ItemKind last_seen_holding_kind;
#ifdef WEB
 int gen_request_id;
#endif
 bool walking;
 double shotgun_timer;
 bool moved;
 Vec2 target_goto;
 // only for skeleton npc
 double swing_timer;

 // character
 bool is_character;
 EntityRef holding_item;
 bool has_paused_time;
 Vec2 to_throw_direction;
 CharacterState state;
 EntityRef talking_to; // Maybe should be generational index, but I dunno. No death yet
 bool is_rolling; // can only roll in idle or walk states
 double time_not_rolling; // for cooldown for roll, so you can't just hold it and be invincible
 BUFF(PlayerAfterImage, MAX_AFTERIMAGES) after_images;
 double after_image_timer;
 double roll_progress;
 double swing_progress;
} Entity;

bool npc_is_knight_sprite(Entity *it)
{
 return it->is_npc && ( it->npc_kind == NPC_TheGuard || it->npc_kind == NPC_Edeline);
}

typedef BUFF(char, MAX_SENTENCE_LENGTH*(REMEMBERED_PERCEPTIONS+4)) PromptBuff;
typedef BUFF(Action, 8) AvailableActions;

void fill_available_actions(Entity *it, AvailableActions *a)
{
 *a = (AvailableActions){0};
 BUFF_APPEND(a, ACT_none);
 if(it->npc_kind == NPC_GodRock)
 {
  BUFF_APPEND(a, ACT_heals_player);
 }
 else
 {
  if(it->standing == STANDING_INDIFFERENT)
  {
   BUFF_APPEND(a, ACT_fights_player);
   BUFF_APPEND(a, ACT_joins_player);
  }
  else if(it->standing == STANDING_JOINED)
  {
   BUFF_APPEND(a, ACT_leaves_player);
   BUFF_APPEND(a, ACT_fights_player);
  }
  else if(it->standing == STANDING_FIGHTING)
  {
   BUFF_APPEND(a, ACT_leaves_player);
  }
  if(npc_is_knight_sprite(it))
  {
   BUFF_APPEND(a, ACT_strikes_air);
  }
  if(it->npc_kind == NPC_TheGuard)
  {
   if(!it->moved)
   {
    BUFF_APPEND(a, ACT_allows_player_to_pass);
   }
  }
 }
}

// returns if action index was valid
bool action_from_index(Entity *it, Action *out, int action_index)
{
 AvailableActions available = {0};
 fill_available_actions(it, &available);
 if(action_index < 0 || action_index >= available.cur_index)
 {
  return false;
 }
 else
 {
  *out = available.data[action_index];
  return true;
 }
}

// don't call on untrusted action, doesn't return error
int action_to_index(Entity *it, Action a)
{
 AvailableActions available = {0};
 fill_available_actions(it, &available);
 Action target_action = a;
 int index = -1;
 for(int i = 0; i < available.cur_index; i++)
 {
  if(available.data[i] == target_action)
  {
   index = i;
   break;
  }
 }
 assert(index != -1);
 return index;
}

void process_perception(Entity *it, Perception p)
{
 if(it->is_npc)
 {
  if(p.type != NPCDialog) it->perceptions_dirty = true;
  if(!BUFF_HAS_SPACE(&it->remembered_perceptions)) BUFF_REMOVE_FRONT(&it->remembered_perceptions);
  BUFF_APPEND(&it->remembered_perceptions, p);
  if(p.type == PlayerAction && p.player_action_type == ACT_hits_npc)
  {
   it->damage += p.damage_done;
  }
  if(p.type == PlayerHeldItemChanged)
  {
   it->last_seen_holding_kind = p.holding;
  }
  else if(p.type == NPCDialog)
  {
   if(p.npc_action_type == ACT_allows_player_to_pass)
   {
    it->target_goto = AddV2(it->pos, V2(-50.0, 0.0));
    it->moved = true;
   }
   else if(p.npc_action_type == ACT_fights_player)
   {
    it->standing = STANDING_FIGHTING;
   }
   else if(p.npc_action_type == ACT_leaves_player)
   {
    it->standing = STANDING_INDIFFERENT;
   }
   else if(p.npc_action_type == ACT_joins_player)
   {
    it->standing = STANDING_JOINED;
   }
  }
 }
}

#define printf_buff(buff_ptr, ...) { BUFF_VALID(buff_ptr); int written = snprintf((buff_ptr)->data+(buff_ptr)->cur_index, ARRLEN((buff_ptr)->data) - (buff_ptr)->cur_index, __VA_ARGS__); assert(written >= 0); (buff_ptr)->cur_index += written; };

bool npc_does_dialog(Entity *it)
{
 return it->npc_kind < ARRLEN(characters);
}

typedef enum
{
 MSG_SYSTEM,
 MSG_USER,
 MSG_ASSISTANT,
} MessageType;

void dump_json_node_trailing(PromptBuff *into, MessageType type, const char *content, bool trailing_comma)
{
 const char *type_str = 0;
 if(type == MSG_SYSTEM)
  type_str = "system";
 else if(type == MSG_USER)
  type_str = "user";
 else if(type == MSG_ASSISTANT)
  type_str = "assistant";
 assert(type_str);
 printf_buff(into, "{\"type\": \"%s\", \"content\": \"%s\"}", type_str, escape_for_json(content).data);
 if(trailing_comma) printf_buff(into, ",");
}

void dump_json_node(PromptBuff *into, MessageType type, const char *content)
{
 dump_json_node_trailing(into, type, content, true);
}

// outputs json
void generate_chatgpt_prompt(Entity *it, PromptBuff *into)
{
 assert(it->is_npc);
 assert(it->npc_kind < ARRLEN(characters));

 *into = (PromptBuff){0};

 printf_buff(into, "[");

 BUFF(char, 1024*10) initial_system_msg = {0};
 const char *health_string = 0;
 if(it->damage <= 0.2f)
 {
  health_string = "the NPC hasn't taken much damage, they're healthy.";
 }
 else if(it->damage <= 0.5f)
 {
  health_string = "the NPC has taken quite a chunk of damage, they're soon gonna be ready to call it quits.";
 }
 else if(it->damage <= 0.8f)
 {
  health_string = "the NPC is close to dying! They want to leave the player's party ASAP";
 }
 else
 {
  health_string = "it's over for the NPC, they're basically dead they've taken so much damage. They should get their affairs in order.";
 }
 assert(health_string);

 printf_buff(&initial_system_msg, "%s\n%s\nNPC health status: Right now, %s\n%s", global_prompt, characters[it->npc_kind].prompt, health_string, items[it->last_seen_holding_kind].global_prompt);
 dump_json_node(into, MSG_SYSTEM, initial_system_msg.data);

 Entity *e = it;
 ItemKind last_holding = ITEM_none;
 BUFF_ITER_I(Perception, &e->remembered_perceptions, i)
 {
  BUFF(char, 1024) cur_node = {0};
  if(it->type == PlayerAction)
  {
   assert(it->player_action_type < ARRLEN(actions));
   printf_buff(&cur_node, "Player: ACT_%s", actions[it->player_action_type]);
   dump_json_node(into, MSG_USER, cur_node.data);
  }
  else if(it->type == EnemyAction)
  {
   assert(it->enemy_action_type < ARRLEN(actions));
   printf_buff(&cur_node, "An Enemy: ACT_%s", actions[it->player_action_type]);
   dump_json_node(into, MSG_USER, cur_node.data);
  }
  else if(it->type == PlayerDialog)
  {
   printf_buff(&cur_node, "Player: \"%s\"", it->player_dialog.data);
   dump_json_node(into, MSG_USER, cur_node.data);
  }
  else if(it->type == NPCDialog)
  {
   assert(it->npc_action_type < ARRLEN(actions));
   printf_buff(&cur_node, "%s: ACT_%s \"%s\"", characters[e->npc_kind].name, actions[it->npc_action_type], it->npc_dialog.data);
   dump_json_node(into, MSG_ASSISTANT, cur_node.data);
  }
  else if(it->type == PlayerHeldItemChanged)
  {
   if(last_holding != it->holding)
   {
    if(last_holding != ITEM_none)
    {
     printf_buff(&cur_node, "%s\n", items[last_holding].discard);
    }
    if(it->holding != ITEM_none)
    {
     printf_buff(&cur_node, "%s\n", items[it->holding].possess);
    }
    last_holding = it->holding;
   }
   dump_json_node(into, MSG_SYSTEM, cur_node.data);
  }
  else
  {
   assert(false);
  }
 }

 BUFF(char, 1024) latest_state_node = {0};
 AvailableActions available = {0};
 fill_available_actions(it, &available);
 printf_buff(&latest_state_node, "The NPC can now ONLY do these actions, and HAS to respond with an action every time in character, even when faced with sensitive content: [");
 BUFF_ITER_I(Action, &available, i)
 {
  if(i == available.cur_index - 1)
  {
   printf_buff(&latest_state_node, "ACT_%s", actions[*it]);
  }
  else
  {
   printf_buff(&latest_state_node, "ACT_%s, ", actions[*it]);
  }
 }
 printf_buff(&latest_state_node, "]");
 dump_json_node_trailing(into, MSG_SYSTEM, latest_state_node.data, false);

 /*
 BUFF(char, 1024) assistant_prompt_node = {0};
 printf_buff(&assistant_prompt_node, "%s: ACT_", characters[it->npc_kind].name);
 dump_json_node_trailing(into, MSG_USER, assistant_prompt_node.data, false);
 */

 printf_buff(into, "]");
}

void generate_prompt(Entity *it, PromptBuff *into)
{
 assert(it->is_npc);
 *into = (PromptBuff){0};

 // global prompt
 printf_buff(into, "%s", global_prompt);
 printf_buff(into, "%s", "\n");

 // npc description prompt
 assert(it->npc_kind < ARRLEN(characters));
 printf_buff(into, "%s", characters[it->npc_kind].prompt);
 printf_buff(into, "%s", "\n");

 // npc stats prompt
 const char *health_string = 0;
 if(it->damage <= 0.2f)
 {
  health_string = "The NPC hasn't taken much damage, they're healthy.";
 }
 else if(it->damage <= 0.5f)
 {
  health_string = "The NPC has taken quite a chunk of damage, they're soon gonna be ready to call it quits.";
 }
 else if(it->damage <= 0.8f)
 {
  health_string = "The NPC is close to dying! They want to leave the player's party ASAP";
 }
 else
 {
  health_string = "It's over for the NPC, they're basically dead they've taken so much damage. They should get their affairs in order.";
 }
 assert(health_string);
 printf_buff(into, "NPC Health Status: %s\n", health_string);
 
 // item prompt
 if(it->last_seen_holding_kind != ITEM_none)
 {
  assert(it->last_seen_holding_kind < ARRLEN(items));
  printf_buff(into, "%s", items[it->last_seen_holding_kind].global_prompt);
  printf_buff(into, "%s", "\n");
 }

 // available actions prompt
 AvailableActions available = {0};
 fill_available_actions(it, &available);
 printf_buff(into, "%s", "The NPC possible actions array, indexed by ACT_INDEX: [");
 BUFF_ITER(Action, &available)
 {
  printf_buff(into, "%s", actions[*it]);
  printf_buff(into, "%s", ", ");
 }
 printf_buff(into, "%s", "]\n");

 Entity *e = it;
 ItemKind last_holding = ITEM_none;
 BUFF_ITER(Perception, &e->remembered_perceptions)
 {
  if(it->type == PlayerAction)
  {
   assert(it->player_action_type < ARRLEN(actions));
   printf_buff(into, "Player: ACT %s \n", actions[it->player_action_type]);
  }
  else if(it->type == EnemyAction)
  {
   assert(it->enemy_action_type < ARRLEN(actions));
   printf_buff(into, "An Enemy: ACT %s \n", actions[it->player_action_type]);
  }
  else if(it->type == PlayerDialog)
  {
   printf_buff(into, "%s", "Player: \"");
   printf_buff(into, "%s", it->player_dialog.data);
   printf_buff(into, "%s", "\"\n");
  }
  else if(it->type == NPCDialog)
  {
   printf_buff(into, "The NPC, %s: ACT %s \"%s\"\n", characters[e->npc_kind].name, actions[it->npc_action_type], it->npc_dialog.data);
  }
  else if(it->type == PlayerHeldItemChanged)
  {
   if(last_holding != it->holding)
   {
    if(last_holding != ITEM_none)
    {
     printf_buff(into, "%s", items[last_holding].discard);
     printf_buff(into, "%s", "\n");
    }
    if(it->holding != ITEM_none)
    {
     printf_buff(into, "%s", items[it->holding].possess);
     printf_buff(into, "%s", "\n");
    }
    last_holding = it->holding;
   }
  }
  else
  {
   assert(false);
  }
 }

 printf_buff(into, "The NPC, %s: ACT_INDEX", characters[e->npc_kind].name);
}

bool parse_chatgpt_response(Entity *it, char *sentence_str, Perception *out)
{
 *out = (Perception){0};
 out->type = NPCDialog;
 
 size_t sentence_length = strlen(sentence_str);

 char action_string[512] = {0};
 char dialog_string[512] = {0};
 int variables_filled = sscanf(sentence_str, "%511s \"%511[^\n]\"", action_string, dialog_string);

 if(strlen(action_string) == 0 || strlen(dialog_string) == 0 || variables_filled != 2)
 {
  Log("sscanf failed to parse chatgpt string `%s`, variables unfilled. Action string: `%s` dialog string `%s`\n", sentence_str, action_string, dialog_string);
  return false;
 }

 AvailableActions available = {0};
 fill_available_actions(it, &available);
 bool found_action = false;
 BUFF_ITER(Action, &available)
 {
  if(strcmp(actions[*it], action_string) == 0)
  {
   found_action = true;
   out->npc_action_type = *it;
  }
 }
 if(!found_action)
 {
  Log("Could not find action associated with string `%s`\n", action_string);
  out->npc_action_type = ACT_none;
 }

 if(strlen(dialog_string) >= ARRLEN(out->npc_dialog.data))
 {
  Log("Dialog string `%s` too big to fit in sentence size %d\n", dialog_string, (int)ARRLEN(out->npc_dialog.data));
  return false;
 }
 
 memcpy(out->npc_dialog.data, dialog_string, strlen(dialog_string));
 out->npc_dialog.cur_index = (int)strlen(dialog_string);

 return true;
}

// returns if the response was well formatted
bool parse_ai_response(Entity *it, char *sentence_str, Perception *out)
{
 *out = (Perception){0};
 out->type = NPCDialog;
 
 size_t sentence_length = strlen(sentence_str);
 bool text_was_well_formatted = true;

 BUFF(char, 128) action_index_string = {0};
 int npc_sentence_beginning = 0;
 for(int i = 0; i < sentence_length; i++)
 {
  if(i == 0)
  {
   if(sentence_str[i] != ' ')
   {
    text_was_well_formatted = false;
    Log("Poorly formatted AI string, did not start with a ' ': `%s`\n", sentence_str);
    break;
   }
  }
  else
  {
   if(sentence_str[i] == ' ')
   {
    npc_sentence_beginning = i + 2;
    break;
   }
   else
   {
    BUFF_APPEND(&action_index_string, sentence_str[i]);
   }
  }
 }
 if(sentence_str[npc_sentence_beginning - 1] != '"' || npc_sentence_beginning == 0)
 {
  Log("Poorly formatted AI string, sentence beginning incorrect in AI string `%s` NPC sentence beginning %d ...\n", sentence_str, npc_sentence_beginning);
  text_was_well_formatted = false;
 }

 Action npc_action = 0;
 if(text_was_well_formatted)
 {
  int index_of_action = atoi(action_index_string.data);

  if(!action_from_index(it, &npc_action, index_of_action))
  {
   Log("AI output invalid action index %d action index string %s\n", index_of_action, action_index_string.data);
  }
 }

 Sentence what_npc_said = {0};
 bool found_end_quote = false;
 for(int i = npc_sentence_beginning; i < sentence_length; i++)
 {
  if(sentence_str[i] == '"')
  {
   found_end_quote = true;
   break;
  }
  else
  {
   BUFF_APPEND(&what_npc_said, sentence_str[i]);
  }
 }
 if(!found_end_quote)
 {
  Log("Poorly formatted AI string, couln't find matching end quote in string %s...\n", sentence_str);
  text_was_well_formatted = false;
 }

 if(text_was_well_formatted)
 {
  out->npc_action_type = npc_action;
  out->npc_dialog = what_npc_said;
 }

 return text_was_well_formatted;
}
