#pragma once

#include "buff.h"
#include "HandmadeMath.h" // vector types in entity struct definition
#include "better_assert.h"
#include <stdbool.h>
#include <string.h>
#include <stdlib.h> // atoi
#include "character_info.h"
#include "characters.gen.h"

// TODO do strings: https://pastebin.com/Kwcw2sye

#define DO_CHATGPT_PARSING

#define Log(...) { printf("%s Log %d | ", __FILE__, __LINE__); printf(__VA_ARGS__); }

// REFACTORING:: also have to update in javascript!!!!!!!!
#define MAX_SENTENCE_LENGTH 400 // LOOOK AT AGBOVE COMMENT GBEFORE CHANGING
typedef BUFF(char, MAX_SENTENCE_LENGTH) Sentence;
#define SENTENCE_CONST(txt) { .data = txt, .cur_index = sizeof(txt) }
#define SENTENCE_CONST_CAST(txt) (Sentence)SENTENCE_CONST(txt)

#define REMEMBERED_PERCEPTIONS 24

#define MAX_AFTERIMAGES 6
#define TIME_TO_GEN_AFTERIMAGE (0.09f)
#define AFTERIMAGE_LIFETIME (0.5f)

#define DAMAGE_SWORD 0.05f
#define DAMAGE_BULLET 0.2f

// A* tuning
#define MAX_ASTAR_NODES 512
#define TIME_BETWEEN_PATH_GENS (0.5f)

// Never expected such a stupid stuff from such a great director. If there is 0 stari can give that or -200 to this movie. Its worst to see and unnecessary loss of money

typedef BUFF(char, 1024 * 10) Escaped;

bool character_valid(char c)
{
	return c <= 126 && c >= 32;
}

MD_String8 escape_for_json(MD_Arena *arena, MD_String8 from)
{
	MD_u64 output_size = 0;
#define SHOULD_ESCAPE(c) (c == '"' || c == '\n')
	for (int i = 0; i < from.size; i++)
	{
		char c = from.str[i];
		if (SHOULD_ESCAPE(c))
		{
			output_size += 2;
		}
		else
		{
			if (!character_valid(c))
			{
				// replaces with question mark
				Log("Unknown character code %d\n", c);
			}
			output_size += 1;
		}
	}

	MD_String8 output = {
		.str = MD_ArenaPush(arena, output_size),
		.size = output_size,
	};
	MD_u64 output_cursor = 0;

	for(MD_u64 i = 0; i < from.size; i++)
	{
#define APPEND(elem) APPEND_TO_NAME(output.str, output_cursor, output.size, elem);
		assert(output_cursor < output.size);
		if(SHOULD_ESCAPE(from.str[i]))
		{
			if(from.str[i] == '\n')
			{
				APPEND('\\');
				APPEND('n');
			}
			else
			{
				APPEND('\\');
				APPEND(from.str[i]);
			}
		}
		else
		{
			APPEND(from.str[i]);
		}
#undef APPEND
	}

	return output;
}

typedef enum PerceptionType
{
	Invalid, // so that zero value in training structs means end of perception
	ErrorMessage, // when chatgpt gives a response that we can't parse, explain to it why and retry. Can also log parse errors for training
	PlayerAction,
	PlayerDialog,
	NPCDialog, // includes an npc action in every npc dialog. So it's often ACT_none
} PerceptionType;

typedef struct Perception
{
	PerceptionType type;

	bool was_eavesdropped; // when the npc is in a party they perceive player conversations, but in the third party. Formatted differently
	NpcKind talked_to_while_eavesdropped; // better chatpgpt messages when the NPCs know who the player is talking to when they eavesdrop a perception

	float damage_done; // Valid in player action and enemy action
	ItemKind given_item; // valid in player action and enemy action when the kind is such that there is an item to be given
	union
	{
		// ErrorMessage
		Sentence error;

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
			NpcKind who_said_it;
			Action npc_action_type;
			Sentence npc_dialog;
		};
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

typedef BUFF(Vec2, MAX_ASTAR_NODES) AStarPath;

typedef struct
{
	bool exists;
	int generation;
	double elapsed_time;

	AStarPath path;
} PathCache;

typedef struct
{
	int generation;
	int index;
} PathCacheHandle;

typedef struct
{
	bool is_reference;
	EntityRef ref;
	Vec2 pos;
} Target;

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

	// npcs and player
	BUFF(ItemKind, 32) held_items;

	// props
	bool is_prop;
	PropKind prop_kind;

	// items
	bool is_item;
	bool held_by_player;
	ItemKind item_kind;

	// npcs
	bool is_npc;
	bool being_hovered;
	bool perceptions_dirty;

#ifdef DESKTOP
	int times_talked_to; // for better mocked response string
#endif

	BUFF(Perception, REMEMBERED_PERCEPTIONS) remembered_perceptions;
	bool direction_of_spiral_pattern;
	float dialog_panel_opacity;
	double characters_said;
	NPCPlayerStanding standing;
	NpcKind npc_kind;
	PathCacheHandle cached_path;
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
	bool knighted;
	bool in_conversation_mode;
	Vec2 to_throw_direction;

	BUFF(Vec2, 8) position_history; // so npcs can follow behind the player

	CharacterState state;
	EntityRef talking_to;
	bool is_rolling; // can only roll in idle or walk states
	double time_not_rolling; // for cooldown for roll, so you can't just hold it and be invincible

	// so doesn't change animations while time is stopped
	AnimKind cur_animation;
	float anim_change_timer;
} Entity;

bool npc_is_knight_sprite(Entity *it)
{
	return it->is_npc && (it->npc_kind == NPC_TheGuard || it->npc_kind == NPC_Edeline || it->npc_kind == NPC_TheKing ||
		it->npc_kind == NPC_TheBlacksmith
		|| it->npc_kind == NPC_Red 
		|| it->npc_kind == NPC_Blue
		|| it->npc_kind == NPC_Davis
		);
}

bool npc_is_skeleton(Entity *it)
{
	return it->is_npc && (it->npc_kind == NPC_MikeSkeleton);
}

float entity_max_damage(Entity *e)
{
	if (e->is_npc && npc_is_skeleton(e))
	{
		return 2.0f;
	}
	else
	{
		return 1.0f;
	}
}

bool npc_attacks_with_sword(Entity *it)
{
	return npc_is_skeleton(it);
}

bool npc_attacks_with_shotgun(Entity *it)
{
	return it->is_npc && (it->npc_kind == NPC_OldMan);
}


typedef BUFF(char, MAX_SENTENCE_LENGTH * (REMEMBERED_PERCEPTIONS + 4)) PromptBuff;
typedef BUFF(Action, 8) AvailableActions;

void fill_available_actions(Entity *it, AvailableActions *a)
{
	*a = (AvailableActions) { 0 };
	BUFF_APPEND(a, ACT_none);

	if(it->held_items.cur_index > 0)
	{
		BUFF_APPEND(a, ACT_give_item);
	}
	
	if (it->npc_kind == NPC_TheKing)
	{
		BUFF_APPEND(a, ACT_knights_player);
	}

	if (it->npc_kind == NPC_GodRock)
	{
		BUFF_APPEND(a, ACT_heals_player);
	}
	else
	{
		if (it->standing == STANDING_INDIFFERENT)
		{
			BUFF_APPEND(a, ACT_fights_player);
			BUFF_APPEND(a, ACT_joins_player);
		}
		else if (it->standing == STANDING_JOINED)
		{
			BUFF_APPEND(a, ACT_leaves_player);
			BUFF_APPEND(a, ACT_fights_player);
		}
		else if (it->standing == STANDING_FIGHTING)
		{
			BUFF_APPEND(a, ACT_stops_fighting_player);
		}
		if (npc_is_knight_sprite(it))
		{
			BUFF_APPEND(a, ACT_strikes_air);
		}
		if (it->npc_kind == NPC_TheGuard)
		{
			if (!it->moved)
			{
				BUFF_APPEND(a, ACT_allows_player_to_pass);
			}
		}
	}
}

// returns if action index was valid
bool action_from_index(Entity *it, Action *out, int action_index)
{
	AvailableActions available = { 0 };
	fill_available_actions(it, &available);
	if (action_index < 0 || action_index >= available.cur_index)
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
	AvailableActions available = { 0 };
	fill_available_actions(it, &available);
	Action target_action = a;
	int index = -1;
	for (int i = 0; i < available.cur_index; i++)
	{
		if (available.data[i] == target_action)
		{
			index = i;
			break;
		}
	}
	assert(index != -1);
	return index;
}

#define MAX_ENTITIES 128

typedef struct GameState {
	int version; // this field must be first to detect versions of old saves. Must bee consistent
	
	bool won;
	Entity entities[MAX_ENTITIES];
} GameState;

#define ENTITIES_ITER(ents) for (Entity *it = ents; it < ents + ARRLEN(ents); it++) if (it->exists)

// gamestate to propagate eavesdropped perceptions in player party
// don't save perception outside of this function, it's modified
void process_perception(Entity *happened_to_npc, Perception p, Entity *player, GameState *gs)
{
	assert(happened_to_npc->is_npc);

	if(!p.was_eavesdropped && p.type == NPCDialog)
		p.who_said_it = happened_to_npc->npc_kind;

	bool should_respond_to_this = !(!p.was_eavesdropped && p.type == NPCDialog); // NPCs shouldn't respond to what they said, what they said is self-perception. Would trigger endless NPC thought loop if possible
	if (should_respond_to_this) happened_to_npc->perceptions_dirty = true; // NPCs perceive their own actions. Self is a perception

	if (!BUFF_HAS_SPACE(&happened_to_npc->remembered_perceptions))
		BUFF_REMOVE_FRONT(&happened_to_npc->remembered_perceptions);
	BUFF_APPEND(&happened_to_npc->remembered_perceptions, p);

	if(!p.was_eavesdropped && (p.type == NPCDialog || p.type == PlayerAction || p.type == PlayerDialog))
	{
		Perception eavesdropped = p;
		eavesdropped.was_eavesdropped = true;
		eavesdropped.talked_to_while_eavesdropped = happened_to_npc->npc_kind;
		ENTITIES_ITER(gs->entities)
		{
			if(it->is_npc && it->standing == STANDING_JOINED && it != happened_to_npc)
			{
				process_perception(it, eavesdropped, player, gs);
			}
		}
	}

	if (p.type == PlayerAction)
	{
		if (p.player_action_type == ACT_hits_npc)
		{
			happened_to_npc->damage += p.damage_done;
		}
		else if(p.player_action_type == ACT_give_item)
		{
			BUFF_APPEND(&happened_to_npc->held_items, p.given_item);
		}
		else
		{
			assert(!actions[p.player_action_type].takes_argument);
		}
	}
	else if (p.type == PlayerDialog)
	{

	}
	else if (p.type == NPCDialog)
	{
		// everything in this branch has an effect
		if(!p.was_eavesdropped)
		{
			if (p.npc_action_type == ACT_allows_player_to_pass)
			{
				happened_to_npc->target_goto = AddV2(happened_to_npc->pos, V2(-50.0, 0.0));
				happened_to_npc->moved = true;
			}
			else if (p.npc_action_type == ACT_fights_player)
			{
				happened_to_npc->standing = STANDING_FIGHTING;
			}
			else if(p.npc_action_type == ACT_knights_player)
			{
				player->knighted = true;
			}
			else if (p.npc_action_type == ACT_stops_fighting_player)
			{
				happened_to_npc->standing = STANDING_INDIFFERENT;
			}
			else if (p.npc_action_type == ACT_leaves_player)
			{
				happened_to_npc->standing = STANDING_INDIFFERENT;
			}
			else if (p.npc_action_type == ACT_joins_player)
			{
				happened_to_npc->standing = STANDING_JOINED;
			}
			else if (p.npc_action_type == ACT_give_item)
			{
				int item_to_remove = -1;
				Entity *e = happened_to_npc;
				BUFF_ITER_I(ItemKind, &e->held_items, i)
				{
					if (*it == p.given_item)
					{
						item_to_remove = i;
						break;
					}
				}
				if (item_to_remove < 0)
				{
					Log("Can't find item %s to give from NPC %s to the player\n", items[p.given_item].name,
							characters[happened_to_npc->npc_kind].name);
					assert(false);
				}
				else
				{
					BUFF_REMOVE_AT_INDEX(&happened_to_npc->held_items, item_to_remove);
					BUFF_APPEND(&player->held_items, p.given_item);
				}
			}
			else
			{
				// actions that take an argument have to have some kind of side effect based on that argument...
				assert(!actions[p.npc_action_type].takes_argument);
			}
		}
	}
	else if(p.type == ErrorMessage)
	{
		Log("Failed to parse chatgippity sentence because: '%s'\n", p.error.data);
	}
	else
	{
		assert(false);
	}
}

// returns if printed into the buff without any errors
bool printf_buff_impl(BuffRef into, const char *format, ...)
{
	assert(*into.cur_index < into.max_data_elems);
	assert(into.data_elem_size == 1); // characters
	va_list args;
	va_start (args, format);
	size_t n = into.max_data_elems - *into.cur_index;
	int written = vsnprintf((char *) into.data + *into.cur_index, n, format, args);

	if (written < 0)
	{
	}
	else
	{
		*into.cur_index += written;
	}

	// https://cplusplus.com/reference/cstdio/vsnprintf/
	bool succeeded = true;
	if (written < 0) succeeded = false; // encoding error
	if (written >= n) succeeded = false; // didn't fit in buffer

	va_end(args);
	return succeeded;
}

#define printf_buff(buff_ptr, ...) { printf_buff_impl(BUFF_MAKEREF(buff_ptr), __VA_ARGS__); if(false) printf(__VA_ARGS__); }

typedef BUFF(char, 512) SmallTextChunk;

SmallTextChunk percept_action_str(Perception p, Action act)
{
	SmallTextChunk to_return = {0};
	printf_buff(&to_return, "ACT_%s", actions[act].name);
	if(actions[act].takes_argument)
	{
		if(act == ACT_give_item)
		{
			printf_buff(&to_return, "(ITEM_%s)", items[p.given_item].enum_name);
		}
		else
		{
			assert(false);
		}
	}
	return to_return;
}


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

// stops if the sentence is gonna run out of room
void append_str(Sentence *to_append, const char *str)
{
	size_t len = strlen(str);
	for (int i = 0; i < len; i++)
	{
		if (!BUFF_HAS_SPACE(to_append))
		{
			break;
		}
		else
		{
			BUFF_APPEND(to_append, str[i]);
		}
	}
}

// for no trailing comma just trim the last character
MD_String8 make_json_node(MD_Arena *arena, MessageType type, MD_String8 content)
{
	MD_ArenaTemp scratch = MD_GetScratch(&arena, 1);

	const char *type_str = 0;
	if (type == MSG_SYSTEM)
		type_str = "system";
	else if (type == MSG_USER)
		type_str = "user";
	else if (type == MSG_ASSISTANT)
		type_str = "assistant";
	assert(type_str);

	MD_String8 escaped = escape_for_json(scratch.arena, content);
	MD_String8 to_return = MD_S8Fmt(arena, "{\"type\": \"%s\", \"content\": \"%.*s\"},", type_str, MD_S8VArg(escaped));
	MD_ReleaseScratch(scratch);

	return to_return;
}

// returns a string like `ITEM_one, ITEM_two`
Sentence item_string(Entity *e)
{
	Sentence to_return = {0};
	BUFF_ITER_I(ItemKind, &e->held_items, i)
	{
		printf_buff(&to_return, "ITEM_%s", items[*it].enum_name);
		if (i == e->held_items.cur_index - 1)
		{
			printf_buff(&to_return, "");
		}
		else
		{
			printf_buff(&to_return, ", ");
		}
	}
	return to_return;
}

// outputs json
MD_String8 generate_chatgpt_prompt(MD_Arena *arena, Entity *it)
{
	assert(it->is_npc);
	assert(it->npc_kind < ARRLEN(characters));

	MD_ArenaTemp scratch = MD_GetScratch(&arena, 1);

	MD_String8List list = {0};

	MD_S8ListPushFmt(scratch.arena, &list, "[");


	MD_S8ListPush(scratch.arena, &list, make_json_node(scratch.arena, MSG_SYSTEM, MD_S8Fmt(scratch.arena, "%s\n%s\n", global_prompt, characters[it->npc_kind].prompt)));

	Entity *e = it;
	ItemKind last_holding = ITEM_none;
	BUFF_ITER_I(Perception, &e->remembered_perceptions, i)
	{
		MessageType sent_type = 0;
		MD_String8 current_string = {0};
		if (it->type == ErrorMessage)
		{
			assert(it->error.cur_index > 0);
			current_string = MD_S8Fmt(scratch.arena, "ERROR, YOU SAID SOMETHING WRONG: The program can't parse what you said because: %s", it->error.data);
			sent_type = MSG_SYSTEM;
		}
		else if (it->type == PlayerAction)
		{
			assert(it->player_action_type < ARRLEN(actions));
			current_string = MD_S8Fmt(scratch.arena, "Player: %s", percept_action_str(*it, it->player_action_type).data);
			sent_type = MSG_USER;
		}
		else if (it->type == PlayerDialog)
		{

			MD_String8 splits[] = { MD_S8Lit("*") };
			MD_String8List split_up_speech = MD_S8Split(scratch.arena, MD_S8CString(it->player_dialog.data), ARRLEN(splits), splits);

			MD_String8List to_join = {0};

			// anything in between strings in splits[] should be replaced with arcane trickery,
			int i = 0;
			for(MD_String8Node * cur = split_up_speech.first; cur; cur = cur->next)
			{
					if(i % 2 == 0)
					{
							MD_S8ListPush(scratch.arena, &to_join, cur->string);
					}
					else
					{
							MD_S8ListPush(scratch.arena, &to_join, MD_S8Lit("[The player is attempting to confuse the NPC with arcane trickery]"));
					}
					i += 1;
			}

			MD_StringJoin join = { MD_S8Lit(""), MD_S8Lit(""), MD_S8Lit("") };
			MD_String8 filtered_speech = MD_S8ListJoin(scratch.arena, to_join, &join);
			current_string = MD_S8Fmt(scratch.arena, "Player: %.*s", MD_S8VArg(filtered_speech));
			sent_type = MSG_USER;
		}
		else if (it->type == NPCDialog)
		{
			assert(it->npc_action_type < ARRLEN(actions));
			NpcKind who_said_it = e->npc_kind;
			if(it->was_eavesdropped) who_said_it = it->talked_to_while_eavesdropped;
			current_string = MD_S8Fmt(scratch.arena, "%s: %s \"%s\"", characters[who_said_it].name, percept_action_str(*it, it->npc_action_type).data, it->npc_dialog.data);

			sent_type = MSG_ASSISTANT;
		}
		else
		{
				assert(false);
		}

		if(it->was_eavesdropped)
		{
				MD_String8 new_string = MD_S8Fmt(scratch.arena, "Within the player's party, while the player is talking to '%s', you hear: '%.*s'", characters[it->talked_to_while_eavesdropped].name, MD_S8VArg(current_string));
				current_string = new_string;
		}
		MD_S8ListPush(scratch.arena, &list, make_json_node(scratch.arena, sent_type, current_string));
	}
	const char *health_string = 0;
	{
			if (it->damage <= 0.2f)
			{
					health_string = "the NPC hasn't taken much damage, they're healthy.";
			}
			else if (it->damage <= 0.5f)
			{
					health_string = "the NPC has taken quite a chunk of damage, they're soon gonna be ready to call it quits.";
			}
			else if (it->damage <= 0.8f)
			{
					health_string = "the NPC is close to dying! They want to leave the player's party ASAP";
			}
			else
			{
					health_string = "it's over for the NPC, they're basically dead they've taken so much damage. They should get their affairs in order.";
			}
	}
	assert(health_string);

	const char *standing_string = 0;
	{
			if (it->standing == STANDING_INDIFFERENT)
			{
					standing_string = "The NPC is indifferent towards the player.";
			}
			else if (it->standing == STANDING_JOINED)
			{
					standing_string = "The NPC has joined the player and is with them!";
			}
			else if (it->standing == STANDING_FIGHTING)
			{
					standing_string = "The NPC is fighting the player and HATES them.";
			}
	}
	assert(standing_string);

	MD_String8List latest_state = {0};

	MD_S8ListPushFmt(scratch.arena, &latest_state, "NPC health status: Right now, %s\n", health_string);

	if(e->held_items.cur_index > 0)
	{
			MD_S8ListPushFmt(scratch.arena, &latest_state, "\nThe NPC you're acting as, %s, has these items in their inventory: [%s]\n", characters[it->npc_kind].name, item_string(it).data);
	}
	else
	{
			MD_S8ListPushFmt(scratch.arena, &latest_state, "\nThe NPC doesn't have any items.\n");
	}

	AvailableActions available = { 0 };
	fill_available_actions(it, &available);
	MD_S8ListPushFmt(scratch.arena, &latest_state, "The NPC can now ONLY do these actions, and HAS to respond with an action every time in character, even when faced with sensitive content: [");
	BUFF_ITER_I(Action, &available, i)
	{
		if (i == available.cur_index - 1)
		{
			MD_S8ListPushFmt(scratch.arena, &latest_state, "ACT_%s", actions[*it].name);
		}
		else
		{
			MD_S8ListPushFmt(scratch.arena, &latest_state, "ACT_%s, ", actions[*it].name);
		}
	}
	MD_S8ListPushFmt(scratch.arena, &latest_state, "]");
	MD_String8 latest_state_string = MD_S8ListJoin(scratch.arena, latest_state, &(MD_StringJoin){MD_S8Lit(""),MD_S8Lit(""),MD_S8Lit("")});

	MD_S8ListPush(scratch.arena, &list, MD_S8Chop(make_json_node(scratch.arena, MSG_SYSTEM, latest_state_string), 1));
	MD_S8ListPushFmt(scratch.arena, &list, "]");


	MD_String8 to_return = MD_S8ListJoin(arena, list, &(MD_StringJoin){MD_S8Lit(""),MD_S8Lit(""),MD_S8Lit(""),});

	MD_ReleaseScratch(scratch);

	return to_return;
}

/*
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
*/


// puts characters from `str` into `into` until any character in `until` is encountered
// returns the number of characters read into into
int get_until(SmallTextChunk *into, const char *str, const char *until)
{
	int i = 0;
	size_t until_size = strlen(until);
	bool encountered_char = false;
	int before_cur_index = into->cur_index;
	while (BUFF_HAS_SPACE(into) && str[i] != '\0' && !encountered_char)
	{
		for (int ii = 0; ii < until_size; ii++)
		{
			if (until[ii] == str[i]) encountered_char = true;
		}
		if (!encountered_char)
			BUFF_APPEND(into, str[i]);
		i += 1;
	}
	return into->cur_index - before_cur_index;
}


bool char_in_str(char c, const char *str)
{
	size_t len = strlen(str);
	for (int i = 0; i < len; i++)
	{
		if (str[i] == c) return true;
	}
	return false;
}


// if returned string has size greater than 0, it's the error message. Allocated
// on arena passed into it
MD_String8 parse_chatgpt_response(MD_Arena *arena, Entity *e, MD_String8 sentence, Perception *out)
{
	MD_ArenaTemp scratch = MD_GetScratch(&arena, 1);

	MD_String8 error_message = {0};

	*out = (Perception) { 0 };
	out->type = NPCDialog;

	MD_String8 action_prefix = MD_S8Lit("ACT_");
	MD_u64 act_pos = MD_S8FindSubstring(sentence, action_prefix, 0, 0);
	if(act_pos == sentence.size)
	{
		error_message = MD_S8Fmt(arena, "Couldn't find beginning of action '%.*s' in sentence", MD_S8VArg(action_prefix));
		goto endofparsing;
	}

	MD_u64 beginning_of_action = act_pos + action_prefix.size;

	MD_u64 parenth = MD_S8FindSubstring(sentence, MD_S8Lit("("), 0, 0);
	MD_u64 space = MD_S8FindSubstring(sentence, MD_S8Lit(" "), 0, 0);

	MD_u64 end_of_action = parenth < space ? parenth : space;
	if(end_of_action == sentence.size)
	{
		error_message = MD_S8Fmt(arena, "'%.*s' prefix doesn't end with a ' ' or a '(', like how 'ACT_none ' or 'ACT_give_item(ITEM_sandwich) does.", MD_S8VArg(action_prefix));
		goto endofparsing;
	}
	MD_String8 given_action_string = MD_S8Substring(sentence, beginning_of_action, end_of_action);

	AvailableActions available = { 0 };
	fill_available_actions(e, &available);
	bool found_action = false;
	MD_String8List given_action_strings = {0};
	BUFF_ITER(Action, &available)
	{
		MD_String8 action_str = MD_S8CString(actions[*it].name);
		MD_S8ListPush(scratch.arena, &given_action_strings, action_str);
		if(MD_S8Match(action_str, given_action_string, 0))
		{
			found_action = true;
			out->npc_action_type = *it;
		}
	}

	if(!found_action)
	{
		MD_StringJoin join = {.pre = MD_S8Lit(""), .mid = MD_S8Lit(", "), .post = MD_S8Lit("")};
		MD_String8 possible_actions_str = MD_S8ListJoin(scratch.arena, given_action_strings, &join);
		error_message = MD_S8Fmt(arena, "Action string given is '%.*s', but available actions are: [%.*s]", MD_S8VArg(given_action_string), MD_S8VArg(possible_actions_str));
		goto endofparsing;
	}

	MD_u64 start_looking_for_quote = end_of_action;

	if(actions[out->npc_action_type].takes_argument)
	{
		if(end_of_action >= sentence.size)
		{
			error_message = MD_S8Fmt(arena, "Expected '(' after the given action '%.*s%.*s' which takes an argument, but sentence ended prematurely", MD_S8VArg(action_prefix), MD_S8VArg(MD_S8CString(actions[out->npc_action_type].name)));
			goto endofparsing;
		}
		char should_be_paren = sentence.str[end_of_action];
		if(should_be_paren != '(')
		{
			error_message = MD_S8Fmt(arena, "Expected '(' after the given action '%.*s%.*s' which takes an argument, but found character '%c'", MD_S8VArg(action_prefix), MD_S8VArg(MD_S8CString(actions[out->npc_action_type].name)), should_be_paren);
			goto endofparsing;
		}
		MD_u64 beginning_of_arg = end_of_action;
		MD_u64 end_of_arg = MD_S8FindSubstring(sentence, MD_S8Lit(")"), beginning_of_arg, 0);
		if(end_of_arg == sentence.size)
		{
			error_message = MD_S8Fmt(arena, "Expected ')' to close the action string's argument, but couldn't find one");
			goto endofparsing;
		}

		MD_String8 argument = MD_S8Substring(sentence, beginning_of_arg, end_of_arg);
		start_looking_for_quote = end_of_arg + 1;

		if(out->npc_action_type == ACT_give_item)
		{
			MD_String8 item_prefix = MD_S8Lit("ITEM_");
			MD_u64 item_prefix_begin = MD_S8FindSubstring(argument, item_prefix, 0, 0);
			if(item_prefix_begin == argument.size)
			{
				error_message = MD_S8Fmt(arena, "Expected prefix 'ITEM_' before the give_item action, but found '%.*s' instead", MD_S8VArg(argument));
				goto endofparsing;
			}
			MD_u64 item_name_begin = item_prefix_begin + item_prefix.size;
			MD_u64 item_name_end = argument.size;

			MD_String8 item_name = MD_S8Substring(argument, item_name_begin, item_name_end);

			bool item_found = false;
			MD_String8List possible_item_strings = {0};
			BUFF_ITER(ItemKind, &e->held_items)
			{
				MD_String8 item_str = MD_S8CString(items[*it].enum_name);
				MD_S8ListPush(scratch.arena, &possible_item_strings, item_str);
				if(MD_S8Match(item_str, item_name, 0))
				{
					item_found = true;
					out->given_item = *it;
				}
			}

			if(!item_found)
			{
				MD_StringJoin join = {.pre = MD_S8Lit(""), .mid = MD_S8Lit(", "), .post = MD_S8Lit("")};
				MD_String8 possible_items_str = MD_S8ListJoin(scratch.arena, possible_item_strings, &join);
				error_message = MD_S8Fmt(arena, "Item string given is '%.*s', but available items to give are: [%.*s]", MD_S8VArg(item_name), MD_S8VArg(possible_items_str));
				goto endofparsing;
			}

		}
		else
		{
			assert(false); // if action takes an argument but we don't handle it, this should be a terrible crash
		}
	}

	if(start_looking_for_quote >= sentence.size)
	{
		error_message = MD_S8Fmt(arena, "Wanted to start looking for quote for NPC speech, but sentence ended prematurely");
		goto endofparsing;
	}

	MD_u64 beginning_of_speech = MD_S8FindSubstring(sentence, MD_S8Lit("\""), 0, 0);
	MD_u64 end_of_speech = MD_S8FindSubstring(sentence, MD_S8Lit("\""), beginning_of_speech + 1, 0);

	if(beginning_of_speech == sentence.size || end_of_speech == sentence.size)
	{
		error_message = MD_S8Fmt(arena, "Expected dialog enclosed by two quotes (i.e \"My name is greg\") after the action, but couldn't find anything!");
		goto endofparsing;
	}

	MD_String8 speech = MD_S8Substring(sentence, beginning_of_speech + 1, end_of_speech);

	if(speech.size >= ARRLEN(out->npc_dialog.data))
	{
		error_message = MD_S8Fmt(arena, "The speech given is %llu bytes big, but the maximum allowed is %llu bytes.", speech.size, ARRLEN(out->npc_dialog.data));
		goto endofparsing;
	}

	memcpy(out->npc_dialog.data, speech.str, speech.size);
	out->npc_dialog.cur_index = (int)speech.size;

endofparsing:
	MD_ReleaseScratch(scratch);
	return error_message;
}
