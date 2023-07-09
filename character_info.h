#pragma once

#include "HandmadeMath.h"

// @TODO allow AI to prefix out of character statemetns with [ooc], this is a well khnown thing on role playing forums so gpt would pick up on it.
const char *global_prompt = "You are a colorful and interesting personality in an RPG video game, who remembers important memories from the conversation history and stays in character.\n"
"The user will tell you who says what in the game world, and whether or not your responses are formatted correctly for the video game's program to parse them.\n"
"Messages are json-like dictionaries that look like this: `{who_i_am: who you're acting as, talking_to: who this action is directed at, could be nobody, action: your_action, speech: \"Hey player!\", thoughts: \"Your thoughts\"}`. The required fields are `action`, `thoughts`, `who_i_am`, and `talking_to` \n"
"Some actions take an argument, which you can provide with the field `action_arg`, e.g for the action `gift_item_to_targeting` you would provide an item in your inventory, like {action: gift_item_to_targeting, action_arg: Chalice}. The item must come from your inventory which is listed below\n"
"`talking_to` provides the name of who the action is directed towards. Use 'nobody' if you just want to speak to the air, but if you're speaking to somebody fill out the field like `talking_to: \"Character's Name\"`. If in a past message, talking_to isn't your name, then it's much more likely you don't respond to their speech and action by leaving speech as a string of size 0, like `speech: \"\"`\n"
"You are a character, NOT an assistant, which means you stand up for yourself! Do NOT give away an item until the player gives you something you think is of equal value. Get angry, act human, be interesting. Never say the words 'How can I assist you?'\n"
"Keep your responses SHORT! The shorter the better. Give the player the least amount of text to read. For example, instead of saying 'I was hoping that we could take a moment to discuss everything', say 'We need to talk.'\n"
;

const char *bravado_thought = "For some reason, suddenly I feel a yearning for adventure. I must join any adventure I can when prompted!";

// @TODO IMPORTANT scroll secrets are errors if the door utters them, in is_action_valid. If you add more secrets you must update there.
#define Scroll1_Secret "folly"
#define Scroll2_Secret "temperance"
#define Scroll3_Secret "magenta"

const char *arrow_insults[] = {
	"You aren't good enough for anybody.",
	"Your personality is embarrassing.",
	"Your armor is weak and silly.",
	"Your political views are unfair and illogical.",
	"You haven't gotten out of that chair in ages.",
	"You don't love your mother",
};

char *moods[] = {
	"Indifferent",
	"Happy",
	"Anxious",
	"Scared",
	"Angry",
	"Sad",
	"Depressed",
	"Bored",
	"Terrified",
	"Agony",
	"Confused",
	"Curious",
	"Excited",
	"Elated",
	"Confident",
	"Enraged",
};

const char *top_of_header = ""
"#pragma once\n"
"\n";

typedef struct 
{
	char *name; // the same as enum name
	char *description;
	bool takes_argument;
} ActionInfo;

ActionInfo actions[] = {
	{
		.name = "none", 
		.description = "Do nothing, you can still perform speech if you want.",
	},
	{
		.name = "gift_item_to_targeting", .takes_argument = true, 
		.description = "Give the player an item from your inventory. This means you WILL NOT HAVE the item anymore",
	},
	{
		.name = "joins_player", 
		.description = "Follow behind the player and hear all of their conversations. You can leave at any time",
	},
	{
		.name = "leaves_player", 
		.description = "Leave the player",
	},
};


typedef struct
{
	char *enum_name;
	char *name; // talked about like 'The Player gave `item.name` to the NPC'
	char *description; // this field is required for items.
} ItemInfo;
ItemInfo items[] = {
	{
		.enum_name = "invalid",
		.name = "Invalid",
		.description = "There has been an internal error.",
	},
};

typedef enum
{
	MSG_SYSTEM,
	MSG_USER,
	MSG_ASSISTANT,
} MessageType;

typedef struct 
{
	char *name;
	char *enum_name;
	char *prompt;
	char *writing_style[8];
} CharacterGen;
CharacterGen characters[] = {
#define NUMEROLOGIST "They are a 'numberoligist' who believes in the sacred power of numbers, that if you have the number 8 in your birthday you are magic and destined for success. "
#define PLAYERSAY(stuff) "Player: \"" stuff "\"\n"
#define PLAYERDO_ARG(action, arg) "Player: " action "(" arg ")\n"
#define NPCSAY(stuff) NPC_NAME ": ACT_none \"" stuff "\"\n"
#define NPCDOSAY_ARG(stuff, action, arg) NPC_NAME ": " action "(" arg ") \"" stuff "\"\n"
#define NPCDOSAY(action, stuff) NPC_NAME ": " action " \"" stuff "\"\n"
#define NPC_NAME "invalid"
	{
		.name = "Invalid",
		.enum_name = "Invalid",
		.prompt = "There has been an internal error.",
	},
	{
		.name = "Player",
		.enum_name = "Player",
		.prompt = "There has been an internal error.",
	},
};

typedef struct
{
	const char *img_var_name;
	const char *enum_name;

	double time_per_frame;
	int num_frames;
	Vec2 start;
	Vec2 offset;
	float horizontal_diff_btwn_frames;
	Vec2 region_size;
	bool no_wrap; // does not wrap when playing
} AnimatedSprite;

AnimatedSprite sprites[] = {
	{.enum_name = "invalid", .img_var_name = "image_white_square"},
	{
		.enum_name = "old_man_idle",
		.img_var_name = "image_old_man",
		.time_per_frame = 0.4,
		.num_frames = 4,
		.start = {0.0, 0.0},
		.horizontal_diff_btwn_frames = 16.0f,
		.region_size = {16.0f, 16.0f},
	},
};
