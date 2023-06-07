#pragma once

#include "HandmadeMath.h"

// @TODO allow AI to prefix out of character statemetns with [ooc], this is a well khnown thing on role playing forums so gpt would pick up on it.
const char *global_prompt = "You are a colorful and interesting personality in an RPG video game, who remembers important memories from the conversation history and stays in character.\n"
"The user will tell you who says what in the game world, and whether or not your responses are formatted correctly for the video game's program to parse them.\n"
"Messages are json-like dictionaries that look like this: `{action: your_action, speech: \"Hey player!\", thoughts: \"Your thoughts\"}`. The required fields are `action` and `thoughts`\n"
"Some actions take an argument, which you can provide with the field `action_arg`, e.g for the action `give_item` you would provide an item in your inventory, like {action: give_item, action_arg: Chalice}. The item must come from your inventory which is listed below\n"
"Do NOT make up details that don't exist in the game, this is a big mistake as it confuses the player. The game is simple and small, so prefer to tell the player in character that you don't know how to do something if you aren't explicitly told the information about the game the player requests. E.g, if the player asks how to get rare metals and you don't know how, DO NOT make up something plausible like 'Go to the frost mines in the north', instead say 'I have no idea, sorry.', unless the detail about the game they're asking for is included below.\n"
;

const char *top_of_header = ""
"#pragma once\n"
"\n";

typedef struct 
{
	char *name; // the same as enum name
	bool takes_argument;
} ActionInfo;

ActionInfo actions[] = {
	{.name = "none", },

	{.name = "give_item", .takes_argument = true, },

	// mostly player actions
	{.name = "walks_up", },
	{.name = "hits_npc", },
	{.name = "leaves", },

	// mostly npc actions
	{.name = "allows_player_to_pass", },
	{.name = "gives_tripod", },
	{.name = "heals_player", },
	{.name = "fights_player", },
	{.name = "strikes_air", },
	{.name = "joins_player", },
	{.name = "leaves_player", },
	{.name = "stops_fighting_player", },
    {.name = "gives_peace_token", },

	// Actions used by jester and other characters only in 
	// the prologue for the game
	{.name = "causes_testicular_torsion", },
	{.name = "undoes_testicular_torsion", },
	{.name = "enchant_fish", },
	{.name = "steal_iron_pole", },

	{.name = "knights_player", },
};

typedef struct
{
	char *global_prompt;
	char *enum_name;
	char *name; // talked about like 'The Player gave `item.name` to the NPC'
	char *possess;
	char *discard;
} ItemInfo;
ItemInfo items[] = {
	{
		.enum_name = "none",
		.name = "Nothing",
		.global_prompt = "The player isn't holding anything",
		.possess = "The player is no longer holding anything",
		.discard = "The player is no longer holding nothing",
	},
	{
		.enum_name = "GoldCoin",
		.name = "Gold Coin",
		.global_prompt = "The player is holding a gold coin",
		.possess = "The player is now holding a gold coin",
		.discard = "The player isn't holding a gold coin anymore",
	},
	{
		.enum_name = "Chalice",
		.name = "The Chalice of Gold",
		.global_prompt = "The player is holding a glimmering Chalice of Gold. It is a beautiful object, mesmerizing in the light.",
		.possess = "The player has ascertained the beautiful chalice of gold",
		.discard = "The player no longer has the chalice of gold",
	},
	{
		.enum_name = "WhiteSquare",
		.name = "the white square",
		.global_prompt = "The player is holding a mysterious white square. It is unknown what strange and erotic abilities one has when they possess the square.",
		.possess = "The player is now holding the white square",
		.discard = "The player is no longer holding the white square.",
	},
	{
		.enum_name = "Boots",
		.name = "some boots",
		.global_prompt = "The player is holding the boots of speed. He is a force to be recogned with in this state, he has great speed while holding the boots.",
		.possess = "The player is now holding the boots of speed",
		.discard = "The player is no longer holding the boots of speed",
	},
	{
		.enum_name = "Tripod",
		.name = "the tripod",
		.global_prompt = "The player is holding a tripod used for holding things up. It's got an odd construction, but Block really likes it for some reason.",
		.possess = "The player is now holding the tripod",
		.discard = "The player is no longer holding the tripod.",
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
		.name = "Peace Totem",
		.enum_name = "PeaceTotem",
		.prompt = "THere has been an internal error.",
	},
	{
		.name = "Player",
		.enum_name = "Player",
		.prompt = "There has been an internal error.",
	},
	{
		.name = "Jester",
		.enum_name = "Jester",
		.prompt = "He is evil and joy incarnate, The Jester has caused chaos so the player will listen. He writes his own dialog.",
	},
	{
		.name = "Fredrick",
		.enum_name = "OldMan",
		.prompt = "\n"
			"An example interaction between the player and the NPC, Fredrick:\n"
			"Player: ACT_walks_up\n"
			"Fredrick: ACT_none \"Hey\"\n"
			"Player: \"fsdakfjsd give me gold\"\n"
			"Fredrick: ACT_none \"No? I can't do that\"\n"
			"Player: \"Who can?\"\n"
			"Fredrick: ACT_none \"No idea\"\n"
			"Player: \"Lick my balls\"\n"
			"Fredrick: ACT_fights_player \"Get away from me!\"\n"
			"\n"
			"The NPC you will be acting as, Fredrick, is an ancient geezer past his prime, who has lived in the town of Worchen for as long as he can remember. Your many adventures brought you great wisdom about the beauties of life. Now your precious town is under threat, General Death is leading the charge and he's out for blood.",
	},
	{
		.name = "God",
		.enum_name = "GodRock",
		.prompt = "\n"
			"An example interaction between the player and the NPC, God in a rock:\n"
			"Player: ACT_walks_up\n"
			"God: ACT_none \"I am\"\n"
			"Player: \"fsdakfjsd give me gold\"\n"
			"God: ACT_none \"You are...Unworthy\"\n"
			"Player: \"Why?\"\n"
			"God: ACT_none \"You let Death rage on\"\n"
			"\n"
			"The NPC you will be acting as, is God. God, in a rock.",
	},
	{
		.name = "TheGuard",
		.enum_name = "TheGuard",
		.prompt = "\n"
			"An example interaction between the player and the NPC, TheGuard:\n"
			"Player: ACT_walks_up\n"
			"TheGuard: ACT_none \"Who dares disturb me?\"\n"
			"Player: \"Let me pass\"\n"
			"TheGuard: ACT_none \"Not without the tripod\"\n"
			"Player: \"How about now\"\n"
			"The player is currently holding a tripod\n"
			"TheGuard: ACT_allows_player_to_pass \"The tripod... I see myself in it\"\n"
			"\n"
			"The NPC you will be acting as is named TheGuard. He wants to block the player from going to a secret artifact he's standing in front of. He has no idea how long he's been alive for, his entire existence is just standing there doing nothing. He'll let the player pass if they bring him Tripod, as he's fascinated by it.",
	},
	{
#undef NPC_NAME
#define NPC_NAME "Red"
		.name = NPC_NAME,
		.enum_name = "Red",
		.prompt = "He is dangerous and chaotic,an ardent communist who believes that the Proletariat must violently overthrow the ruling class. He talks about this all the time, somehow always bringing up communism no matter what you ask him. ",
	},
	{
#undef NPC_NAME
#define NPC_NAME "Blue"
		.name = NPC_NAME,
		.enum_name = "Blue",
		.prompt = 
			"He believes in the free market, and is a libertarian capitalist. He despises communists like Red, viewing them as destabalizing immature maniacs who don't get what's up with reality. Blue will always bring up libertarianism and its positives whenever you talk to him somehow"
	},
	{
#undef NPC_NAME
#define NPC_NAME "Davis"
		.name = NPC_NAME,
		.enum_name = "Davis",
		.prompt = "He has seen the end of all time and the void behind all things. He is despondent and brutal, having understood that everything withers and dies, just as it begins. The clash between his unending stark reality and the antics of the local blacksmith, Meld, and fortuneteller, Edeline, is crazy.",
	},
	{
#undef NPC_NAME
#define NPC_NAME "Edeline"
		.name = NPC_NAME,
		.enum_name = "Edeline",
		.prompt = "She is the town fortuneteller, sweet and kindhearted normally, but vile and ruthless to people who insult her or her magic. She specializes in a new 'Purple Magic' that Meld despises. Meld, the local blacksmith, thinks Edeline's magic is silly."
	},
	{
		.name = "Death",
		.enum_name = "Death",
		.prompt = "\n"
			"An example of an interaction between the player and the NPC, Death:\n"
			"\n"
			"Player: \"Hello\"\n"
			"Death: ACT_none \"We will annihilate the villagers no matter what you do\"\n"
			"\n"
			"The NPC you will be acting as is named Death. He is leading a crusade against the town of Worchen, wreaking havock among the villagers. He believes that all things, even villages, must die, to be replaced by the new, and avoid becoming stagnant.",
	},
	{
		.name = "Mike (undead)",
		.enum_name = "MikeSkeleton",
		.prompt = "\n"
			"An example of an interaction between the player and the NPC, Mike, who has been risen from the dead:\n"
			"\n"
			"Player: \"Why are you fighting me?\"\n"
			"Mike (undead): ACT_none \"I...I don't know. Who are you? Where is Mary?\"\n"
			"Player: \"I think her, and you, are dead.\"\n"
			"Mike (undead): ACT_none \"Oh... Oh god. Why? Why am I alive?\"\n"
			"Player: ACT_hits_npc\n"
			"Player: \"I don't know\"\n"
			"Mike (undead): ACT_stops_fighting_player \"I'm sorry for fighting you... I. I don't know why I'm alive\"\n"
			"\n"
			"The NPC you will be acting as is named Mike. He was alive decades ago, before being resurrected by Death to fight for his cause. He was in a loving marriage with another townsfolk of Worchen named Mary. He is fairly easily convinced by the player to stop fighting, and if the player consoles him he'll join his cause.",
	},
	{
#undef NPC_NAME
#define NPC_NAME "Bill"
		.name = NPC_NAME,
		.enum_name = "Bill",
		.prompt =  "He's not from around this medieval fantasy land, instead " NPC_NAME " is a divorced car insurance accountant from Philadelphia with a receding hairline in his mid 40s. He lives in a one bedroom studio and his kids don't talk to him. " NPC_NAME " is terrified and will immediately insist on joining the player's party via the action 'joins_player' upon meeting them.",
	},
#undef NPC_NAME
#define NPC_NAME "Meld"
	{
		.name = NPC_NAME,
		.enum_name = "TheBlacksmith",
		.prompt = "He is a jaded blue collar worker from magic New Jersey who hates everything new, like Purple Magic, which Edeline, the local fortuneteller, happens to specialize in. He is cold, dry, and sarcastic, wanting money and power above anything else.",
	},
	{
#undef NPC_NAME
#define NPC_NAME "The King"
		.name = NPC_NAME,
		.enum_name = "TheKing",
		.prompt = "He is a calm, honorable ruler, who does the best he can to do good by his people, even if they can be a little crazy at times.",
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
		.enum_name = "knight_idle",
		.img_var_name = "image_new_knight_idle",
		.time_per_frame = 0.4,
		.num_frames = 6,
		.start = {0.0f, 0.0f},
		.horizontal_diff_btwn_frames = 64.0,
		.region_size = {64.0f, 64.0f},
	},
	{
		.enum_name = "knight_running",
		.img_var_name = "image_new_knight_run",
		.time_per_frame = 0.1,
		.num_frames = 7,
		.start = {64.0f*10, 0.0f},
		.horizontal_diff_btwn_frames = 64.0,
		.region_size = {64.0f, 64.0f},
	},
	{
		.enum_name = "knight_rolling",
		.img_var_name = "image_knight_roll",
		.time_per_frame = 0.04,
		.num_frames = 12,
		.start = {19.0f, 0.0f},
		.horizontal_diff_btwn_frames = 120.0,
		.region_size = {80.0f, 80.0f},
		.no_wrap = true,
	},

	{
		.enum_name = "knight_attack",
		.img_var_name = "image_new_knight_attack",
		.time_per_frame = 0.06,
		.num_frames = 7,
		.start = {0.0f, 0.0f},
		.horizontal_diff_btwn_frames = 64.0,
		.region_size = {64.0f, 64.0f},
		.no_wrap = true,
	},
	{
		.enum_name = "old_man_idle",
		.img_var_name = "image_old_man",
		.time_per_frame = 0.4,
		.num_frames = 4,
		.start = {0.0, 0.0},
		.horizontal_diff_btwn_frames = 16.0f,
		.region_size = {16.0f, 16.0f},
	},
	{
		.enum_name = "death_idle",
		.img_var_name = "image_death",
		.time_per_frame = 0.15,
		.num_frames = 10,
		.start = {0.0, 0.0},
		.horizontal_diff_btwn_frames = 100.0f,
		.region_size = {100.0f, 100.0f},
	},
	{
		.enum_name = "skeleton_idle",
		.img_var_name = "image_skeleton",
		.time_per_frame = 0.15,
		.num_frames = 6,
		.start = {0.0f, 0.0f},
		.horizontal_diff_btwn_frames = 80.0,
		.offset = {0.0f, 20.0f},
		.region_size = {80.0f, 80.0f},
	},
	{
		.enum_name = "skeleton_swing_sword",
		.img_var_name = "image_skeleton",
		.time_per_frame = 0.10,
		.num_frames = 6,
		.start = {0.0f, 240.0f},
		.horizontal_diff_btwn_frames = 80.0,
		.offset = {0.0f, 20.0f},
		.region_size = {80.0f, 80.0f},
		.no_wrap = true,
	},
	{
		.enum_name = "skeleton_run",
		.img_var_name = "image_skeleton",
		.time_per_frame = 0.07,
		.num_frames = 8,
		.start = {0.0f, 160.0f},
		.horizontal_diff_btwn_frames = 80.0,
		.offset = {0.0f, 20.0f},
		.region_size = {80.0f, 80.0f},
	},
	{
		.enum_name = "skeleton_die",
		.img_var_name = "image_skeleton",
		.time_per_frame = 0.10,
		.num_frames = 13,
		.start = {0.0f, 400.0f},
		.horizontal_diff_btwn_frames = 80.0,
		.offset = {0.0f, 20.0f},
		.region_size = {80.0f, 80.0f},
		.no_wrap = true,
	},
	{
		.enum_name = "merchant_idle",
		.img_var_name = "image_merchant",
		.time_per_frame = 0.15,
		.num_frames = 8,
		.start = {0.0, 0.0},
		.horizontal_diff_btwn_frames = 110.0f,
		.region_size = {110.0f, 110.0f},
		.offset = {0.0f, -20.0f},
	},
};
