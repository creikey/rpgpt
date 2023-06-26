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
#define SHOULD_ESCAPE(c) (c == '"' || c == '\n' || c == '\\')
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
	// serialized as bytes. No pointers.
	ItemKind item_to_give;
} ActionArgument;

typedef struct Action
{
	ActionKind kind;
	ActionArgument argument;
	MD_u8 speech[MAX_SENTENCE_LENGTH];
	int speech_length;

	bool talking_to_somebody;
	NpcKind talking_to_kind;

	MD_u8 internal_monologue[MAX_SENTENCE_LENGTH];
	int internal_monologue_length;

	MoodKind mood;
} Action;

typedef struct 
{
	bool i_said_this; // don't trigger npc action on own self memory modification
	NpcKind author_npc_kind; // only valid if author is AuthorNpc
	bool was_talking_to_somebody;
	NpcKind talking_to_kind;
	bool heard_physically; // if not physically, the source was directly
	bool dont_show_to_player; // jester and past memories are hidden to the player when made into dialog
} MemoryContext;

// memories are subjective to an individual NPC
typedef struct Memory
{
	struct Memory *prev;
	struct Memory *next;

	uint64_t tick_happened; // can sort memories by time for some modes of display
	// if action_taken is none, there might still be speech. If speech_length == 0 and action_taken == none, it's an invalid memory and something has gone wrong
	ActionKind action_taken;
	ActionArgument action_argument;

	// the context that the action happened in
	MemoryContext context;

	MD_u8 speech[MAX_SENTENCE_LENGTH];
	int speech_length;

	// internal monologue is only valid if context.is_said_this is true
	MD_u8 internal_monologue[MAX_SENTENCE_LENGTH];
	int internal_monologue_length;

	MoodKind mood;

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

typedef struct TextChunk
{
	struct TextChunk *next;
	struct TextChunk *prev;
	char text[MAX_SENTENCE_LENGTH];
	int text_length;
} TextChunk;

typedef enum
{
	MACH_invalid,
	MACH_idol_dispenser,
	MACH_arrow_shooter,
} MachineKind;

MD_String8 points_at_chunk(TextChunk *t)
{
	return MD_S8((MD_u8*)t->text, t->text_length);
}

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

	// machines, like the machine that gives the player the idol, or the ones that
	// shoot arrows
	bool is_machine;
	MachineKind machine_kind;
	bool has_given_idol;
	float idol_reminder_opacity; // fades out
	float arrow_timer;

	// items
	bool is_item;
	bool held_by_player;
	ItemKind item_kind;

	// npcs
	bool is_npc;
	bool being_hovered;
	bool perceptions_dirty;
	TextChunk *errorlist_first;
	TextChunk *errorlist_last;
#ifdef DESKTOP
	int times_talked_to; // for better mocked response string
#endif
	bool opened;
	float opened_amount;
	bool gave_away_sword;
	Memory *memories_first;
	Memory *memories_last;
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

typedef BUFF(NpcKind, 32) CanTalkTo;

bool npc_is_knight_sprite(Entity *it)
{
	return it->is_npc && (false
		|| it->npc_kind == NPC_Edeline
		|| it->npc_kind == NPC_TheKing
		|| it->npc_kind == NPC_TheBlacksmith
		|| it->npc_kind == NPC_Red 
		|| it->npc_kind == NPC_Blue
		|| it->npc_kind == NPC_Davis
		|| it->npc_kind == NPC_Bill
		|| it->npc_kind == NPC_Jester
		);
}

bool item_is_scroll(ItemKind i)
{
	if(i == ITEM_Scroll1 || i == ITEM_Scroll2 || i == ITEM_Scroll3)
	{
		return true;
	}
	else
	{
		return false;
	}
}

MD_String8 scroll_secret(ItemKind i)
{
	if(i == ITEM_Scroll1)
	{
		return MD_S8Lit(Scroll1_Secret);
	}
	else if(i == ITEM_Scroll2)
	{
		return MD_S8Lit(Scroll2_Secret);
	}
	else if(i == ITEM_Scroll3)
	{
		return MD_S8Lit(Scroll3_Secret);
	}
	else
	{
		assert(false);
		return MD_S8Lit("");
	}
}

float entity_max_damage(Entity *e)
{
	return 1.0f;
}

bool npc_attacks_with_sword(Entity *it)
{
	return false;
}

bool npc_attacks_with_shotgun(Entity *it)
{
	return it->is_npc && (false);
}


typedef BUFF(ActionKind, 8) AvailableActions;

void fill_available_actions(Entity *it, AvailableActions *a)
{
	*a = (AvailableActions) { 0 };
	BUFF_APPEND(a, ACT_none);

	if(it->npc_kind == NPC_Pile)
	{
		if(!it->gave_away_sword) BUFF_APPEND(a, ACT_releases_sword_of_nazareth);
	}
	else if(it->npc_kind == NPC_Door)
	{
		if(!it->opened) BUFF_APPEND(a, ACT_opens_myself);
	}
	else
	{
		if(it->held_items.cur_index > 0)
		{
			BUFF_APPEND(a, ACT_gift_item_to_targeting);
		}

		if (it->npc_kind == NPC_TheKing)
		{
			BUFF_APPEND(a, ACT_knights_player);
		}

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
	}
}


typedef struct GameState {
	uint64_t tick;
	bool won;
	Entity entities[MAX_ENTITIES];
} GameState;

#define ENTITIES_ITER(ents) for (Entity *it = ents; it < ents + ARRLEN(ents); it++) if (it->exists && !it->destroy && it->generation > 0)

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


// outputs json
MD_String8 generate_chatgpt_prompt(MD_Arena *arena, Entity *e, CanTalkTo can_talk_to)
{
	assert(e->is_npc);
	assert(e->npc_kind < ARRLEN(characters));

	MD_ArenaTemp scratch = MD_GetScratch(&arena, 1);

	MD_String8List list = {0};

	PushWithLint(scratch.arena, &list, "[");
	
	MD_String8List first_system_string = {0};

	PushWithLint(scratch.arena, &first_system_string, "%s\n", global_prompt);
	PushWithLint(scratch.arena, &first_system_string, "The NPC you will be acting as is named \"%s\". %s\n", characters[e->npc_kind].name, characters[e->npc_kind].prompt);

	// writing style
	{
		if(characters[e->npc_kind].writing_style[0])
			PushWithLint(scratch.arena, &first_system_string, "Examples of %s's writing style:\n", characters[e->npc_kind].name);
		for(int i = 0; i < ARRLEN(characters[e->npc_kind].writing_style); i++)
		{
			char *writing = characters[e->npc_kind].writing_style[i];
			if(writing)
				PushWithLint(scratch.arena, &first_system_string, "'%s'\n", writing);
		}
		PushWithLint(scratch.arena, &first_system_string, "\n");
	}


	if(e->errorlist_first)
		PushWithLint(scratch.arena, &first_system_string, "Errors to watch out for: ");
	for(TextChunk *cur = e->errorlist_first; cur; cur = cur->next)
	{
		PushWithLint(scratch.arena, &first_system_string, "%.*s\n", MD_S8VArg(points_at_chunk(cur)));
	}
	//MD_S8ListPush(scratch.arena, &list, make_json_node(scratch.arena, MSG_SYSTEM, MD_S8ListJoin(scratch.arena, first_system_string, &(MD_StringJoin){0})));
	MD_S8ListPush(scratch.arena, &list, make_json_node(scratch.arena, MSG_SYSTEM, MD_S8ListJoin(scratch.arena, first_system_string, &(MD_StringJoin){0})));

	for(Memory *it = e->memories_first; it; it = it->next)
	{
		MessageType sent_type = -1;
		MD_String8List cur_list = {0};
		MD_String8 context_string = {0};

		PushWithLint(scratch.arena, &cur_list, "{");
		if(it->context.i_said_this) assert(it->context.author_npc_kind == e->npc_kind);
		PushWithLint(scratch.arena, &cur_list, "who_i_am: \"%s\", ", characters[it->context.author_npc_kind].name);
		MD_String8 speech = MD_S8(it->speech, it->speech_length);

		PushWithLint(scratch.arena, &cur_list, "talking_to: \"%s\", ", it->context.was_talking_to_somebody ? characters[it->context.talking_to_kind].name : "nobody");

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

		// add internal things
		if(it->context.i_said_this)
		{
			PushWithLint(scratch.arena, &cur_list, "thoughts: \"%.*s\", ", MD_S8VArg(MD_S8(it->internal_monologue, it->internal_monologue_length)));
			PushWithLint(scratch.arena, &cur_list, "mood: %s, ", moods[it->mood]);
		}

		// add action
		PushWithLint(scratch.arena, &cur_list, "action: %s, ", actions[it->action_taken].name);
		if(actions[it->action_taken].takes_argument)
		{
			if(it->action_taken == ACT_gift_item_to_targeting)
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
		MD_S8ListPush(scratch.arena, &list, make_json_node(scratch.arena, sent_type, MD_S8ListJoin(scratch.arena, cur_list, &(MD_StringJoin){0})));
	}


	MD_String8List latest_state = {0};

	const char *standing_string = 0;
	{
			if (e->standing == STANDING_INDIFFERENT)
			{
					standing_string = "You are currently indifferent towards the player.";
			}
			else if (e->standing == STANDING_JOINED)
			{
					standing_string = "You have joined the player, and are following them everywhere they go! This means you're on their side.";
			}
			else if (e->standing == STANDING_FIGHTING)
			{
					standing_string = "You are fighting the player right now! That means that the player can't leave conversation with you until you stop fighting them, effectively trapping the player with you.";
			}
			else
			{
				assert(false);
			}
	}
	PushWithLint(scratch.arena, &latest_state, "%s\n", standing_string);

	if(e->held_items.cur_index > 0)
	{
			PushWithLint(scratch.arena, &latest_state, "You have these items in their inventory: [\n");
			BUFF_ITER(ItemKind, &e->held_items)
			{
				PushWithLint(scratch.arena, &latest_state, "'%s' - %s,\n", items[*it].name, items[*it].description);
			}
			PushWithLint(scratch.arena, &latest_state, "]\n");
	}
	else
	{
			PushWithLint(scratch.arena, &latest_state, "Your inventory is EMPTY right now. That means if you gave something to the player expecting them to give you something, they haven't held up their end of the bargain!\n");
	}

	AvailableActions available = { 0 };
	fill_available_actions(e, &available);
	PushWithLint(scratch.arena, &latest_state, "The actions you can perform: [\n");
	BUFF_ITER_I(ActionKind, &available, i)
	{
		if(actions[*it].description)
		{
			PushWithLint(scratch.arena, &latest_state, "%s - %s,\n", actions[*it].name, actions[*it].description);
		}
		else
		{
			PushWithLint(scratch.arena, &latest_state, "%s,\n", actions[*it].name);
		}
	}
	PushWithLint(scratch.arena, &latest_state, "]\n");

	PushWithLint(scratch.arena, &latest_state, "You must output a mood every generation. The moods are parsed by code that expects your mood to exactly match one in this list: [");
	for(int i = 0; i < ARRLEN(moods); i++)
	{
		PushWithLint(scratch.arena, &latest_state, "%s, ", moods[i]);
	}
	PushWithLint(scratch.arena, &latest_state, "]\n");

	PushWithLint(scratch.arena, &latest_state, "The characters close enough for you to talk to with `talking_to`: [");
	BUFF_ITER(NpcKind, &can_talk_to)
	{
		PushWithLint(scratch.arena, &latest_state, "\"%s\", ", characters[*it].name);
	}
	PushWithLint(scratch.arena, &latest_state, "]\n");

	// last thought explanation and re-prompt
	{
		Memory *last_memory_that_was_me = 0;
		for(Memory *cur = e->memories_first; cur; cur = cur->next)
		{
			if(cur->context.i_said_this)
			{
				last_memory_that_was_me = cur;
			}
		}
		if(last_memory_that_was_me)
		{
			MD_String8 last_thought_string = MD_S8(last_memory_that_was_me->internal_monologue, last_memory_that_was_me->internal_monologue_length);
			PushWithLint(scratch.arena, &latest_state, "Your last thought was: %.*s\nYour current mood is %s, make sure you act like it!", MD_S8VArg(last_thought_string), moods[last_memory_that_was_me->mood]);
		}
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
	MD_String8 who_i_am_str = {0};
	MD_String8 talking_to_str = {0};
	MD_String8 mood_str = {0};
	if(error_message.size == 0)
	{
		action_str = get_field(message_obj, MD_S8Lit("action"));
		who_i_am_str = get_field(message_obj, MD_S8Lit("who_i_am"));
		speech_str = get_field(message_obj, MD_S8Lit("speech"));
		thoughts_str = get_field(message_obj, MD_S8Lit("thoughts"));
		action_arg_str = get_field(message_obj, MD_S8Lit("action_arg"));
		talking_to_str = get_field(message_obj, MD_S8Lit("talking_to"));
		mood_str = get_field(message_obj, MD_S8Lit("mood"));
	}
	if(error_message.size == 0 && who_i_am_str.size == 0)
	{
		error_message = MD_S8Lit("Expected field named `who_i_am` in message");
	}
	if(error_message.size == 0 && action_str.size == 0)
	{
		error_message = MD_S8Lit("Expected field named `action` in message");
	}
	if(error_message.size == 0 && talking_to_str.size == 0)
	{
		error_message = MD_S8Lit("You must have a field named `talking_to` in your message");
	}
	if(error_message.size == 0 && mood_str.size == 0)
	{
		error_message = MD_S8Lit("You must have a field named `mood` in your message");
	}
	if(error_message.size == 0 && thoughts_str.size == 0)
	{
		error_message = MD_S8Lit("You must have a field named `thoughts` in your message, and it must have nonzero size. Like { ... thoughts: \"<your thoughts>\" ... }");
	}
	if(error_message.size == 0 && speech_str.size >= MAX_SENTENCE_LENGTH)
	{
		error_message = FmtWithLint(arena, "Speech string provided is too big, maximum bytes is %d", MAX_SENTENCE_LENGTH);
	}
	if(error_message.size == 0 && thoughts_str.size >= MAX_SENTENCE_LENGTH)
	{
		error_message = FmtWithLint(arena, "Thoughts string provided is too big, maximum bytes is %d", MAX_SENTENCE_LENGTH);
	}

	assert(!e->is_character); // player can't perform AI actions?
	MD_String8 my_name = MD_S8CString(characters[e->npc_kind].name);
	if(error_message.size == 0 && !MD_S8Match(who_i_am_str, my_name, 0))
	{
		error_message = FmtWithLint(arena, "You are acting as `%.*s`, not what you said in who_i_am, `%.*s`", MD_S8VArg(my_name), MD_S8VArg(who_i_am_str));
	}

	if(error_message.size == 0)
	{
		if(MD_S8Match(talking_to_str, MD_S8Lit("nobody"), 0))
		{
			out->talking_to_somebody = false;
		}
		else
		{
			bool found = false;
			for(int i = 0; i < ARRLEN(characters); i++)
			{
				if(MD_S8Match(talking_to_str, MD_S8CString(characters[i].name), 0))
				{
					found = true;
                    out->talking_to_somebody = true;
					out->talking_to_kind = i;
				}
			}
			if(!found)
			{
				error_message = FmtWithLint(arena, "Unrecognized character provided in talking_to: `%.*s`", MD_S8VArg(talking_to_str));
			}
		}
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
		int it_index = 0;
		ARR_ITER(ActionInfo, actions)
		{
			MD_String8 cur_action_string = MD_S8CString(it->name);
			MD_S8ListPush(scratch.arena, &action_strings, cur_action_string);
			if(MD_S8Match(cur_action_string, action_str, 0))
			{
				out->kind = it_index;
				found_action = true;
			}
			it_index += 1;
		}

		if(!found_action)
		{
			MD_String8 list_of_actions = MD_S8ListJoin(scratch.arena, action_strings, &(MD_StringJoin){.mid = MD_S8Lit(", ")});

			error_message = FmtWithLint(arena, "Couldn't find valid action in game from string `%.*s`. Available actions: [%.*s]", MD_S8VArg(action_str), MD_S8VArg(list_of_actions));
		}
	}

	if(error_message.size == 0)
	{
		if(actions[out->kind].takes_argument)
		{
			MD_String8List item_enum_names = {0};
			if(out->kind == ACT_gift_item_to_targeting)
			{
				bool found_item = false;
				BUFF_ITER(ItemKind, &e->held_items)
				{
					MD_String8 cur_item_string = MD_S8CString(items[*it].name);
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

	if(error_message.size == 0)
	{
		bool found = false;
		for(int i = 0; i < ARRLEN(moods); i++)
		{
			if(MD_S8Match(MD_S8CString(moods[i]), mood_str, 0))
			{
				out->mood = i;
				found = true;
				break;
			}
		}
		if(!found)
		{
			error_message = FmtWithLint(arena, "Game does not recognize the mood '%.*s', you must use an available mood from the list provided.", MD_S8VArg(mood_str));
		}
	}

	MD_ReleaseScratch(scratch);
	return error_message;
}
