#pragma once

#include "buff.h"
#include "HandmadeMath.h" // vector types in entity struct definition
#include "utility.h"
#include <stdbool.h>
#include <string.h>
#include <stdlib.h> // atoi
#include "character_info.h"
#include "characters.gen.h"
#include "rnd.h"

#include "tuning.h"

#define DO_CHATGPT_PARSING

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

typedef struct TextChunk
{
	char text[MAX_SENTENCE_LENGTH];
	int text_length;
} TextChunk;

typedef struct TextChunkList
{
	struct TextChunkList *next;
	struct TextChunkList  *prev;
	TextChunk text;
} TextChunkList;

typedef struct GameplayObjective
{
	ObjectiveVerb verb;
	NpcKind who_to_kill;
} GameplayObjective;

typedef enum
{
	ARG_CHARACTER,
	ARG_OBJECTIVE,
} ActionArgumentKind;

typedef struct
{
	ActionArgumentKind kind;
	NpcKind targeting;
	GameplayObjective objective;
} ActionArgument;

// returns ai understandable, human readable name, on the arena, so not the enum name
MD_String8 action_argument_string(MD_Arena *arena, ActionArgument arg)
{
	switch(arg.kind)
	{
		case ARG_CHARACTER:
		return FmtWithLint(arena, "%s", characters[arg.targeting].name);
		break;
		case ARG_OBJECTIVE:
		return FmtWithLint(arena, "%s %s", verbs[arg.objective.verb], characters[arg.objective.who_to_kill].enum_name);
	}
	return (MD_String8){0};
}

typedef struct Action
{
	ActionKind kind;
	ActionArgument argument;

	TextChunk speech;

	NpcKind talking_to_kind;
} Action;

typedef struct 
{
	bool i_said_this; // don't trigger npc action on own self memory modification
	NpcKind author_npc_kind;
	NpcKind talking_to_kind;
	bool heard_physically; // if not physically, the source was directly
	bool drama_memory; // drama memories arent forgotten, and once they end it's understood that a lot of time has passed.
} MemoryContext;

// memories are subjective to an individual NPC
typedef struct Memory
{
	struct Memory *prev;
	struct Memory *next;

	// if action_taken is none, there might still be speech. If speech_length == 0 and action_taken == none, it's an invalid memory and something has gone wrong
	ActionKind action_taken;
	ActionArgument action_argument;

	MemoryContext context;
	TextChunk speech;
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

typedef enum
{
	STANDING_INDIFFERENT,
	STANDING_JOINED,
} NPCPlayerStanding;

typedef Vec4 Color;

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


// text chunk must be a literal, not a pointer
// and this returns a s8 that points at the text chunk memory
#define TextChunkString8(t) MD_S8((MD_u8*)(t).text, (t).text_length)
#define TextChunkVArg(t) MD_S8VArg(TextChunkString8(t))

void chunk_from_s8(TextChunk *into, MD_String8 from)
{
	assert(from.size < ARRLEN(into->text));
	memset(into->text, 0, ARRLEN(into->text));
	memcpy(into->text, from.str, from.size);
	into->text_length = (int)from.size;
}

typedef struct Entity
{
	bool exists;
	bool destroy;
	int generation;

	// the kinds are at the top so you can quickly see what kind an entity is in the debugger
	bool is_world; // the static world. An entity is always returned when you collide with something so support that here
	bool is_npc;

	// fields for all gs.entities
	Vec2 pos;
	Vec2 last_moved;
	float quick_turning_timer;
	float rotation;
	Vec2 vel; // only used sometimes, like in old man and bullet
	float damage; // at 1.0, dead! zero initialized
	bool dead;

	// npcs
	NpcKind npc_kind;
	EntityRef joined;
	EntityRef aiming_shotgun_at;
	EntityRef looking_at; // aiming shotgun at takes facing priority over this
	bool killed;
	float target_rotation; // turns towards this angle in conversation
	bool being_hovered;
	bool perceptions_dirty;
	float dialog_fade;
	TextChunkList *errorlist_first;
	TextChunkList *errorlist_last;
#ifdef DESKTOP
	int times_talked_to; // for better mocked response string
#endif
	float loading_anim_in;
	Memory *memories_first;
	Memory *memories_last;
	Memory *memories_added_while_time_stopped;
	float dialog_panel_opacity;

	// last_said_sentence(entity) contains the dialog the player has yet to see
	bool undismissed_action;
	uint64_t undismissed_action_tick;
	float characters_of_word_animated;
	int words_said_on_page;
	int cur_page_index;

	PathCacheHandle cached_path;
	int gen_request_id;
	Vec2 target_goto;

	EntityRef interacting_with; // for drawing outline on maybe interacting with somebody
	BUFF(Vec2, 8) position_history; // so npcs can follow behind the player
	EntityRef talking_to;
} Entity;

typedef BUFF(Entity*, 32) CanTalkTo;

float entity_max_damage(Entity *e)
{
	(void)e;
	return 1.0f;
}

typedef BUFF(ActionKind, 8) AvailableActions;


typedef struct GameState {
	uint64_t tick;
	bool won;
	
	MD_String8 current_room_name; // the string is allocated on the level that is currently loaded
	bool finished_reading_dying_dialog;
	bool no_angel_screen;
	bool assigned_objective;
	GameplayObjective objective;
	
	// processing may still occur after time has stopped on the gamestate, 
	bool stopped_time;

	// these must point entities in its own array.
	Entity *player;
	Entity *world_entity;
	Entity entities[MAX_ENTITIES];
	rnd_gamerand_t random;
} GameState;

#define ENTITIES_ITER(ents) for (Entity *it = ents; it < ents + ARRLEN(ents); it++) if (it->exists && !it->destroy && it->generation > 0)

float g_randf(GameState *gs)
{
	return rnd_gamerand_nextf(&gs->random);
}

Entity *gete_specified(GameState *gs, EntityRef ref)
{
	if (ref.generation == 0) return 0;
	Entity *to_return = &gs->entities[ref.index];
	if (!to_return->exists || to_return->generation != ref.generation)
	{
		return 0;
	}
	else
	{
		return to_return;
	}
}

void fill_available_actions(GameState *gs, Entity *it, AvailableActions *a)
{
	*a = (AvailableActions) { 0 };
	BUFF_APPEND(a, ACT_none);

	if(gete_specified(gs, it->joined))
	{
		BUFF_APPEND(a, ACT_leave)
	}
	else
	{
		BUFF_APPEND(a, ACT_join)
	}

	if(it->npc_kind == NPC_Angel)
	{
		BUFF_APPEND(a, ACT_assign_gameplay_objective);
	}

	bool has_shotgun = it->npc_kind == NPC_Daniel;
	if(has_shotgun)
	{
		if(gete_specified(gs, it->aiming_shotgun_at))
		{
			BUFF_APPEND(a, ACT_put_shotgun_away);
			BUFF_APPEND(a, ACT_fire_shotgun);
		}
		else
		{
			BUFF_APPEND(a, ACT_aim_shotgun);
		}
	}
}


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

MD_String8 npc_to_human_readable(Entity *me, NpcKind kind)
{
	if(me->npc_kind == kind)
	{
		return MD_S8Lit("You");
	}
	else
	{
		return MD_S8CString(characters[kind].name);
	}
}

// outputs json which is parsed by the server
MD_String8 generate_chatgpt_prompt(MD_Arena *arena, GameState *gs, Entity *e, CanTalkTo can_talk_to)
{
	assert(e->is_npc);
	assert(e->npc_kind < ARRLEN(characters));

	MD_ArenaTemp scratch = MD_GetScratch(&arena, 1);

	MD_String8List list = {0};

	PushWithLint(scratch.arena, &list, "[");

	#define AddFmt(...) PushWithLint(scratch.arena, &current_list, __VA_ARGS__)
	#define AddNewNode(node_type) { MD_S8ListPush(scratch.arena, &list, make_json_node(scratch.arena, node_type, MD_S8ListJoin(scratch.arena, current_list, &(MD_StringJoin){0}))); current_list = (MD_String8List){0}; }
	

	// make first system node
	{
		MD_String8List current_list = {0};
		AddFmt("%s\n\n", global_prompt);
		AddFmt("%s\n\n", characters[e->npc_kind].prompt);
		AddFmt("The characters who are near you, that you can target:\n");
		BUFF_ITER(Entity*, &can_talk_to)
		{
			assert((*it)->is_npc);
			MD_String8 info = MD_S8Lit("");
			if((*it)->killed)
			{
				info = MD_S8Lit(" - they're currently dead, they were murdered");
			}
			AddFmt("%s%.*s\n", characters[(*it)->npc_kind].name, MD_S8VArg(info));
		}
		AddFmt("\n");

		// @TODO unhardcode this, this will be a description of where the character is right now
		AddFmt("You're currently standing in Daniel's farm's barn, a run-down structure that barely serves its purpose. Daniel's mighty protective of it though.\n");

		if(e->npc_kind == NPC_Angel)
		{
			AddFmt("Acceptable verbs for assigning a gameplay objective:\n");
			ARR_ITER(char*, verbs)
			{
				AddFmt("%s\n", *it);
			}
			AddFmt("\n");

			AddFmt("The characters in the game you can use when you assign your gameplay objective:\n");
			AddFmt("Raphael\n");
			AddFmt("Daniel\n");
			AddFmt("\n");
		}

		AddFmt("The actions you can perform, what they do, and the arguments they expect:\n");
		AvailableActions can_perform;
		fill_available_actions(gs, e, &can_perform);
		BUFF_ITER(ActionKind, &can_perform)
		{
			AddFmt("%s - %s - %s\n", actions[*it].name, actions[*it].description, actions[*it].argument_description);
		}

		AddNewNode(MSG_SYSTEM);
	}

	MD_String8List current_list = {0};
	bool in_drama_memories = true;
	assert(e->memories_first->context.drama_memory);
	for(Memory *it = e->memories_first; it; it = it->next)
	{
		// going through memories, I'm going to accumulate human understandable sentences for what happened in current_list.
		// when I see an 'i_said_this' memory, that means I flush. and add a new assistant node.


		// write a new human understandable sentence or two to current_list
		if (!it->context.i_said_this) {
			if(in_drama_memories && !it->context.drama_memory)
			{
				in_drama_memories = false;
				AddFmt("Some time passed...\n");
			}
			// dump a human understandable sentence description of what happened in this memory
			bool no_longer_wants_to_converse = false; // add the no longer wants to converse text after any speech, it makes more sense reading it
			if(it->action_taken != ACT_none)
			{
				switch(it->action_taken)
				{
				#define HUMAN(kind) MD_S8VArg(npc_to_human_readable(e, kind))
				case ACT_none:
					break;
				case ACT_join:
					AddFmt("%.*s joined %.*s\n", HUMAN(it->context.author_npc_kind), HUMAN(it->action_argument.targeting));
					break;
				case ACT_leave:
					AddFmt("%.*s left their party\n",  HUMAN(it->context.author_npc_kind));
					break;
				case ACT_aim_shotgun:
					AddFmt("%.*s aimed their shotgun at %.*s\n", HUMAN(it->context.author_npc_kind), HUMAN(it->action_argument.targeting));
					break;
				case ACT_fire_shotgun:
					AddFmt("%.*s fired their shotgun at %.*s, brutally murdering them.\n", HUMAN(it->context.author_npc_kind), HUMAN(it->action_argument.targeting));
					break;
				case ACT_put_shotgun_away:
					AddFmt("%.*s holstered their shotgun, no longer threatening anybody\n", HUMAN(it->context.author_npc_kind));
					break;
				case ACT_approach:
					AddFmt("%.*s approached %.*s\n", HUMAN(it->context.author_npc_kind), HUMAN(it->action_argument.targeting));
					break;
				case ACT_end_conversation:
				  	no_longer_wants_to_converse = true;
					break;
				case ACT_assign_gameplay_objective:
				 	AddFmt("%.*s assigned a definitive game objective to %.*s", HUMAN(it->context.author_npc_kind), HUMAN(it->context.talking_to_kind));
					break;
				}
			}
			if(it->speech.text_length > 0)
			{
				MD_String8 target_string = MD_S8Lit("the world");
				if(it->context.talking_to_kind != NPC_nobody)
				{
					if(it->context.talking_to_kind == e->npc_kind)
						target_string = MD_S8Lit("you");
					else
						target_string = MD_S8CString(characters[it->context.talking_to_kind].name);
				}

				MD_String8 speaking_to_you_helper = MD_S8Lit("(Speaking directly you) ");
				if(it->context.talking_to_kind != e->npc_kind)
				{
					speaking_to_you_helper = MD_S8Lit("(Overheard conversation, they aren't speaking directly to you) ");
				}

				AddFmt("%.*s%s said \"%.*s\" to %.*s (you are %s)\n", MD_S8VArg(speaking_to_you_helper), characters[it->context.author_npc_kind].name, TextChunkVArg(it->speech), MD_S8VArg(target_string), characters[e->npc_kind].name);
			}

			if(no_longer_wants_to_converse)
			{
				if (it->action_argument.targeting == NPC_nobody)
				{
					AddFmt("%.*s no longer wants to converse with everybody\n", HUMAN(it->context.author_npc_kind));
				}
				else
				{
					AddFmt("%.*s no longer wants to converse with %.*s\n", HUMAN(it->context.author_npc_kind), HUMAN(it->action_argument.targeting));
				}
			}
				#undef HUMAN
		}

		// if I said this, or it's the last memory, flush the current list as a user node
		if(it->context.i_said_this || it == e->memories_last)
		{
			if(it == e->memories_last && e->errorlist_first)
			{
				AddFmt("Errors you made: \n");
				for(TextChunkList *cur = e->errorlist_first; cur; cur = cur->next)
				{
					AddFmt("%.*s\n", TextChunkVArg(cur->text));
				}
			}
			if(current_list.node_count > 0)
				AddNewNode(MSG_USER);
		}

		if(it->context.i_said_this)
		{
			MD_String8List current_list = {0}; // shadow the list of human understandable sentences to quickly flush 
			AddFmt("{");
			AddFmt("\"speech\":\"%.*s\",", TextChunkVArg(it->speech));
			AddFmt("\"action\":\"%s\",", actions[it->action_taken].name);
			MD_String8 arg_str = action_argument_string(scratch.arena, it->action_argument);
			AddFmt("\"action_argument\":\"%.*s\",", MD_S8VArg(arg_str));
			AddFmt("\"target\":\"%s\"}", characters[it->context.talking_to_kind].name);
			AddNewNode(MSG_ASSISTANT);
		}
	}
	MD_String8 with_trailing_comma = MD_S8ListJoin(scratch.arena, list, &(MD_StringJoin){MD_S8Lit(""),MD_S8Lit(""),MD_S8Lit(""),});
	MD_String8 no_trailing_comma = MD_S8Chop(with_trailing_comma, 1);

	MD_String8 to_return = MD_S8Fmt(arena, "%.*s]", MD_S8VArg(no_trailing_comma));

	MD_ReleaseScratch(scratch);

	return to_return;
}

MD_String8 get_field(MD_Node *parent, MD_String8 name)
{
	return MD_ChildFromString(parent, name, 0)->first_child->string;
}

void parse_action_argument(MD_Arena *error_arena, MD_String8 *cur_error_message, ActionKind action, MD_String8 action_argument_str, ActionArgument *out)
{
	assert(cur_error_message);
	if(cur_error_message->size > 0) return;
	MD_ArenaTemp scratch = MD_GetScratch(&error_arena, 1);
	MD_String8 action_str = MD_S8CString(actions[action].name);
	// @TODO refactor into, action argument kinds and they parse into different action argument types
	bool arg_is_character = action == ACT_join || action == ACT_aim_shotgun || action == ACT_end_conversation;
	bool arg_is_gameplay_objective = action == ACT_assign_gameplay_objective;

	if (arg_is_character)
	{
		out->kind = ARG_CHARACTER;
		bool found_npc = false;
		for (int i = 0; i < ARRLEN(characters); i++)
		{
			if (MD_S8Match(MD_S8CString(characters[i].name), action_argument_str, 0))
			{
				found_npc = true;
				(*out).targeting = i;
			}
		}
		if (!found_npc)
		{
			*cur_error_message = FmtWithLint(error_arena, "Argument for action `%.*s` you gave is `%.*s`, which doesn't exist in the game so is invalid", MD_S8VArg(action_str), MD_S8VArg(action_argument_str));
		}
	}
	else if (arg_is_gameplay_objective)
	{
		out->kind = ARG_OBJECTIVE;
		MD_String8List split = MD_S8Split(scratch.arena, action_argument_str, 1, &MD_S8Lit(" "));
		if (split.node_count != 2)
		{
			*cur_error_message = FmtWithLint(error_arena, "Gameplay objective must follow this format: `VERB ACTION`, with a space between the verb and the action. You gave a response with %d words in it, which isn't the required amount, 2", (int)split.node_count);
		}

		if (cur_error_message->size == 0)
		{
			MD_String8 verb = split.first->string;

			bool found = false;
			MD_String8List available_verbs = {0};
			ARR_ITER_I(char *, verbs, verb_enum)
			{
				MD_S8ListPush(scratch.arena, &available_verbs, MD_S8CString(*it));
				if (MD_S8Match(MD_S8CString(*it), verb, 0))
				{
					(*out).objective.verb = verb_enum;
					found = true;
				}
			}
			if (!found)
			{
				MD_String8 verbs_str = MD_S8ListJoin(scratch.arena, available_verbs, &(MD_StringJoin){.mid = MD_S8Lit(" ")});
				*cur_error_message = FmtWithLint(error_arena, "The gameplay verb you provided '%.*s' doesn't match any of the gameplay verbs available to you: %.*s", MD_S8VArg(verb), MD_S8VArg(verbs_str));
			}
		}

		if (cur_error_message->size == 0)
		{
			MD_String8 subject = split.first->next->string;
			MD_String8 verb = split.first->string;
			bool found_npc = false;
			for (int i = 0; i < ARRLEN(characters); i++)
			{
				if (MD_S8Match(MD_S8CString(characters[i].name), subject, 0))
				{
					found_npc = true;
					(*out).objective.who_to_kill = i;
				}
			}
			if (!found_npc)
			{
				*cur_error_message = FmtWithLint(error_arena, "Argument for gameplay verb `%.*s` you gave is `%.*s`, which doesn't exist in the game so is invalid", MD_S8VArg(verb), MD_S8VArg(subject));
			}
		}
	}
	else
	{
		assert(false); // don't know how to parse the argument string for this kind of action...
	}
	MD_ReleaseScratch(scratch);
}

// if returned string has size greater than 0, it's the error message. Allocated
// on arena passed into it or in constant memory
MD_String8 parse_chatgpt_response(MD_Arena *arena, Entity *e, MD_String8 action_in_json, Action *out)
{
	MD_ArenaTemp scratch = MD_GetScratch(&arena, 1);

	MD_String8 error_message = {0};

	*out = (Action) { 0 };

	MD_ParseResult result = MD_ParseWholeString(scratch.arena, MD_S8Lit("chat_message"), action_in_json);
	if(result.errors.node_count > 0)
	{
		MD_Message *cur = result.errors.first;
		MD_CodeLoc loc = MD_CodeLocFromNode(cur->node);
		error_message = FmtWithLint(arena, "Parse Error on column %d: %.*s", loc.column, MD_S8VArg(cur->string));
	}

	MD_Node *message_obj = result.node->first_child;

	MD_String8 speech_str = {0};
	MD_String8 action_str = {0};
	MD_String8 action_argument_str = {0};
	MD_String8 target_str = {0};
	if(error_message.size == 0)
	{
		speech_str = get_field(message_obj, MD_S8Lit("speech"));
		action_str = get_field(message_obj, MD_S8Lit("action"));
		action_argument_str = get_field(message_obj, MD_S8Lit("action_argument"));
		target_str = get_field(message_obj, MD_S8Lit("target"));
	}
	if(error_message.size == 0 && action_str.size == 0)
	{
		error_message = MD_S8Lit("The field `action` must be of nonzero length, if you don't want to do anything it should be `none`");
	}
	if(error_message.size == 0 && action_str.size == 0)
	{
		error_message = MD_S8Lit("The field `target` must be of nonzero length, if you don't want to target anybody it should be `nobody`");
	}	if(error_message.size == 0 && speech_str.size >= MAX_SENTENCE_LENGTH)
	{
		error_message = FmtWithLint(arena, "Speech string provided is too big, maximum bytes is %d", MAX_SENTENCE_LENGTH);
	}
	assert(e->npc_kind != NPC_Player); // player can't perform AI actions?

	if(error_message.size == 0)
	{
		if(MD_S8Match(target_str, MD_S8Lit("nobody"), 0))
		{
			out->talking_to_kind = NPC_nobody;
		}
		else
		{
			bool found = false;
			for(int i = 0; i < ARRLEN(characters); i++)
			{
				if(MD_S8Match(target_str, MD_S8CString(characters[i].name), 0))
				{
					found = true;
                    out->talking_to_kind = i;
				}
			}
			if(!found)
			{
				error_message = FmtWithLint(arena, "Unrecognized character provided in field 'target': `%.*s`", MD_S8VArg(target_str));
			}
		}
	}

	if(error_message.size == 0)
	{
		memcpy(out->speech.text, speech_str.str, speech_str.size);
		out->speech.text_length = (int)speech_str.size;
	}

	if(error_message.size == 0)
	{
		bool found_action = false;
		for(int i = 0; i < ARRLEN(actions); i++)
		{
			if(MD_S8Match(MD_S8CString(actions[i].name), action_str, 0))
			{
				assert(!found_action);
				found_action = true;
				out->kind = i;
			}
		}
		if(!found_action)
		{
			error_message = FmtWithLint(arena, "Action `%.*s` is invalid, doesn't exist in the game", MD_S8VArg(action_str));
		}

		if(error_message.size == 0)
		{
			if(actions[out->kind].takes_argument)
			{
				parse_action_argument(arena,&error_message, out->kind, action_argument_str, &out->argument);
			}
		}
	}

	MD_ReleaseScratch(scratch);
	return error_message;
}