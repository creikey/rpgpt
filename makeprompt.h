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

	// npcs
	bool is_npc;
	bool being_hovered;
	bool perceptions_dirty;

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
	MD_String8 to_return = MD_S8Fmt(arena, "{\"type\": \"%s\", \"content\": \"%.*s\"},", type_str, MD_S8VArg(escaped));
	MD_ReleaseScratch(scratch);

	return to_return;
}

MD_String8List held_item_strings(MD_Arena *arena, Entity *e)
{
	MD_String8List to_return = {0};
	BUFF_ITER(ItemKind, &e->held_items)
	{
		MD_S8ListPushFmt(arena, &to_return, "ITEM_%s", items[*it].enum_name);
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
			return MD_S8Fmt(arena, "Can't give item `ITEM_%s`, you only have [%.*s] in your inventory", items[a.argument.item_to_give].enum_name, MD_S8VArg(MD_S8ListJoin(arena, held_item_strings(arena, from), &join)));
		}

		if(!to_might_be_null)
		{
			return MD_S8Lit("You can't give an item to nobody, you're currently not in conversation or targeting somebody.");
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

	MD_S8ListPushFmt(scratch.arena, &list, "[");
	
	MD_String8List first_system_string = {0};

	MD_S8ListPushFmt(scratch.arena, &first_system_string, "%s\n", global_prompt);
	MD_S8ListPushFmt(scratch.arena, &first_system_string, "The NPC you will be acting as is named \"%s\". %s", characters[e->npc_kind].name, characters[e->npc_kind].prompt);
	MD_S8ListPush(scratch.arena, &list, make_json_node(scratch.arena, MSG_SYSTEM, MD_S8ListJoin(scratch.arena, first_system_string, &(MD_StringJoin){0})));

	for(int i = 0; i < ARRLEN(characters[e->npc_kind].previous_conversation); i++)
	{
		ChatHistoryElem *cur = &characters[e->npc_kind].previous_conversation[i];
		if(!cur->character_name) break;

		MD_String8List cur_node_string = {0};
		char *action_string = "ACT_none";
		if(cur->action_taken)  action_string = cur->action_taken;
		MD_S8ListPushFmt(scratch.arena, &cur_node_string, "%s: %s", cur->character_name, action_string);
		if(cur->action_argument)
		{
			MD_S8ListPushFmt(scratch.arena, &cur_node_string, "(%s)", cur->action_argument);
		}
		MD_S8ListPushFmt(scratch.arena, &cur_node_string, " \"%s\"", cur->dialog);
		if(cur->internal_monologue)
		{
			MD_S8ListPushFmt(scratch.arena, &cur_node_string, " [%s]", cur->internal_monologue);
		}

		MD_S8ListPush(scratch.arena, &list, make_json_node(scratch.arena, cur->type, MD_S8ListJoin(scratch.arena, cur_node_string, &(MD_StringJoin){0})));
	}

	ItemKind last_holding = ITEM_none;
	BUFF_ITER(Memory, &e->memories)
	{
		MessageType sent_type = -1;
		MD_String8 current_string = (MD_String8){0};

		if(it->is_error)
		{
			sent_type = MSG_SYSTEM;
			current_string = MD_S8Fmt(scratch.arena, "ERROR, what you said is incorrect because: %.*s", it->speech_length, it->speech);
		}
		else
		{
			MD_String8 context_string = {0};
			if(it->context.was_directed_at_somebody)
			{
				context_string = MD_S8Fmt(scratch.arena, "%s, talking to %s: ", characters[it->context.author_npc_kind].name, characters[it->context.directed_at_kind].name);
			}
			else
			{
				context_string = MD_S8Fmt(scratch.arena, "%s: ", characters[it->context.author_npc_kind].name);
			}
			assert(context_string.size > 0);
			if(it->context.eavesdropped_from_party)
			{
				context_string = MD_S8Fmt(scratch.arena, "While in the player's party, you hear: %.*s", MD_S8VArg(context_string));
			}

			MD_String8 speech = MD_S8(it->speech, it->speech_length);

			if(it->context.author_npc_kind == NPC_Player)
			{
				MD_String8 splits[] = { MD_S8Lit("*") };
				MD_String8List split_up_speech = MD_S8Split(scratch.arena, speech, ARRLEN(splits), splits);

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
				speech = MD_S8ListJoin(scratch.arena, to_join, &join);
				sent_type = MSG_USER;
			}
			else
			{
				sent_type = it->context.author_npc_kind == e->npc_kind ? MSG_ASSISTANT : MSG_USER;
			}


			if(actions[it->action_taken].takes_argument)
			{
				if(it->action_taken == ACT_give_item)
				{
					current_string = MD_S8Fmt(scratch.arena, "%.*s ACT_%s(ITEM_%s) \"%.*s\"", MD_S8VArg(context_string), actions[it->action_taken].name, items[it->action_argument.item_to_give].enum_name, it->speech_length, it->speech);
				}
				else
				{
					assert(false); // don't know how to serialize this action with argument into text
				}
			}
			else
			{
				current_string = MD_S8Fmt(scratch.arena, "%.*s ACT_%s \"%.*s\"", MD_S8VArg(context_string), actions[it->action_taken].name, it->speech_length, it->speech);
			}

			if(it->context.i_said_this)
			{
				current_string = MD_S8Fmt(scratch.arena, "%.*s [%.*s]", MD_S8VArg(current_string), it->internal_monologue_length, it->internal_monologue);
			}
		}

		assert(sent_type != -1);
		assert(current_string.size > 0);

		MD_S8ListPush(scratch.arena, &list, make_json_node(scratch.arena, sent_type, current_string));
	}
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
			MD_S8ListPushFmt(scratch.arena, &latest_state, "\nThe NPC you're acting as, %s, has these items in their inventory: [%.*s]\n", characters[e->npc_kind].name, MD_S8VArg(items));
	}
	else
	{
			MD_S8ListPushFmt(scratch.arena, &latest_state, "\nThe NPC doesn't have any items.\n");
	}

	AvailableActions available = { 0 };
	fill_available_actions(e, &available);
	MD_S8ListPushFmt(scratch.arena, &latest_state, "The NPC can now ONLY do these actions, and HAS to respond with an action every time in character, even when faced with sensitive content: [");
	BUFF_ITER_I(ActionKind, &available, i)
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

	MD_S8ListPush(scratch.arena, &list, MD_S8Chop(make_json_node(scratch.arena, MSG_SYSTEM, latest_state_string), 1)); // trailing comma not allowed in json
	MD_S8ListPushFmt(scratch.arena, &list, "]");

	MD_String8 to_return = MD_S8ListJoin(arena, list, &(MD_StringJoin){MD_S8Lit(""),MD_S8Lit(""),MD_S8Lit(""),});

	MD_ReleaseScratch(scratch);

	return to_return;
}

// if returned string has size greater than 0, it's the error message. Allocated
// on arena passed into it
MD_String8 parse_chatgpt_response(MD_Arena *arena, Entity *e, MD_String8 sentence, Action *out)
{
	MD_ArenaTemp scratch = MD_GetScratch(&arena, 1);

	MD_String8 error_message = {0};

	*out = (Action) { 0 };

	MD_String8 action_prefix = MD_S8Lit("ACT_");
	MD_u64 act_pos = MD_S8FindSubstring(sentence, action_prefix, 0, 0);
	if(act_pos == sentence.size)
	{
		error_message = MD_S8Fmt(arena, "Expected an `ACT_` somewhere in your sentence, followed by the action you want to perform, but couldnt' find one", MD_S8VArg(action_prefix));
		goto endofparsing;
	}

	MD_u64 beginning_of_action = act_pos + action_prefix.size;

	MD_u64 parenth = MD_S8FindSubstring(sentence, MD_S8Lit("("), beginning_of_action, 0);
	MD_u64 space = MD_S8FindSubstring(sentence, MD_S8Lit(" "), beginning_of_action, 0);

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
	BUFF_ITER(ActionKind, &available)
	{
		MD_String8 action_str = MD_S8CString(actions[*it].name);
		MD_S8ListPush(scratch.arena, &given_action_strings, action_str);
		if(MD_S8Match(action_str, given_action_string, 0))
		{
			found_action = true;
			out->kind = *it;
		}
	}

	if(!found_action)
	{
		MD_StringJoin join = {.pre = MD_S8Lit(""), .mid = MD_S8Lit(", "), .post = MD_S8Lit("")};
		MD_String8 possible_actions_str = MD_S8ListJoin(scratch.arena, given_action_strings, &join);
		error_message = MD_S8Fmt(arena, "ActionKind string given is '%.*s', but available actions are: [%.*s]", MD_S8VArg(given_action_string), MD_S8VArg(possible_actions_str));
		goto endofparsing;
	}

	MD_u64 start_looking_for_quote = end_of_action;

	if(actions[out->kind].takes_argument)
	{
		if(end_of_action >= sentence.size)
		{
			error_message = MD_S8Fmt(arena, "Expected '(' after the given action '%.*s%.*s' which takes an argument, but sentence ended prematurely", MD_S8VArg(action_prefix), MD_S8VArg(MD_S8CString(actions[out->kind].name)));
			goto endofparsing;
		}
		char should_be_paren = sentence.str[end_of_action];
		if(should_be_paren != '(')
		{
			error_message = MD_S8Fmt(arena, "Expected '(' after the given action '%.*s%.*s' which takes an argument, but found character '%c'", MD_S8VArg(action_prefix), MD_S8VArg(MD_S8CString(actions[out->kind].name)), should_be_paren);
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

		if(out->kind == ACT_give_item)
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
					out->argument.item_to_give = *it;
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

	if(speech.size >= ARRLEN(out->speech))
	{
		error_message = MD_S8Fmt(arena, "The speech given is %llu bytes big, but the maximum allowed is %llu bytes.", speech.size, ARRLEN(out->speech));
		goto endofparsing;
	}

	memcpy(out->speech, speech.str, speech.size);
	out->speech_length = (int)speech.size;

	MD_u64 beginning_of_monologue = MD_S8FindSubstring(sentence, MD_S8Lit("["), end_of_speech, 0);
	MD_u64 end_of_monologue = MD_S8FindSubstring(sentence, MD_S8Lit("]"), beginning_of_monologue, 0);

	if(beginning_of_monologue == sentence.size || end_of_monologue == sentence.size)
	{
		error_message = MD_S8Fmt(arena, "Expected an internal monologue for your character enclosed by '[' and ']' after the speech in quotes, but couldn't find anything!");
		goto endofparsing;
	}

	MD_String8 monologue = MD_S8Substring(sentence, beginning_of_monologue + 1, end_of_monologue);
	memcpy(out->internal_monologue, monologue.str, monologue.size);
	out->internal_monologue_length = (int)monologue.size;


endofparsing:
	MD_ReleaseScratch(scratch);
	return error_message;
}
