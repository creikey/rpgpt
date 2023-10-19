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


typedef BUFF(char, 1024 * 10) Escaped;

bool character_valid(char c)
{
	return c <= 126 && c >= 32;
}

String8 escape_for_json(Arena *arena, String8 from)
{
	u64 output_size = 0;
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

	String8 output = {
		.str = PushArray(arena, u8, output_size),
		.size = output_size,
	};
	u64 output_cursor = 0;

	for(u64 i = 0; i < from.size; i++)
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

typedef enum
{
	ARG_CHARACTER,
} ActionArgumentKind;

// A value of 0 means no npc. So is invalid if you're referring to somebody.
typedef int NpcKind;
#define NPC_nobody 0
#define NPC_player 1

typedef struct
{
	ActionArgumentKind kind;
	NpcKind targeting;
} ActionArgument;


typedef struct ActionOld
{
	ActionKind kind;
	ActionArgument argument;

	TextChunk speech;

	NpcKind talking_to_kind;
} ActionOld;

typedef struct 
{
	bool i_said_this; // don't trigger npc action on own self memory modification
	NpcKind author_npc_kind;
} MemoryContext;

// memories are subjective to an individual NPC
typedef struct Memory
{
	struct Memory *prev;
	struct Memory *next;

	ActionOld action;
	MemoryContext context;
} Memory;

// text chunk must be a literal, not a pointer
// and this returns a s8 that points at the text chunk memory
#define TextChunkString8(t) S8((u8*)(t).text, (t).text_length)
#define TextChunkVArg(t) S8VArg(TextChunkString8(t))
#define TextChunkLitC(s) {.text = s, .text_length = sizeof(s) - 1} // sizeof includes the null terminator. Not good.
#define TextChunkLit(s) (TextChunk) TextChunkLitC(s)

void chunk_from_s8(TextChunk *into, String8 from)
{
	assert(from.size < ARRLEN(into->text));
	memset(into->text, 0, ARRLEN(into->text));
	memcpy(into->text, from.str, from.size);
	into->text_length = (int)from.size;
}
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


typedef struct RememberedError
{
	struct RememberedError *next;
	struct RememberedError *prev;
	Memory offending_self_output; // sometimes is just zero value if reason why it's bad is severe enough.
	TextChunk reason_why_its_bad;
} RememberedError;

typedef int TextInputResultKey;

typedef struct Entity
{
	bool exists;
	bool destroy;
	int generation;

	// the kinds are at the top so you can quickly see what kind an entity is in the debugger
	bool is_world; // the static world. An entity is always returned when you collide with something so support that here
	bool is_npc;
	NpcKind npc_kind;

	// fields for all gs.entities
	Vec2 pos;
	Vec2 last_moved;
	float quick_turning_timer;
	float rotation;
	Vec2 vel; // only used sometimes, like in old man and bullet
	float damage; // at 1.0, dead! zero initialized
	bool dead;
	u64 current_roomid;

	// npcs
	TextInputResultKey player_input_key;
	void *armature; // copied into the gamestate's arena, created if null. Don't serialize
	EntityRef joined;
	EntityRef aiming_shotgun_at;
	EntityRef looking_at; // aiming shotgun at takes facing priority over this
	bool killed;
	float target_rotation; // turns towards this angle in conversation
	bool being_hovered;
	bool perceptions_dirty;
	float dialog_fade;
	RememberedError *errorlist_first;
	RememberedError *errorlist_last;
	int times_talked_to; // for better mocked response string
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

typedef enum HealthStatus {
	HEALTH_ok,
	HEALTH_decent,
	HEALTH_dying,
	HEALTH_verge_of_death,
} HealthStatus;

// Whatever this health is, it can be perceived by others, e.g it's public
typedef struct Health {
	HealthStatus status;
	float drunkenness; // 1.0 is max drunkenness
} Health;


// these are items and events that are available during the game, but 'rendered' to different structs
// when sent to the AI as text so that they're more stable. I.E, if you change the name of an item or an index,
// old memories still work, old reactions to items in a room still work, etc. 
typedef enum ItemKind {
	ITEM_none,
	ITEM_whiskey,
} ItemKind;

typedef struct Item {
	ItemKind kind;
	int times_used;
} Item;

typedef enum EventKind {
	EVENT_none,
	EVENT_said_to,
	EVENT_stopped_talking,
} EventKind;

// these are the structs as presented to the AI, without a dependence on game data.

typedef struct ItemInSituation {
	TextChunk name;
	TextChunk description; // might include some state, e.g 'the beer was drank 5 times'
	ItemKind actual_item_kind; // used to map back to gameplay items, sometimes might be invalid item if that's not the purpose of the item in a situation
} ItemInSituation;


typedef struct CharacterPerception {
	Health health;
	TextChunk name;
	ItemInSituation held_item;
} CharacterPerception;

typedef struct ScenePerception {
	BUFF(CharacterPerception, 10) characters;
} ScenePerception;

typedef struct CharacterStatus {
	Item held_item;
	Health health;
} CharacterStatus;

typedef enum TargetKind {
	TARGET_invalid,
	TARGET_person,
} TargetKind;

typedef struct Target {
	TextChunk name;
	TextChunk description;
	TargetKind kind;
} SituationTarget;

typedef struct ArgumentSpecification {
	TextChunk name;
	TextChunk description;
} ArgumentSpecification;

#define MAX_ARGS 3

typedef struct SituationAction {
	TextChunk name;
	TextChunk description;
	BUFF(ArgumentSpecification, MAX_ARGS) args;
} SituationAction;

// the situation for somebody
typedef struct CharacterSituation {
	TextChunk room_description;
	BUFF(TextChunk, 5) events; // events that this character has observed in the plain english form
	BUFF(SituationTarget, 10) targets;
	BUFF(SituationAction, 10) actions;
} CharacterSituation;

typedef BUFF(TextChunk, MAX_ARGS) ArgList;

typedef struct Response {
	// both of these indices correspond to what was provided in the CharacterSituation
	TextChunk action;
	ArgList arguments;
} Response;

#define MAX_ACTIONS 5
typedef BUFF(Response, MAX_ACTIONS) FullResponse; // what the AI is allowed to output 

typedef struct ParsedResponse {
	SituationAction *taken;
	ArgList arguments;
} ParsedResponse;

typedef struct ParsedFullResponse {
	BUFF(ParsedResponse, MAX_ACTIONS) parsed;
} ParsedFullResponse;

CharacterSituation situation_0 = {
	.room_description = TextChunkLitC("A lush forest, steeped in shade. Some mysterious gears are scattered across the floor"),
	.events.cur_index = 2,
	.events.data = {
		TextChunkLitC("The player approached you"),
		TextChunkLitC("The player said to you, 'What's up fool?'"),
	},
	.targets.cur_index = 1,
	.targets.data = {
		{
			.name = TextChunkLitC("The Player"),
			.description = TextChunkLitC("The Player. They just spawned in out of nowhere, and are causing a bit of a ruckus."),
			.kind = TARGET_person,
		},
	},
	.actions.cur_index = 2,
	.actions.data = {
		{
			.name = TextChunkLitC("none"),
			.description = TextChunkLitC("Do nothing"),
		},
		{
			.name = TextChunkLitC("say_to"),
			.description = TextChunkLitC("Say something to the target"),
			.args.cur_index = 2,
			.args.data = {
				{
					.name = TextChunkLitC("target"),
					.description = TextChunkLitC("The target of your speech. Must be a valid target specified earlier, must match exactly that target")
				},
				{
					.name = TextChunkLitC("speech"),
					.description = TextChunkLitC("The content of your speech, is a string that's whatever you want it to be."),
				},
			},
		},
	},
};

/*
Training samples must remain stable as the game is changed, is the decision here: i.e, if the characters
in the situations are edited/changed, the training samples KEEP the old characters. This is so custom characters
don't become invalid when the game is updated. I'm making the same decision with the items, instead of storing
an across-update-stable 'item ID', I want to store the name of the item. All items can be used and have the same 'API'
so no need of updating there is necessary.

I.E: the situation is freestanding and doesn't refer to any other data. Not NpcKind, not ItemKind, not anything.
Even the list of available actions and their arguments are stored in the situation. It's basically like pure JSON data.

Also, you can't deserialize training samples easily because they exact text, and such a thing should NEVER happen.
Training sample into gamestate = bad time. For things like recording gamestate for replays, there's a real man serialization
codepath into binary.
*/
typedef struct TrainingSample {
	CharacterSituation situation;
	FullResponse response; // one or more actions
} TrainingSample;

typedef struct Npc {
	TextChunk name;
	NpcKind kind; // must not be 0, that's nobody! This isn't edited by the player, just used to uniquely identify the character
	TextChunk description;
	BUFF(TrainingSample, 4) soul;
} Npc;

Npc *get_npc(Arena *arena) {
	Npc *ret = PushArrayZero(arena, Npc, 1);
	ret->name = TextChunkLit("Bill");
	ret->kind = 1;
	ret->description = TextChunkLit("You have an intense drinking problem, and you talk like Theo Von, often saying offensive statements accidentally. You bumble around and will do anything for more whiskey. Often you go on bizarre, weird tangents and stories, that seem to just barely make sense. Your storied background has no rival: growing up adventurous and poor has given you texture and character. You have zero self awareness, and never directly state to anybody why you do things with a clear explanation, it's always cryptic, bizarre, and out of pocket.");
	BUFF_APPEND(&ret->soul,
		((TrainingSample){
			.situation = situation_0,
			.response.cur_index = 1,
			.response.data = {
				{
					.action = TextChunkLitC("say_to"),
					.arguments.cur_index = 2,
					.arguments.data[0] = TextChunkLitC("The Player"),
					.arguments.data[1] = TextChunkLitC("Don't *hic* 'fool' me partner. I'm jus-*hic* tryna' relax over here."),
				},
			},
		})
	);
	return ret;
}

typedef struct EditorState {
	bool enabled;
	u64 current_roomid;
	Vec2 camera_panning_target;
	Vec2 camera_panning;
	NpcKind placing_npc;
	NpcKind editing_npc;
	bool editing_dialog_open; // this is so while it's animated being closed the editing_npc data is still there

	bool placing_spawn;
	u64 player_spawn_roomid;
	Vec2 player_spawn_position;
} EditorState;

typedef struct GameState {
	Arena *arena; // all allocations done with the lifecycle of a gamestate (loading/reloading entire levels essentially) must be allocated on this arena.
	uint64_t tick;
	bool won;
	
	EditorState edit;
	
	bool finished_reading_dying_dialog;

	double time; // in seconds, fraction of length of day

	// processing may still occur after time has stopped on the gamestate, 
	bool stopped_time;
	BUFF(Npc, 10) characters;

	// these must point entities in its own array.
	u64 current_roomid;
	Entity entities[MAX_ENTITIES];
	rnd_gamerand_t random;
} GameState;

#define ENTITIES_ITER(ents) for (Entity *it = ents; it < ents + ARRLEN(ents); it++) if (it->exists && !it->destroy && it->generation > 0)



Npc nobody_data = { 
	.name = TextChunkLitC("Nobody"),
	.kind = NPC_nobody,
};
Npc player_data = {
	.name = TextChunkLitC("The Player"),
	.kind = NPC_player,
};
Npc *npc_data_by_name(GameState *gs, String8 name) {
	BUFF_ITER(Npc, &gs->characters) {
		if(S8Match(TextChunkString8(it->name), name, 0)) {
			return it;
		}
	}
	return 0;
}
Npc *npc_data(GameState *gs, NpcKind kind) {
	if(kind == NPC_nobody) return &nobody_data;
	if(kind == NPC_player) return &player_data;
	BUFF_ITER(Npc, &gs->characters) {
		if(it->kind == kind) {
			return it;
		}
	}
	return &nobody_data;
}
NpcKind get_next_kind(GameState *gs) {
	NpcKind max_found = 0;
	BUFF_ITER(Npc, &gs->characters) {
		assert(it->kind != NPC_nobody);
		assert(it->kind != NPC_player);
		if(it->kind > max_found) max_found = it->kind;
	}
	return max(NPC_player + 1, max_found + 1);
}

// to fix initializer is not constant
#define S8LitC(s)        {(u8 *)(s), sizeof(s)-1}

String8 npc_to_human_readable(GameState *gs, Entity *me, NpcKind kind)
{
	if(me->npc_kind == kind)
	{
		return S8Lit("You");
	}
	else
	{
		return TextChunkString8(npc_data(gs, kind)->name);
	}
}

// returns ai understandable, human readable name, on the arena, so not the enum name
String8 action_argument_string(Arena *arena, GameState *gs, ActionArgument arg)
{
	switch(arg.kind)
	{
		case ARG_CHARACTER:
		return FmtWithLint(arena, "%.*s", TextChunkVArg(npc_data(gs, arg.targeting)->name));
		break;
	}
	return (String8){0};
}


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
	bool has_shotgun = false;
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

typedef enum
{
	MSG_SYSTEM,
	MSG_USER,
	MSG_ASSISTANT,
} MessageType;

// for no trailing comma just trim the last character
String8 make_json_node(Arena *arena, MessageType type, String8 content)
{
	ArenaTemp scratch = GetScratch(&arena, 1);

	const char *type_str = 0;
	if (type == MSG_SYSTEM)
		type_str = "system";
	else if (type == MSG_USER)
		type_str = "user";
	else if (type == MSG_ASSISTANT)
		type_str = "assistant";
	assert(type_str);

	String8 escaped = escape_for_json(scratch.arena, content);
	String8 to_return = FmtWithLint(arena, "{\"type\": \"%s\", \"content\": \"%.*s\"},", type_str, S8VArg(escaped));
	ReleaseScratch(scratch);

	return to_return;
}


String8List dump_memory_as_json(Arena *arena, GameState *gs, Memory *it)
{
	ArenaTemp scratch = GetScratch(&arena, 1);
	String8List current_list = {0};
	#define AddFmt(...) PushWithLint(arena, &current_list, __VA_ARGS__)

	AddFmt("{");
	AddFmt("\"speech\":\"%.*s\",", TextChunkVArg(it->action.speech));
	AddFmt("\"action\":\"%s\",", actions[it->action.kind].name);
	String8 arg_str = action_argument_string(scratch.arena, gs, it->action.argument);
	AddFmt("\"action.argument\":\"%.*s\",", S8VArg(arg_str));
	AddFmt("\"target\":\"%.*s\"}", TextChunkVArg(npc_data(gs, it->action.talking_to_kind)->name));

 #undef AddFmt
	ReleaseScratch(scratch);
	return current_list;
}

String8List memory_description(Arena *arena, GameState *gs, Entity *e, Memory *it)
{
	String8List current_list = {0};
	#define AddFmt(...) PushWithLint(arena, &current_list, __VA_ARGS__)
	// dump a human understandable sentence description of what happened in this memory
	bool no_longer_wants_to_converse = false; // add the no longer wants to converse text after any speech, it makes more sense reading it
	
#define HUMAN(kind) S8VArg(npc_to_human_readable(gs, e, kind))
	if (it->action.kind != ACT_none)
	{
		switch (it->action.kind)
		{
		case ACT_none:
			break;
		case ACT_join:
			AddFmt("%.*s joined %.*s\n", HUMAN(it->context.author_npc_kind), HUMAN(it->action.argument.targeting));
			break;
		case ACT_leave:
			AddFmt("%.*s left their party\n", HUMAN(it->context.author_npc_kind));
			break;
		case ACT_aim_shotgun:
			AddFmt("%.*s aimed their shotgun at %.*s\n", HUMAN(it->context.author_npc_kind), HUMAN(it->action.argument.targeting));
			break;
		case ACT_fire_shotgun:
			AddFmt("%.*s fired their shotgun at %.*s, brutally murdering them.\n", HUMAN(it->context.author_npc_kind), HUMAN(it->action.argument.targeting));
			break;
		case ACT_put_shotgun_away:
			AddFmt("%.*s holstered their shotgun, no longer threatening anybody\n", HUMAN(it->context.author_npc_kind));
			break;
		case ACT_approach:
			AddFmt("%.*s approached %.*s\n", HUMAN(it->context.author_npc_kind), HUMAN(it->action.argument.targeting));
			break;
		case ACT_end_conversation:
			no_longer_wants_to_converse = true;
			break;
		case ACT_assign_gameplay_objective:
			AddFmt("%.*s assigned a definitive game objective to %.*s\n", HUMAN(it->context.author_npc_kind), HUMAN(it->action.talking_to_kind));
			break;
		case ACT_award_victory:
			AddFmt("%.*s awarded victory to %.*s\n", HUMAN(it->context.author_npc_kind), HUMAN(it->action.talking_to_kind));
			break;
		}
	}
	if (it->action.speech.text_length > 0)
	{
		String8 target_string = S8Lit("the world");
		if (it->action.talking_to_kind != NPC_nobody)
		{
			if (it->action.talking_to_kind == e->npc_kind)
				target_string = S8Lit("you");
			else
				target_string = TextChunkString8(npc_data(gs, it->action.talking_to_kind)->name);
		}

		if(!e->is_world)
		{
			if(it->action.talking_to_kind == e->npc_kind)
			{
				AddFmt("(Speaking directly you) ");
			}
			else
			{
				AddFmt("(Overheard conversation, they aren't speaking directly to you) ");
			}
		}
		AddFmt("%.*s said \"%.*s\" to %.*s", TextChunkVArg(npc_data(gs, it->context.author_npc_kind)->name), TextChunkVArg(it->action.speech), S8VArg(target_string));
		if(!e->is_world)
		{
			// AddFmt(" (you are %.*s)", TextChunkVArg(npc_data(gs, e->npc_kind)->name));
		}
		AddFmt("\n");
	}

	if (no_longer_wants_to_converse)
	{
		if (it->action.argument.targeting == NPC_nobody)
		{
			AddFmt("%.*s no longer wants to converse with everybody\n", HUMAN(it->context.author_npc_kind));
		}
		else
		{
			AddFmt("%.*s no longer wants to converse with %.*s\n", HUMAN(it->context.author_npc_kind), HUMAN(it->action.argument.targeting));
		}
	}
#undef HUMAN
#undef AddFmt
	return current_list;
}

String8List describe_situation(Arena *arena, CharacterSituation *situation) {
	String8List ret = {0};

	#define AddFmt(...) PushWithLint(arena, &ret, __VA_ARGS__)

	AddFmt("Where you're standing: %.*s\n\n", TextChunkVArg(situation->room_description));

	AddFmt("The targets you can provide as arguments to your actions:\n");
	BUFF_ITER(SituationTarget, &situation->targets) {
		AddFmt("%.*s - %.*s\n", TextChunkVArg(it->name), TextChunkVArg(it->description));
	}
	AddFmt("\n");

	AddFmt("The actions you can take, and the arguments they expect. Actions are called like `ACTION(\"Arg 1\", \"Arg 2\")`\n");
	BUFF_ITER(SituationAction, &situation->actions) {
		AddFmt("%.*s - %.*s - ", TextChunkVArg(it->name), TextChunkVArg(it->description));
		SituationAction *action = it;
		BUFF_ITER_I(ArgumentSpecification, &action->args, i) {
			AddFmt("(Argument %d: %.*s, %.*s), ", i, TextChunkVArg(it->name), TextChunkVArg(it->description));
		}
		AddFmt("\n");
	}
	AddFmt("\n");

	AddFmt("Previous events you need to respond to:\n");
	BUFF_ITER(TextChunk, &situation->events) {
		AddFmt("%.*s\n", TextChunkVArg(*it));
	}

	#undef AddFmt
	return ret;
}

// Parses something like this: `action_one("arg 1", "arg 2"), action_two("arg 3", "arg 4")`
ParsedFullResponse *parse_output(Arena *arena, CharacterSituation *situation, String8 raw_output, String8 *error_out) {
	#define error(...) *error_out = S8Fmt(arena, __VA_ARGS__)

	ParsedFullResponse *ret = PushArrayZero(arena, ParsedFullResponse, 1);
	String8 output = S8SkipWhitespace(S8ChopWhitespace(raw_output));

	ArenaTemp scratch = GetScratch(&arena, 1);

	typedef enum {
		ST_invalid,
		ST_action,
		ST_in_args,
		ST_in_string,
	} State;

	State stat = ST_action;

	typedef struct ParsedActionCall {
		struct ParsedActionCall *next;
		String8 action_name;
		BUFF(String8, MAX_ARGS) arguments;
	} ParsedActionCall;

	ParsedActionCall *parsed = 0;

	ParsedActionCall cur_parsed = {0};
	u64 cur = 0;
	while(cur < output.size && error_out->size == 0) {
		switch(stat) {
			case ST_invalid:
			assert(false);
			break;

			case ST_action:
			{
				if(output.str[cur] == ' ' || output.str[cur] == ',') {
					cur = cur + 1;
				} else {
					u64 args = S8FindSubstring(output, S8Lit("("), cur, 0);
					if(args == output.size) {
						error("Expected a '(' after action '%.*s'", S8VArg(S8Substring(output, cur, output.size)));
					} else {
						cur_parsed.action_name = S8Substring(output, cur, args);
						stat = ST_in_args;
						cur = args + 1;
					}
				}
			}
			break;

			case ST_in_args:
			{
				if(output.str[cur] == ')') {
					ParsedActionCall *newly_parsed = PushArrayZero(scratch.arena, ParsedActionCall, 1);
					*newly_parsed = cur_parsed;
					StackPush(parsed, newly_parsed);
					cur_parsed = (ParsedActionCall){0};
					cur = cur + 1;
					stat = ST_action;
				} else {
					if(output.str[cur] == '"') {
						BUFF_APPEND(&cur_parsed.arguments, S8(output.str + cur + 1, 0));
						stat = ST_in_string;
						cur = cur + 1;
						else if(output.str[cur] == ',' || output.str[cur] == ' ') {
							cur = cur + 1;
						} else {
							error("Expected a `\"` to mark the beginning of a string in an action");
						}
					}
				}
			}
			break;

			case ST_in_string:
			{
				assert(cur_parsed.arguments.cur_index > 0);
				u64 end = S8FindSubstring(output, S8Lit("\""), cur, 0);
				String *outputting = &cur_parsed.arguments.data[cur_parsed.arguments.cur_index - 1];
				outputting->size = end - (outputting->str - output.str);
				cur = end + 1;
				stat = ST_in_args;
			}
			break;

			default:
			assert(false);
			break;
		}
	}
	if(stat != ST_action) {
		// output ended before unfinished business
		error("Unclosed `(` or `\"` detected. Not allowed!");
	}

	if(error_out->size == 0) {
		// convert into the parsed response, potentially erroring
		for(ParsedActionCall *cur = parsed; cur->next; cur = cur->next) {
			if(error_out.size != 0) break;

			ParsedResponse newly_parsed = {0};

			SituationAction *found = 0;
			BUFF_ITER(SituationAction, &situation->actions) {
				if(S8Match(cur->action_name, it->name, 0)) {
					found = it;
				}
			}
			if(!found) {
				error("Couldn't find action of name '%.*s'", S8VArg(cur->action_name));
			}

			if(error_out.size == 0) {
				newly_parsed.taken = found;
				if(cur->arguments.cur_index != found->args.cur_index) {
					error("For action '%.*s', expected %llu arguments but found %llu", found->args.cur_index, cur->arguments.cur_index);
				} else {
					newly_parsed.arguments = cur->arguments;
				}
			}

			if(error_out.size == 0) {
				BUFF_APPEND(&ret->parsed, newly_parsed);
			}
		}
	}

	ReleaseScratch(scratch);

	#undef error
	return ret;
}

// outputs json which is parsed by the server
String8 generate_chatgpt_prompt(Arena *arena, Npc *personality, CharacterSituation *situation)
{
	ArenaTemp scratch = GetScratch(&arena, 1);

	String8List list = {0};

	PushWithLint(scratch.arena, &list, "[");

	#define AddFmt(...) PushWithLint(scratch.arena, &current_list, __VA_ARGS__)
	#define AddNewNode(node_type) { S8ListPush(scratch.arena, &list, make_json_node(scratch.arena, node_type, S8ListJoin(scratch.arena, current_list, &(StringJoin){0}))); current_list = (String8List){0}; }
	
	// make first system node
	{
		String8List current_list = {0};
		AddFmt("%s\n\n", global_prompt);
		AddFmt("You are acting as %.*s. %.*s\n", TextChunkVArg(personality->name), TextChunkVArg(personality->description));
		AddNewNode(MSG_SYSTEM);
	}

	// add previous samples
	BUFF_ITER(TrainingSample, &personality->soul) {
		{
			String8List current_list = {0};
			String8List described = describe_situation(scratch.arena, &it->situation);
			S8ListConcat(&current_list, &described);
			AddNewNode(MSG_USER);
		}
		{
			String8List current_list = {0};
			FullResponse *sample_response = &it->response;
			BUFF_ITER_I(Response, sample_response, i) {
				AddFmt("%.*s(", TextChunkVArg(it->action));
				Response *resp = it;
				BUFF_ITER_I(TextChunk, &resp->arguments, i) {
					AddFmt("\"%.*s\"", TextChunkVArg(*it));
					if(i < resp->arguments.cur_index - 1) AddFmt(", ");
				}
				AddFmt(")");
				if(i < sample_response->cur_index - 1) AddFmt(", ");
			}
			AddNewNode(MSG_ASSISTANT);
		}
	}

   // add current situation
	{
		String8List current_list = {0};
		String8List described = describe_situation(scratch.arena, situation);
		S8ListConcat(&current_list, &described);
		AddNewNode(MSG_USER);
	}

	String8 with_trailing_comma = S8ListJoin(scratch.arena, list, &(StringJoin){S8Lit(""),S8Lit(""),S8Lit(""),});
	String8 no_trailing_comma = S8Chop(with_trailing_comma, 1);

	String8 to_return = S8Fmt(arena, "%.*s]", S8VArg(no_trailing_comma));

	ReleaseScratch(scratch);

 #undef AddFmt
 #undef AddNewNode
	return to_return;
	/*
	assert(e->is_npc);
	assert(npc_data(gs, e->npc_kind) != 0);

	ArenaTemp scratch = GetScratch(&arena, 1);

	String8List list = {0};

	PushWithLint(scratch.arena, &list, "[");

	#define AddFmt(...) PushWithLint(scratch.arena, &current_list, __VA_ARGS__)
	#define AddNewNode(node_type) { S8ListPush(scratch.arena, &list, make_json_node(scratch.arena, node_type, S8ListJoin(scratch.arena, current_list, &(StringJoin){0}))); current_list = (String8List){0}; }
	

	// make first system node
	{
		String8List current_list = {0};
		AddFmt("%s\n\n", global_prompt);
		// AddFmt("%.*s\n\n", TextChunkVArg(npc_data(gs, e->npc_kind)->prompt));
		AddFmt("The characters who are near you, that you can target:\n");
		BUFF_ITER(Entity*, &can_talk_to)
		{
			assert((*it)->is_npc);
			String8 info = S8Lit("");
			if((*it)->killed)
			{
				info = S8Lit(" - they're currently dead, they were murdered");
			}
			AddFmt("%.*s%.*s\n", TextChunkVArg(npc_data(gs, (*it)->npc_kind)->name), S8VArg(info));
		}
		AddFmt("\n");

		// @TODO unhardcode this, this will be a description of where the character is right now
		//AddFmt("You're currently standing in Daniel's farm's barn, a run-down structure that barely serves its purpose. Daniel's mighty protective of it though.\n");
		AddFmt("You and everybody you're talking to is in a small sparse forest near Daniel's farm. There are some mysterious mechanical parts strewn about on the floor that Daniel seems relunctant and fearful to talk about.\n");

		AddFmt("\n");

		AddFmt("The actions you can perform, what they do, and the arguments they expect:\n");
		AvailableActions can_perform;
		fill_available_actions(gs, e, &can_perform);
		BUFF_ITER(ActionKind, &can_perform)
		{
			AddFmt("%s - %s - %s\n", actions[*it].name, actions[*it].description, actions[*it].argument_description);
		}

		AddNewNode(MSG_SYSTEM);
	}

	String8List current_list = {0};
	for(Memory *it = e->memories_first; it; it = it->next)
	{
		// going through memories, I'm going to accumulate human understandable sentences for what happened in current_list.
		// when I see an 'i_said_this' memory, that means I flush. and add a new assistant node.

		// write a new human understandable sentence or two to current_list
		if (!it->context.i_said_this) {
			String8List desc_list = memory_description(scratch.arena, gs, e, it);
			S8ListConcat(&current_list, &desc_list);
		}

		// if I said this, or it's the last memory, flush the current list as a user node
		if(it->context.i_said_this || it == e->memories_last)
		{
			if(it == e->memories_last && e->errorlist_first)
			{
				AddFmt("Errors you made: \n");
				for(RememberedError *cur = e->errorlist_first; cur; cur = cur->next)
				{
					if(cur->offending_self_output.action.speech.text_length > 0 || cur->offending_self_output.action.kind != ACT_none)
					{
						String8 offending_json_output = S8ListJoin(scratch.arena, dump_memory_as_json(scratch.arena, gs, &cur->offending_self_output), &(StringJoin){0});
						AddFmt("When you output, `%.*s`, ", S8VArg(offending_json_output));
					}
					AddFmt("%.*s\n", TextChunkVArg(cur->reason_why_its_bad));
				}
			}
			if(current_list.node_count > 0)
				AddNewNode(MSG_USER);
		}

		if(it->context.i_said_this)
		{
			String8List current_list = {0}; // shadow the list of human understandable sentences to quickly flush 
			current_list = dump_memory_as_json(scratch.arena, gs, it);
			AddNewNode(MSG_ASSISTANT);
		}
	}
	String8 with_trailing_comma = S8ListJoin(scratch.arena, list, &(StringJoin){S8Lit(""),S8Lit(""),S8Lit(""),});
	String8 no_trailing_comma = S8Chop(with_trailing_comma, 1);

	String8 to_return = S8Fmt(arena, "%.*s]", S8VArg(no_trailing_comma));

	ReleaseScratch(scratch);

 #undef AddFmt
	return to_return;
*/
}


String8 get_field(Node *parent, String8 name)
{
	return MD_ChildFromString(parent, name, 0)->first_child->string;
}

void parse_action_argument(Arena *error_arena, GameState *gs, String8 *cur_error_message, ActionKind action, String8 action_argument_str, ActionArgument *out)
{
	assert(cur_error_message);
	if(cur_error_message->size > 0) return;
	ArenaTemp scratch = GetScratch(&error_arena, 1);
	String8 action_str = S8CString(actions[action].name);
	// @TODO refactor into, action argument kinds and they parse into different action argument types
	bool arg_is_character = action == ACT_join || action == ACT_aim_shotgun || action == ACT_end_conversation;

	if (arg_is_character)
	{
		out->kind = ARG_CHARACTER;
		bool found_npc = false;
		Npc * npc = npc_data_by_name(gs, action_argument_str);
		found_npc = npc != 0;
		if(npc) {
			out->targeting = npc->kind;
		}
		if (!found_npc)
		{
			*cur_error_message = FmtWithLint(error_arena, "Argument for action `%.*s` you gave is `%.*s`, which doesn't exist in the game so is invalid", S8VArg(action_str), S8VArg(action_argument_str));
		}
	}
	else
	{
		assert(false); // don't know how to parse the argument string for this kind of action...
	}
	ReleaseScratch(scratch);
}

// if returned string has size greater than 0, it's the error message. Allocated
// on arena passed into it or in constant memory
String8 parse_chatgpt_response(Arena *arena, GameState *gs, Entity *e, String8 action_in_json, ActionOld *out)
{
	ArenaTemp scratch = GetScratch(&arena, 1);

	String8 error_message = {0};

	*out = (ActionOld) { 0 };

	ParseResult result = ParseWholeString(scratch.arena, S8Lit("chat_message"), action_in_json);
	if(result.errors.node_count > 0)
	{
		Message *cur = result.errors.first;
		CodeLoc loc = CodeLocFromNode(cur->node);
		error_message = FmtWithLint(arena, "Parse Error on column %d: %.*s", loc.column, S8VArg(cur->string));
	}

	Node *message_obj = result.node->first_child;

	String8 speech_str = {0};
	String8 action_str = {0};
	String8 action_argument_str = {0};
	String8 target_str = {0};
	if(error_message.size == 0)
	{
		speech_str = get_field(message_obj, S8Lit("speech"));
		action_str = get_field(message_obj, S8Lit("action"));
		action_argument_str = get_field(message_obj, S8Lit("action.argument"));
		target_str = get_field(message_obj, S8Lit("target"));
	}
	if(error_message.size == 0 && action_str.size == 0)
	{
		error_message = S8Lit("The field `action` must be of nonzero length, if you don't want to do anything it should be `none`");
	}
	if(error_message.size == 0 && action_str.size == 0)
	{
		error_message = S8Lit("The field `target` must be of nonzero length, if you don't want to target anybody it should be `nobody`");
	}	if(error_message.size == 0 && speech_str.size >= MAX_SENTENCE_LENGTH)
	{
		error_message = FmtWithLint(arena, "Speech string provided is too big, maximum bytes is %d", MAX_SENTENCE_LENGTH);
	}
	assert(e->npc_kind != NPC_player); // player can't perform AI actions?

	if(error_message.size == 0)
	{
		if(S8Match(target_str, S8Lit("nobody"), 0))
		{
			out->talking_to_kind = NPC_nobody;
		}
		else
		{
			Npc * npc = npc_data_by_name(gs, target_str);
			bool found = false;
			if(npc) {
				found = true;
				out->talking_to_kind = npc->kind;
			}
			if(!found)
			{
				error_message = FmtWithLint(arena, "Unrecognized character provided in field 'target': `%.*s`", S8VArg(target_str));
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
			if(S8Match(S8CString(actions[i].name), action_str, 0))
			{
				assert(!found_action);
				found_action = true;
				out->kind = i;
			}
		}
		if(!found_action)
		{
			error_message = FmtWithLint(arena, "Action `%.*s` is invalid, doesn't exist in the game", S8VArg(action_str));
		}

		if(error_message.size == 0)
		{
			if(actions[out->kind].takes_argument)
			{
				parse_action_argument(arena, gs, &error_message, out->kind, action_argument_str, &out->argument);
			}
		}
	}

	ReleaseScratch(scratch);
	return error_message;
}