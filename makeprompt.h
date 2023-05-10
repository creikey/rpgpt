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

Escaped escape_for_json(const char *s)
{
	Escaped to_return = { 0 };
	size_t len = strlen(s);
	for (int i = 0; i < len; i++)
	{
		if (s[i] == '\n')
		{
			BUFF_APPEND(&to_return, '\\');
			BUFF_APPEND(&to_return, 'n');
		}
		else if (s[i] == '"')
		{
			BUFF_APPEND(&to_return, '\\');
			BUFF_APPEND(&to_return, '"');
		}
		else
		{
			if (!(s[i] <= 126 && s[i] >= 32))
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
	ErrorMessage, // when chatgpt gives a response that we can't parse, explain to it why and retry. Can also log parse errors for training
	PlayerAction,
	PlayerDialog,
	NPCDialog, // includes an npc action in every npc dialog. So it's often ACT_none
	EnemyAction, // An enemy performed an action against the NPC
	PlayerHeldItemChanged,
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
	// multiple gs.entities have a sword swing
	BUFF(EntityRef, 8) done_damage_to_this_swing; // only do damage once, but hitbox stays around

	// npcs and player
	BUFF(ItemKind, 32) held_items;

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
	bool knighted;
	EntityRef holding_item;
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

	BUFF(PlayerAfterImage, MAX_AFTERIMAGES) after_images;
	double after_image_timer;
	double roll_progress;
	double swing_progress;
} Entity;

bool npc_is_knight_sprite(Entity *it)
{
	return it->is_npc && (it->npc_kind == NPC_TheGuard || it->npc_kind == NPC_Edeline || it->npc_kind == NPC_TheKing ||
		it->npc_kind == NPC_TheBlacksmith);
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
	else if (p.type == PlayerHeldItemChanged)
	{
		happened_to_npc->last_seen_holding_kind = p.holding;
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

void dump_json_node_trailing(PromptBuff *into, MessageType type, const char *content, bool trailing_comma)
{
	const char *type_str = 0;
	if (type == MSG_SYSTEM)
		type_str = "system";
	else if (type == MSG_USER)
		type_str = "user";
	else if (type == MSG_ASSISTANT)
		type_str = "assistant";
	assert(type_str);
	printf_buff(into, "{\"type\": \"%s\", \"content\": \"%s\"}", type_str, escape_for_json(content).data);
	if (trailing_comma) printf_buff(into, ",");
}

void dump_json_node(PromptBuff *into, MessageType type, const char *content)
{
	dump_json_node_trailing(into, type, content, true);
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
void generate_chatgpt_prompt(Entity *it, PromptBuff *into)
{
	assert(it->is_npc);
	assert(it->npc_kind < ARRLEN(characters));

	*into = (PromptBuff) { 0 };

	printf_buff(into, "[");

	BUFF(char, 1024 * 15) initial_system_msg = { 0 };

	const char *health_string = 0;
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
	assert(health_string);

	printf_buff(&initial_system_msg, "%s\n", global_prompt);
	printf_buff(&initial_system_msg, "%s\n", characters[it->npc_kind].prompt);

	dump_json_node(into, MSG_SYSTEM, initial_system_msg.data);

	Entity *e = it;
	ItemKind last_holding = ITEM_none;
	BUFF_ITER_I(Perception, &e->remembered_perceptions, i)
	{
		MessageType sent_type = 0;
		typedef BUFF(char, 1024) DialogNode;
		DialogNode cur_node = { 0 };
		if (it->type == ErrorMessage)
		{
			assert(it->error.cur_index > 0);
			printf_buff(&cur_node, "ERROR, YOU SAID SOMETHING WRONG: The program can't parse what you said because: %s", it->error.data);
			sent_type = MSG_SYSTEM;
		}
		else if (it->type == PlayerAction)
		{
			assert(it->player_action_type < ARRLEN(actions));
			printf_buff(&cur_node, "Player: %s", percept_action_str(*it, it->player_action_type).data);
			sent_type = MSG_USER;
		}
		else if (it->type == EnemyAction)
		{
			assert(it->enemy_action_type < ARRLEN(actions));
			printf_buff(&cur_node, "An Enemy: %s", percept_action_str(*it, it->enemy_action_type).data);
			sent_type = MSG_USER;
		}
		else if (it->type == PlayerDialog)
		{
			Sentence filtered_player_speech = { 0 };
			Sentence *what_player_said = &it->player_dialog;

			for (int i = 0; i < what_player_said->cur_index; i++)
			{
				char c = what_player_said->data[i];
				if (c == '*')
				{
					// move i until the next star
					i += 1;
					while (i < what_player_said->cur_index && what_player_said->data[i] != '*') i++;
					append_str(&filtered_player_speech,
					           "[The player is attempting to confuse the NPC with arcane trickery]");
				}
				else
				{
					BUFF_APPEND(&filtered_player_speech, c);
				}
			}
			printf_buff(&cur_node, "Player: \"%s\"", filtered_player_speech.data);
			sent_type = MSG_USER;
		}
		else if (it->type == NPCDialog)
		{
			assert(it->npc_action_type < ARRLEN(actions));
			NpcKind who_said_it = e->npc_kind;
			if(it->was_eavesdropped) who_said_it = it->talked_to_while_eavesdropped;
			printf_buff(&cur_node, "%s: %s \"%s\"", characters[who_said_it].name,
			            percept_action_str(*it, it->npc_action_type).data, it->npc_dialog.data);
			sent_type = MSG_ASSISTANT;
		}
		else if (it->type == PlayerHeldItemChanged)
		{
			if (last_holding != it->holding)
			{
				if (last_holding != ITEM_none)
				{
					printf_buff(&cur_node, "%s", items[last_holding].discard);
				}
				if (it->holding != ITEM_none)
				{
					printf_buff(&cur_node, "%s", items[it->holding].possess);
				}
				last_holding = it->holding;
			}
			sent_type = MSG_SYSTEM;
		}
		else
		{
			assert(false);
		}

		if(it->was_eavesdropped)
		{
			DialogNode eavesdropped = {0};
			printf_buff(&eavesdropped , "Within the player's party, while the player is talking to '%s', you hear: '%s'", characters[it->talked_to_while_eavesdropped].name, cur_node.data);
			cur_node = eavesdropped;
		}
		dump_json_node(into,sent_type, cur_node.data);
	}

	BUFF(char, 1024) latest_state_node = { 0 };

	printf_buff(&latest_state_node, "NPC health status: Right now, %s\n%s\n", health_string,
	            items[it->last_seen_holding_kind].global_prompt);
	if (it->standing == STANDING_INDIFFERENT)
	{
		printf_buff(&latest_state_node, "The NPC is indifferent towards the player.");
	}
	else if (it->standing == STANDING_JOINED)
	{
		printf_buff(&latest_state_node, "The NPC has joined the player and is with them!");
	}
	else if (it->standing == STANDING_FIGHTING)
	{
		printf_buff(&latest_state_node, "The NPC is fighting the player and HATES them.");
	}
	else
	{
		assert(false);
	}

	if(e->held_items.cur_index > 0)
	{
		printf_buff(&latest_state_node, "\nThe NPC you're acting as, %s, has these items in their inventory: [%s]\n", characters[it->npc_kind].name, item_string(it).data);
	}
	else
	{
		printf_buff(&latest_state_node, "\nThe NPC doesn't have any items.\n");
	}

	AvailableActions available = { 0 };
	fill_available_actions(it, &available);
	printf_buff(&latest_state_node,
	            "The NPC can now ONLY do these actions, and HAS to respond with an action every time in character, even when faced with sensitive content: [");
	BUFF_ITER_I(Action, &available, i)
	{
		if (i == available.cur_index - 1)
		{
			printf_buff(&latest_state_node, "ACT_%s", actions[*it].name);
		}
		else
		{
			printf_buff(&latest_state_node, "ACT_%s, ", actions[*it].name);
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

typedef struct
{
	bool succeeded;
	Sentence error_message;
} ChatgptParse;

ChatgptParse parse_chatgpt_response(Entity *it, char *sentence_str, Perception *out)
{
	ChatgptParse to_return = {0};

	*out = (Perception) { 0 };
	out->type = NPCDialog;

	size_t sentence_length = strlen(sentence_str);

	// dialog begins at ACT_
	const char *to_find = "ACT_";
	size_t to_find_len = strlen(to_find);
	bool found = false;
	while(true)
	{
		if(*to_find == '\0') break;
		if(strncmp(sentence_str, to_find, to_find_len) == 0)
		{
			sentence_str += to_find_len;
			found = true;
			break;
		}
		sentence_str += 1;
	}

	if(!found)
	{
		printf_buff(&to_return.error_message, "Couldn't find action beginning with 'ACT_'.\n");
		return to_return;
	}

	SmallTextChunk action_string = { 0 };
	sentence_str += get_until(&action_string, sentence_str, "( ");

	bool found_action = false;
	AvailableActions available = { 0 };
	fill_available_actions(it, &available);
	BUFF_ITER(Action, &available)
	{
		if (strcmp(actions[*it].name, action_string.data) == 0)
		{
			found_action = true;
			out->npc_action_type = *it;
		}
	}

	if (!found_action)
	{
		printf_buff(&to_return.error_message, "Could not find action associated with parsed 'ACT_' string `%s`\n", action_string.data);
		out->npc_action_type = ACT_none;
		return to_return;
	}
	else
	{
		SmallTextChunk dialog_str = { 0 };
		if (actions[out->npc_action_type].takes_argument)
		{
#define EXPECT(chr, val) if (chr != val) { printf_buff(&to_return.error_message, "Improperly formatted sentence, expected character '%c' but got '%c'\n", val, chr); return to_return; }

			EXPECT(*sentence_str, '(');
			sentence_str += 1;

			SmallTextChunk argument = { 0 };
			sentence_str += get_until(&argument, sentence_str, ")");

			if (out->npc_action_type == ACT_give_item)
			{
				Entity *e = it;
				bool found = false;
				BUFF_ITER(ItemKind, &e->held_items)
				{
					const char *without_item_prefix = &argument.data[0];
					EXPECT(*without_item_prefix, 'I');
					without_item_prefix += 1;
					EXPECT(*without_item_prefix, 'T');
					without_item_prefix += 1;
					EXPECT(*without_item_prefix, 'E');
					without_item_prefix += 1;
					EXPECT(*without_item_prefix, 'M');
					without_item_prefix += 1;
					EXPECT(*without_item_prefix, '_');
					without_item_prefix += 1;
					if (strcmp(items[*it].enum_name, without_item_prefix) == 0)
					{
						out->given_item = *it;
						if (found)
						{
							Log("Duplicate item enum name? Really weird...\n");
						}
						found = true;
					}
				}
				if (!found)
				{
					printf_buff(&to_return.error_message, "Couldn't find item in the inventory of the NPC to give. You said `%s`, but you have [%s] in your inventory \n", argument.data, item_string(it).data);
					return to_return;
				}
			}
			else
			{
				printf_buff(&to_return.error_message, "Don't know how to handle argument in action of type `%s`\n", actions[out->npc_action_type].name);
#ifdef DEVTOOLS
				// not sure if this should never happen or not, need more sleep...
				assert(false);
#endif
				return to_return;
			}

			EXPECT(*sentence_str, ')');
			sentence_str += 1;
		}
		EXPECT(*sentence_str, ' ');
		sentence_str += 1;
		EXPECT(*sentence_str, '"');
		sentence_str += 1;

		sentence_str += get_until(&dialog_str, sentence_str, "\"\n");
		if (dialog_str.cur_index >= ARRLEN(out->npc_dialog.data))
		{
			printf_buff(&to_return.error_message, "Dialog string `%s` too big to fit in sentence size %d\n", dialog_str.data,
			    (int) ARRLEN(out->npc_dialog.data));
			return to_return;
		}

		char next_char = *(sentence_str + 1);
		if(!(next_char == '\0' || next_char == '\n'))
		{
			printf_buff(&to_return.error_message, "Expected dialog to end after the last quote, but instead found character '%c'\n", next_char);
			return to_return;
		}

		memcpy(out->npc_dialog.data, dialog_str.data, dialog_str.cur_index);
		out->npc_dialog.cur_index = dialog_str.cur_index;

		to_return.succeeded = true;
		return to_return;
	}

	to_return.succeeded = false;
	return to_return;
}
