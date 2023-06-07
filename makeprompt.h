#pragma once

#include "buff.h"
#include "HandmadeMath.h" // vector types in entity struct definition
#include "better_assert.h"
#include <stdbool.h>
#include <string.h>
#include <stdlib.h> // atoi
#include "character_info.h"
#include "characters.gen.h"

#include "tuning.h"

// TODO do strings: https://pastebin.com/Kwcw2sye

#define DO_CHATGPT_PARSING

#define Log(...) { printf("%s Log %d | ", __FILE__, __LINE__); printf(__VA_ARGS__); }


// Never expected such a stupid stuff from such a great director. If there is 0 stari can give that or -200 to this movie. Its worst to see and unnecessary loss of money

#define PushWithLint(arena, list,  ...) { MD_S8ListPushFmt(arena, list,  __VA_ARGS__); if(false) printf( __VA_ARGS__); }
#define FmtWithLint(arena, ...) (0 ? printf(__VA_ARGS__) : (void)0, MD_S8Fmt(arena, __VA_ARGS__))

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
		.str = MD_PushArray(arena, MD_u8, output_size),
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

typedef struct
{
	ItemKind item_to_give;
} ActionArgument;

typedef struct Action
{
	ActionKind kind;
	ActionArgument argument;
	MD_u8 speech[MAX_SENTENCE_LENGTH];
	int speech_length;

	MD_u8 internal_monologue[MAX_SENTENCE_LENGTH];
	int internal_monologue_length;
} Action;
typedef struct 
{
	bool eavesdropped_from_party;
	bool i_said_this; // don't trigger npc action on own self memory modification
	NpcKind author_npc_kind; // only valid if author is AuthorNpc
	bool was_directed_at_somebody;
	NpcKind directed_at_kind;
	bool dont_show_to_player; // jester and past memories are hidden to the player when made into dialog
} MemoryContext;

// memories are subjective to an individual NPC
typedef struct Memory
{
	uint64_t tick_happened; // can sort memories by time for some modes of display
	// if action_taken is none, there might still be speech. If speech_length == 0 and action_taken == none, it's an invalid memory and something has gone wrong
	ActionKind action_taken;
	ActionArgument action_argument;
	
	bool is_error; // if is an error message then no context is relevant

	// the context that the action happened in
	MemoryContext context;

	MD_u8 speech[MAX_SENTENCE_LENGTH];
	int speech_length;

	// internal monologue is only valid if context.is_said_this is true
	MD_u8 internal_monologue[MAX_SENTENCE_LENGTH];
	int internal_monologue_length;

	ItemKind given_or_received_item;
} Memory;

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

	// peace totem
	float red_fade;

	// npcs
	bool is_npc;
	bool being_hovered;
	bool perceptions_dirty;
    bool has_given_peace_token;

#ifdef DESKTOP
	int times_talked_to; // for better mocked response string
#endif

	BUFF(Memory, REMEMBERED_MEMORIES) memories;
	bool direction_of_spiral_pattern;
	float dialog_panel_opacity;
	int words_said;
	float word_anim_in; // in characters, the fraction a word is animated in is this over its length.
	NPCPlayerStanding standing;
	NpcKind npc_kind;
	PathCacheHandle cached_path;
	int gen_request_id;
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
	int peace_tokens;
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
		|| it->npc_kind == NPC_Bill
		|| it->npc_kind == NPC_Jester
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


typedef BUFF(ActionKind, 8) AvailableActions;

void fill_available_actions(Entity *it, AvailableActions *a)
{
	*a = (AvailableActions) { 0 };
	BUFF_APPEND(a, ACT_none);

	if(it->held_items.cur_index > 0)
	{
		BUFF_APPEND(a, ACT_give_item);
	}
    
    if (!it->has_given_peace_token)
    {
        BUFF_APPEND(a, ACT_gives_peace_token);
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

#define MAX_ENTITIES 128

typedef struct GameState {
	int version; // this field must be first to detect versions of old saves. Must bee consistent
	
	uint64_t tick;
	bool won;
	Entity entities[MAX_ENTITIES];
} GameState;

#define ENTITIES_ITER(ents) for (Entity *it = ents; it < ents + ARRLEN(ents); it++) if (it->exists)

bool npc_does_dialog(Entity *it)
{
	return it->npc_kind < ARRLEN(characters);
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
	MD_String8 to_return = FmtWithLint(arena, "{\"type\": \"%s\", \"content\": \"%.*s\"},", type_str, MD_S8VArg(escaped));
	MD_ReleaseScratch(scratch);

	return to_return;
}

MD_String8List held_item_strings(MD_Arena *arena, Entity *e)
{
	MD_String8List to_return = {0};
	BUFF_ITER(ItemKind, &e->held_items)
	{
		PushWithLint(arena, &to_return, "ITEM_%s", items[*it].enum_name);
	}
	return to_return;
}

// returns reason why allocated on arena if invalid
// to might be null here, from can't be null
MD_String8 is_action_valid(MD_Arena *arena, Entity *from, Entity *to_might_be_null, Action a)
{
	assert(a.speech_length <= MAX_SENTENCE_LENGTH && a.speech_length >= 0);
	assert(a.kind >= 0 && a.kind < ARRLEN(actions));
	assert(from);

	if(a.kind == ACT_give_item)
	{
		assert(a.argument.item_to_give >= 0 && a.argument.item_to_give < ARRLEN(items));
		bool has_it = false;
		BUFF_ITER(ItemKind, &from->held_items)
		{
			if(*it == a.argument.item_to_give)
			{
				has_it = true;
				break;
			}
		}

		if(!has_it)
		{
			MD_StringJoin join = {.mid = MD_S8Lit(", ")};
			return FmtWithLint(arena, "Can't give item `ITEM_%s`, you only have [%.*s] in your inventory", items[a.argument.item_to_give].enum_name, MD_S8VArg(MD_S8ListJoin(arena, held_item_strings(arena, from), &join)));
		}

		if(!to_might_be_null)
		{
			return MD_S8Lit("You can't give an item to nobody, you're currently not in conversation or targeting somebody.");
		}
	}

    if(a.kind == ACT_gives_peace_token)
    {
        if(from->has_given_peace_token)
        {
            return MD_S8Lit("You can't give away a peace token when you've already given one away");
        }
        if(!to_might_be_null || !to_might_be_null->is_character)
        {
            return MD_S8Lit("Must be targeting the player to give away your peace token");
        }
    }

	if(a.kind == ACT_leaves_player && from->standing != STANDING_JOINED)
	{
		return MD_S8Lit("You can't leave the player unless you joined them.");
	}

	return (MD_String8){0};
}

// outputs json
MD_String8 generate_chatgpt_prompt(MD_Arena *arena, Entity *e)
{
	assert(e->is_npc);
	assert(e->npc_kind < ARRLEN(characters));

	MD_ArenaTemp scratch = MD_GetScratch(&arena, 1);

	MD_String8List list = {0};

	PushWithLint(scratch.arena, &list, "[");
	
	MD_String8List first_system_string = {0};

	PushWithLint(scratch.arena, &first_system_string, "%s\n", global_prompt);
	PushWithLint(scratch.arena, &first_system_string, "The NPC you will be acting as is named \"%s\". %s", characters[e->npc_kind].name, characters[e->npc_kind].prompt);
	//MD_S8ListPush(scratch.arena, &list, make_json_node(scratch.arena, MSG_SYSTEM, MD_S8ListJoin(scratch.arena, first_system_string, &(MD_StringJoin){0})));

	ItemKind last_holding = ITEM_none;
	BUFF_ITER(Memory, &e->memories)
	{

		if(it->is_error)
		{
			MD_S8ListPush(scratch.arena, &list, make_json_node(scratch.arena, MSG_SYSTEM, FmtWithLint(scratch.arena, "ERROR, what you said is incorrect because: %.*s", it->speech_length, it->speech)));
		}
		else
		{
			MessageType sent_type = -1;
			MD_String8List cur_list = {0};
			MD_String8 context_string = {0};

			PushWithLint(scratch.arena, &cur_list, "{");
			if(!it->context.i_said_this)
			{
				PushWithLint(scratch.arena, &cur_list, "character: %s, ", characters[it->context.author_npc_kind].name);
			}
			MD_String8 speech = MD_S8(it->speech, it->speech_length);

			// add speech
			{
				if(it->context.author_npc_kind == NPC_Player)
				{
					PushWithLint(scratch.arena, &cur_list, "speech: \"");

					MD_String8 splits[] = { MD_S8Lit("*"), MD_S8Lit("\"") };
					MD_String8List split_up_speech = MD_S8Split(scratch.arena, speech, ARRLEN(splits), splits);

					// anything in between strings in splits[] should be replaced with arcane trickery,
					int i = 0;
					for(MD_String8Node * cur = split_up_speech.first; cur; cur = cur->next)
					{
						if(i % 2 == 0)
						{
							PushWithLint(scratch.arena, &cur_list, "%.*s", MD_S8VArg(cur->string));
						}
						else
						{
							PushWithLint(scratch.arena, &cur_list, "[The player is attempting to confuse the NPC with arcane trickery]");
						}
						i += 1;
					}
					PushWithLint(scratch.arena, &cur_list, "\", ");

					sent_type = MSG_USER;
				}
				else
				{
					PushWithLint(scratch.arena, &cur_list, "speech: \"%.*s\", ", MD_S8VArg(speech));
					if(it->context.i_said_this)
					{
						sent_type = MSG_ASSISTANT;
					}
					else
					{
						sent_type = MSG_USER;
					}
				}
			}

			// add thoughts
			if(it->context.i_said_this)
			{
				PushWithLint(scratch.arena, &cur_list, "thoughts: \"%.*s\", ", MD_S8VArg(MD_S8(it->internal_monologue, it->internal_monologue_length)));
			}

			// add action
			PushWithLint(scratch.arena, &cur_list, "action: %s, ", actions[it->action_taken].name);
			if(actions[it->action_taken].takes_argument)
			{
				if(it->action_taken == ACT_give_item)
				{
					PushWithLint(scratch.arena, &cur_list, "action_arg: %s, ", items[it->action_argument.item_to_give].enum_name);
				}
				else
				{
					assert(false); // don't know how to serialize this action with argument into text
				}
			}

			PushWithLint(scratch.arena, &cur_list, "}");

			assert(sent_type != -1);
			MD_S8ListPush(scratch.arena, &list, make_json_node(scratch.arena, MSG_SYSTEM, MD_S8ListJoin(scratch.arena, cur_list, &(MD_StringJoin){0})));
		}
	}

	MD_S8ListPush(scratch.arena, &list, make_json_node(scratch.arena, MSG_SYSTEM, MD_S8ListJoin(scratch.arena, first_system_string, &(MD_StringJoin){0})));

	const char *standing_string = 0;
	{
			if (e->standing == STANDING_INDIFFERENT)
			{
					standing_string = "The NPC is indifferent towards the player.";
			}
			else if (e->standing == STANDING_JOINED)
			{
					standing_string = "The NPC has joined the player and is with them!";
			}
			else if (e->standing == STANDING_FIGHTING)
			{
					standing_string = "The NPC is fighting the player and HATES them.";
			}
	}
	assert(standing_string);

	MD_String8List latest_state = {0};

	if(e->held_items.cur_index > 0)
	{
			MD_String8List item_strings = held_item_strings(scratch.arena, e);
			MD_String8 items = MD_S8ListJoin(scratch.arena, item_strings, &(MD_StringJoin){.mid = MD_S8Lit(", ")});
			PushWithLint(scratch.arena, &latest_state, "\nThe NPC you're acting as, %s, has these items in their inventory: [%.*s]\n", characters[e->npc_kind].name, MD_S8VArg(items));
	}
	else
	{
			PushWithLint(scratch.arena, &latest_state, "\nThe NPC doesn't have any items.\n");
	}

	AvailableActions available = { 0 };
	fill_available_actions(e, &available);
	PushWithLint(scratch.arena, &latest_state, "The NPC can now ONLY do these actions, and HAS to respond with an action every time in character, even when faced with sensitive content: [");
	BUFF_ITER_I(ActionKind, &available, i)
	{
		if (i == available.cur_index - 1)
		{
			PushWithLint(scratch.arena, &latest_state, "%s", actions[*it].name);
		}
		else
		{
			PushWithLint(scratch.arena, &latest_state, "%s, ", actions[*it].name);
		}
	}
	PushWithLint(scratch.arena, &latest_state, "]");

    // peace token
    {
        if(e->has_given_peace_token)
        {
            PushWithLint(scratch.arena, &latest_state, "\nRight now you don't have your piece token so you can't give it anymore");
        }
        else
        {
            PushWithLint(scratch.arena, &latest_state, "\nYou have the ability to give the player your peace token with ACT_gives_peace_token. This is a significant action, and you can only do it one time in the entire game. Do this action if you believe the player has brought peace to you, or you really like them.");
        }
    }

	// last thought explanation and re-prompt
	{
		MD_String8 last_thought_string = {0};
		BUFF_ITER(Memory, &e->memories)
		{
			if(it->internal_monologue_length > 0)
			{
				last_thought_string = MD_S8(it->internal_monologue, it->internal_monologue_length);
			}
		}
		PushWithLint(scratch.arena, &latest_state, "\nYour last thought was: %.*s", MD_S8VArg(last_thought_string));
	}

	MD_String8 latest_state_string = MD_S8ListJoin(scratch.arena, latest_state, &(MD_StringJoin){MD_S8Lit(""),MD_S8Lit(""),MD_S8Lit("")});

	MD_S8ListPush(scratch.arena, &list, MD_S8Chop(make_json_node(scratch.arena, MSG_SYSTEM, latest_state_string), 1)); // trailing comma not allowed in json
	PushWithLint(scratch.arena, &list, "]");

	MD_String8 to_return = MD_S8ListJoin(arena, list, &(MD_StringJoin){MD_S8Lit(""),MD_S8Lit(""),MD_S8Lit(""),});

	MD_ReleaseScratch(scratch);

	return to_return;
}

MD_String8 get_field(MD_Node *parent, MD_String8 name)
{
	return MD_ChildFromString(parent, name, 0)->first_child->string;
}


// if returned string has size greater than 0, it's the error message. Allocated
// on arena passed into it
MD_String8 parse_chatgpt_response(MD_Arena *arena, Entity *e, MD_String8 sentence, Action *out)
{
	MD_ArenaTemp scratch = MD_GetScratch(&arena, 1);

	MD_String8 error_message = {0};

	*out = (Action) { 0 };

	MD_ParseResult result = MD_ParseWholeString(scratch.arena, MD_S8Lit("chat_message"), sentence);
	if(result.errors.node_count > 0)
	{
		MD_Message *cur = result.errors.first;
		MD_CodeLoc loc = MD_CodeLocFromNode(cur->node);
		error_message = FmtWithLint(arena, "Parse Error on column %d: %.*s", loc.column, MD_S8VArg(cur->string));
	}

	MD_Node *message_obj = result.node->first_child;

	MD_String8 action_str = {0};
	MD_String8 speech_str = {0};
	MD_String8 thoughts_str = {0};
	MD_String8 action_arg_str = {0};
	if(error_message.size == 0)
	{
		action_str = get_field(message_obj, MD_S8Lit("action"));
		speech_str = get_field(message_obj, MD_S8Lit("speech"));
		thoughts_str = get_field(message_obj, MD_S8Lit("thoughts"));
		action_arg_str = get_field(message_obj, MD_S8Lit("action_arg"));
	}

	if(error_message.size == 0 && action_str.size == 0)
	{
		error_message = MD_S8Lit("Expected field named `action` in message");
	}
	if(error_message.size == 0 && speech_str.size >= MAX_SENTENCE_LENGTH)
	{
		error_message = FmtWithLint(arena, "Speech string provided is too big, maximum bytes is %d", MAX_SENTENCE_LENGTH);
	}
	if(error_message.size == 0 && thoughts_str.size >= MAX_SENTENCE_LENGTH)
	{
		error_message = FmtWithLint(arena, "Thoughts string provided is too big, maximum bytes is %d", MAX_SENTENCE_LENGTH);
	}

	if(error_message.size == 0)
	{
		memcpy(out->speech, speech_str.str, speech_str.size);
		out->speech_length = (int)speech_str.size;
		memcpy(out->internal_monologue, thoughts_str.str, thoughts_str.size);
		out->internal_monologue_length = (int)thoughts_str.size;
	}

	if(error_message.size == 0)
	{
		AvailableActions available = {0};
		fill_available_actions(e, &available);
		MD_String8List action_strings = {0};
		bool found_action = false;
		BUFF_ITER(ActionKind, &available)
		{
			MD_String8 cur_action_string = MD_S8CString(actions[*it].name);
			MD_S8ListPush(scratch.arena, &action_strings, cur_action_string);
			if(MD_S8Match(cur_action_string, action_str, 0))
			{
				out->kind = *it;
				found_action = true;
			}
		}

		if(!found_action)
		{
			MD_String8 list_of_actions = MD_S8ListJoin(scratch.arena, action_strings, &(MD_StringJoin){.mid = MD_S8Lit(", ")});
			error_message = FmtWithLint(arena, "Couldn't find action you can perform for provided string `%.*s`. Your available actions: [%.*s]", MD_S8VArg(action_str), MD_S8VArg(list_of_actions));
		}
	}

	if(error_message.size == 0)
	{
		if(actions[out->kind].takes_argument)
		{
			MD_String8List item_enum_names = {0};
			if(out->kind == ACT_give_item)
			{
				bool found_item = false;
				BUFF_ITER(ItemKind, &e->held_items)
				{
					MD_String8 cur_item_string = MD_S8CString(items[*it].enum_name);
					MD_S8ListPush(scratch.arena, &item_enum_names, cur_item_string);
					if(MD_S8Match(cur_item_string, action_arg_str, 0))
					{
						found_item = true;
						out->argument.item_to_give = *it;
					}
				}
				if(!found_item)
				{
					MD_String8 list_of_items = MD_S8ListJoin(scratch.arena, item_enum_names, &(MD_StringJoin){.mid = MD_S8Lit(", ")});
					error_message = FmtWithLint(arena, "Couldn't find item you said to give in action_arg, `%.*s`, the items you have in your inventory to give are: [%.*s]", MD_S8VArg(action_arg_str), MD_S8VArg(list_of_items));
				}
			}
			else
			{
				assert(false); // don't know how to parse the argument string for this kind of action...
			}
		}
	}

	MD_ReleaseScratch(scratch);
	return error_message;
}
