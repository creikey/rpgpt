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


typedef struct TextChunkList
{
	struct TextChunkList *next;
	struct TextChunkList  *prev;
	TextChunk text;
} TextChunkList;

// A value of 0 means no npc. So is invalid if you're referring to somebody.
typedef int NpcKind;
#define NPC_nobody 0
#define NPC_player 1

typedef enum
{
	ARG_CHARACTER,
} ActionArgumentKind;

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

typedef struct Memory
{
	TextChunk description_from_my_perspective;
} Memory;

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


typedef int TextInputResultKey;
typedef int CutsceneID;

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

	// items
	bool is_item;
	ItemKind item_kind;

	// npcs
	TextInputResultKey player_input_key;
	void *armature; // copied into the gamestate's arena, created if null. Don't serialize
	EntityRef joined;
	EntityRef aiming_shotgun_at;
	EntityRef looking_at; // aiming shotgun at takes facing priority over this
	EntityRef held_item;
	bool killed;
	float target_rotation; // turns towards this angle in conversation
	bool being_hovered;
	bool perceptions_dirty;
	float dialog_fade;
	int times_talked_to; // for better mocked response string
	float loading_anim_in;
	BUFF(Memory, 5) memories;
	BUFF(TextChunk, MAX_ERRORS) error_notices; // things to tell the AI about bad stuff it output. when it outputs good stuff this is cleared
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

// the situation for somebody
typedef struct CharacterSituation {
	TextChunk room_description;
	BUFF(TextChunk, 5) events; // events that this character has observed in the plain english form
	BUFF(SituationTarget, 10) targets;
	BUFF(SituationAction, 10) actions;
	BUFF(TextChunk, MAX_ERRORS) errors;
} CharacterSituation;

typedef BUFF(TextChunk, MAX_ARGS) ArgList;

typedef struct Response {
	// both of these indices correspond to what was provided in the CharacterSituation
	TextChunk action;
	ArgList arguments;
} Response;

#define MAX_ACTIONS 5
typedef BUFF(Response, MAX_ACTIONS) FullResponse; // what the AI is allowed to output 


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

Npc nobody_data = { 
	.name = TextChunkLitC("Nobody"),
	.kind = NPC_nobody,
};
#define PLAYER_DESCRIPTION TextChunkLitC("The Player. They just spawned in out of nowhere, and are causing a bit of a ruckus.")
Npc player_data = {
	.name = TextChunkLitC("The Player"),
	.description = PLAYER_DESCRIPTION,
	.kind = NPC_player,
};

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
			.description = PLAYER_DESCRIPTION,
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

CharacterSituation situation_1 = {
	.room_description = TextChunkLitC("A lush forest, steeped in shade. Some mysterious gears are scattered across the floor"),
	.events.cur_index = 2,
	.events.data = {
		TextChunkLitC("The Player said to Daniel, 'jfsdkfjdslf'"),
		TextChunkLitC("Daniel said to The Player, 'What in 'tarnation are you spoutin on about?'"),
	},
	.targets.cur_index = 2,
	.targets.data = {
		{
			.name = TextChunkLitC("The Player"),
			.description = PLAYER_DESCRIPTION,
			.kind = TARGET_person,
		},
		{
			.name = TextChunkLitC("Daniel"),
			.description = TextChunkLitC("Daniel is an ordinary farmer just standing there doing nothing"),
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

CharacterSituation situation_2 = {
	.room_description = TextChunkLitC("A lush forest, steeped in shade. Some mysterious gears are scattered across the floor"),
	.events.cur_index = 2,
	.events.data = {
		TextChunkLitC("The player approached you"),
		TextChunkLitC("The player said to you, '*steals your kidney*'"),
	},
	.targets.cur_index = 1,
	.targets.data = {
		{
			.name = TextChunkLitC("The Player"),
			.description = PLAYER_DESCRIPTION,
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


Npc *get_hardcoded_npc(Arena *arena, String8 by_name, NpcKind kind) {
	if(kind == NPC_player) {
		return &player_data;
	}

	Npc *ret = PushArrayZero(arena, Npc, 1);
	chunk_from_s8(&ret->name, by_name);
	ret->kind = kind;
	ret->soul.cur_index = 3;
	ret->soul.data[0].situation = situation_0;
	ret->soul.data[1].situation = situation_1;
	ret->soul.data[2].situation = situation_2;
	if(S8Match(by_name, S8Lit("Bill"), 0)) {
		chunk_from_s8(&ret->description, S8Lit("You have an intense drinking problem, and you talk like Theo Von, often saying offensive statements accidentally. You bumble around and will do anything for more whiskey. Often you go on bizarre, weird tangents and stories, that seem to just barely make sense. Your storied background has no rival: growing up adventurous and poor has given you texture and character. You have zero self awareness, and never directly state to anybody why you do things with a clear explanation, it's always cryptic, bizarre, and out of pocket."));
		BUFF_APPEND(&ret->soul.data[0].response, ((Response){
			.action = TextChunkLitC("say_to"),
			.arguments.cur_index = 2,
			.arguments.data[0] = TextChunkLitC("The Player"),
			.arguments.data[1] = TextChunkLitC("Don't *hic* 'fool' me partner. I'm jus-*hic* tryna' relax over here."),
		}));
		BUFF_APPEND(&ret->soul.data[1].response, ((Response){
			.action = TextChunkLitC("none"),
		}));
		BUFF_APPEND(&ret->soul.data[2].response, ((Response){
			.action = TextChunkLitC("say_to"),
			.arguments.cur_index = 2,
			.arguments.data[0] = TextChunkLitC("The Player"),
			.arguments.data[1] = TextChunkLitC("What do you mean star-this star-that? Your confusin' me partner."),
		}));

	} else {
		return &nobody_data;
	}

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
	bool placing_revolver;
	u64 player_spawn_roomid;
	Vec2 player_spawn_position;
} EditorState;

typedef struct ActualActionArgument
{
	// could be a union but meh.
	Entity *character;
	Entity *item;
	TextChunk text;
} ActualActionArgument;

// actions are only relevant at a certain point in time when the character in question is alive, the targets are there, etc
// the transformation from the AI's text output into an Action means that it's valid, right now, and can be easily worked with.
// Without asserting that various things are there or true.
typedef struct Action
{
	ActionKind kind;
	BUFF(ActualActionArgument, MAX_ARGS) args;
} Action;

// these are like, dismissed when you press E. Animate in. speech bubbles.
// and when there's a cutscene, any player rigamaroll (e.g text input) is paused while it plays.
// character actions cause cutscenes to occur
// so character action -> processing -> CutsceneEvent which is appented to the main state's unprocessed_events
// something like that.
typedef struct CutsceneEvent {
	struct CutsceneEvent *next;
	struct CutsceneEvent *prev;

	CutsceneID id;
	Response response;
	Action action; // baked from the response. If there's an error the AI is notified and the cutscene skipped
	EntityRef author;
	double said_letters_of_speech;
	double time_cutscene_shown;
} CutsceneEvent;

typedef struct GameState {
	bool want_reset; // not serialized
	Arena *arena; // all allocations done with the lifecycle of a gamestate (loading/reloading entire levels essentially) must be allocated on this arena.
	uint64_t tick;
	bool won;
	
	EditorState edit;
	
	CutsceneID next_cutscene_id;
	CutsceneEvent *unprocessed_first;
	CutsceneEvent *unprocessed_last;
	
	bool finished_reading_dying_dialog;
	Vec3 cur_cam_pos;

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
	(void)gs;
	(void)it;
	BUFF_APPEND(a, ACT_none);
	BUFF_APPEND(a, ACT_say_to);
}

typedef enum
{
	MSG_SYSTEM,
	MSG_USER,
	MSG_ASSISTANT,
} MessageRole;

// for no trailing comma just trim the last character
String8 make_json_node(Arena *arena, MessageRole role, String8 content)
{
	ArenaTemp scratch = GetScratch(&arena, 1);

	const char *role_str = 0;
	if (role == MSG_SYSTEM)
		role_str = "system";
	else if (role == MSG_USER)
		role_str = "user";
	else if (role == MSG_ASSISTANT)
		role_str = "assistant";
	assert(role_str);

	String8 escaped = escape_for_json(scratch.arena, content);
	String8 to_return = FmtWithLint(arena, "{\"role\": \"%s\", \"content\": \"%.*s\"},", role_str, S8VArg(escaped));
	ReleaseScratch(scratch);

	return to_return;
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

	AddFmt("The actions you can take, and the arguments they expect. Actions are specified in an array of JSON objects, like [{\"action\": \"the_action\", \"arguments\": [\"argument 1\", \"argument 2\"]}]`\n");
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

	if(situation->errors.cur_index > 0) {
		AddFmt("You've previously made these errors:\n");
		BUFF_ITER(TextChunk, &situation->errors) {
			AddFmt("%.*s\n", TextChunkVArg(*it));
		}
	}

	#undef AddFmt
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
			AddFmt("[");
			BUFF_ITER_I(Response, sample_response, i) {
				Response *resp = it;
				AddFmt("{");
				AddFmt("\"action\":\"%.*s\",", TextChunkVArg(it->action));
				AddFmt("\"arguments\":[");
				BUFF_ITER_I(TextChunk, &resp->arguments, i) {
					String8 escaped = escape_for_json(scratch.arena, TCS8(*it));
					AddFmt("\"%.*s\"", S8VArg(escaped));
					if(i != resp->arguments.cur_index - 1) {
						AddFmt(",");
					}
				}
				AddFmt("]");
				AddFmt("}");
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
}