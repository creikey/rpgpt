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

typedef struct
{
	NpcKind targeting;
} ActionArgument;

// returns ai understandable, human readable name, so not the enum name
MD_String8 action_argument_string(ActionArgument arg)
{
	return MD_S8CString(characters[arg.targeting].name);
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
	bool dont_show_to_player; // jester and past memories are hidden to the player when made into dialog
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

typedef enum CharacterState
{
	CHARACTER_WALKING,
	CHARACTER_IDLE,
	CHARACTER_KILLED,
} CharacterState;

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
#define TextChunkString8(t) MD_S8((MD_u8*)t.text, t.text_length)
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
	bool is_character;

	// fields for all gs.entities
	Vec2 pos;
	Vec2 last_moved;
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




	// character
	bool waiting_on_speech_with_somebody;
	EntityRef interacting_with; // for drawing outline on maybe interacting with somebody
	Vec2 to_throw_direction;
	BUFF(Vec2, 8) position_history; // so npcs can follow behind the player
	CharacterState state;
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
	
	// processing may still occur after time has stopped on the gamestate, 
	bool stopped_time;

	// these must point entities in its own array.
	Entity *player;
	Entity *world_entity;
	Entity entities[MAX_ENTITIES];
} GameState;

#define ENTITIES_ITER(ents) for (Entity *it = ents; it < ents + ARRLEN(ents); it++) if (it->exists && !it->destroy && it->generation > 0)

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
			assert((*it)->is_npc || (*it)->is_character);
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
	for(Memory *it = e->memories_first; it; it = it->next)
	{
		// going through memories, I'm going to accumulate human understandable sentences for what happened in current_list.
		// when I see an 'i_said_this' memory, that means I flush. and add a new assistant node.


		// write a new human understandable sentence or two to current_list
		if (!it->context.i_said_this) {
			// dump a human understandable sentence description of what happened in this memory
			if(it->action_taken != ACT_none)
			{
				switch(it->action_taken)
				{
				case ACT_none:
					break;
				case ACT_join:
					AddFmt("%s joined %s\n", characters[it->context.author_npc_kind].name, characters[it->action_argument.targeting].name);
					break;
				case ACT_leave:
					AddFmt("%s left their party\n", characters[it->context.author_npc_kind].name);
					break;
				case ACT_aim_shotgun:
					AddFmt("%s aimed their shotgun at %s\n", characters[it->context.author_npc_kind].name, characters[it->action_argument.targeting].name);
					break;
				case ACT_fire_shotgun:
					AddFmt("%s fired their shotgun at %s, brutally murdering them.\n", characters[it->context.author_npc_kind].name, characters[it->action_argument.targeting].name);
					break;
				case ACT_put_shotgun_away:
					AddFmt("%s holstered their shotgun, no longer threatening anybody\n", characters[it->context.author_npc_kind].name);
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
			AddFmt("\"action_argument\":\"%.*s\",", MD_S8VArg(action_argument_string(it->action_argument)));
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
	assert(!e->is_character); // player can't perform AI actions?

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
				// @TODO refactor into, action argument kinds and they parse into different action argument types
				bool arg_is_character = out->kind == ACT_join || out->kind == ACT_aim_shotgun;

				if(arg_is_character)
				{
					bool found_npc = false;
					for(int i = 0; i < ARRLEN(characters); i++)
					{
						if(MD_S8Match(MD_S8CString(characters[i].name), action_argument_str, 0))
						{
							found_npc = true;
							out->argument.targeting = i;
						}
					}
					if(!found_npc)
					{
						error_message = FmtWithLint(arena, "Argument for action `%.*s` you gave is `%.*s`, which doesn't exist in the game so is invalid", MD_S8VArg(action_str), MD_S8VArg(action_argument_str));
					}
				}
				else
				{
					assert(false); // don't know how to parse the argument string for this kind of action...
				}
			}
		}
	}

	MD_ReleaseScratch(scratch);
	return error_message;
}
