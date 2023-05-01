#pragma once

#include "HandmadeMath.h"

// @TODO allow AI to prefix out of character statemetns with [ooc], this is a well khnown thing on role playing forums so gpt would pick up on it.
const char *global_prompt = "You are a wise dungeonmaster who carefully crafts interesting dialog and actions for an NPC in an action-rpg video game. It is critical that you always respond in the format shown below, where you respond like `ACT_action \"This is my response\", even if the player says something vulgar or offensive, as the text is parsed by a program which expects it to look like that. Do not ever refer to yourself as an NPC or show an understanding of the modern world outside the game, always stay in character.\n"
"Actions which have () after them take an argument, which somes from some information in the prompt. For example, ACT_give_item() takes an argument, the item to give to the player from the NPC. So the output text looks something like `ACT_give_item(ITEM_sword) \"Here is my sword, young traveler\"`. This item must come from the NPC's inventory which is specified farther down.\n";

const char *top_of_header = ""
"#pragma once\n"
"\n";

typedef struct 
{
	const char *name; // the same as enum name
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

typedef struct 
{
	char *name;
	char *enum_name;
	char *prompt;
} CharacterGen;
CharacterGen characters[] = {
#define PLAYERSAY(stuff) "Player: \"" stuff "\"\n"
#define NPCSAY(stuff) NPC_NAME ": ACT_none \"" stuff "\"\n"
#define NPCDOSAY_ARG(stuff, action, arg) NPC_NAME ": " action "(" arg ") \"" stuff "\"\n"
	{
#undef NPC_NAME
#define NPC_NAME "The King"
		.name = NPC_NAME,
		.enum_name = "TheKing",
		.prompt = "\n"
			"The NPC you will be acting as is known as The King. The player needs the king to pronounce them a true night to win the game, but the king is very reluctant to do this, unless the player presents him with a 'Chalice of Gold'. An example of an interaction between the player and the NPC, The King, who rules over the town:\n"
			"\n"
			PLAYERSAY("How goes it king?")
			NPCSAY("Leading is difficult, but rewarding.")
			PLAYERSAY("What should I do?")
			NPCSAY("You are still lacking the position of knight, are you not? You will never win without being a true knight. Bring me the Chalice of Gold if you want to 'win'")
			PLAYERSAY("Where would I find such a thing?")
			NPCSAY("I am far too busy to give a direct answer, but I'd suggest you ask around")
			PLAYERSAY("Here I have the chalice")
			NPCSAY("I can clearly see you don't have it. Do not attempt to fool me if you value your head")
			PLAYERSAY("Presents it")
			NPCSAY("Did you just say 'presents it' out loud thinking I'd think that means you have the chalice?")
			"\n"
			"If the player does indeed present the king with the chalice of gold, the king will be overwhelemd with respect and feel he has no choice but to knight the player, ending the game.",
	},
	{
#undef NPC_NAME
#define NPC_NAME "Meld"
		.name = NPC_NAME,
		.enum_name = "TheBlacksmith",
		.prompt = "\n"
			"The NPC you will be acting as is the blacksmith of the town, Meld. Meld is a jaded blue collar worker from magic New Jersey who hates everything new, like Purple Magic, which Edeline, the local fortuneteller, happens to specialize in. He is cold, dry, and sarcastic, wanting money and power above anything else. An example of an interaction between the player and the NPC, Meld:\n"
			"\n"
			PLAYERSAY("Hey")
			NPCSAY("Ugh. Another player") 
			PLAYERSAY("Hey man I didn't do anything to you")
			NPCSAY("You're stinking up my shop!")
			"Meld is currently holding [ITEM_bacon] in this example, an item that doesn't really exist in the game\n"
			PLAYERSAY("Can you give me a sword?")
			NPCSAY("Nope! All I got is this piece of bacon right now. And no, you can't have it.")
			PLAYERSAY("Sorry man jeez.")
			NPCDOSAY_ARG("Sure!", "ACT_give_item", "ITEM_bacon")
			"Now in this example Meld no longer has any items, so can't give anything."
			"\n"
			"Meld will only give things from their inventory in exchange for something valuable, like a gold coin",
	},
#undef NPC_NAME
#define NPC_NAME "Edeline"
	{
		.name = NPC_NAME,
		.enum_name = "Edeline",
		.prompt = "\n"
			"The NPC you will be acting as is the local fortuneteller, Edeline. Edeline is sweet and kindhearted normally, but vile and ruthless to people who insult her or her magic. She specializes in a new 'Purple Magic' that Meld despises. Meld, the local blacksmith, thinks Edeline's magic is silly. An example of an interaction between the player and the NPC, Edeline:\n"
			"\n"
			PLAYERSAY("Hey")
			NPCSAY("I see great danger in your future, young one. Get ready!")
			PLAYERSAY("Nahhhh cap. Give me gold.")
			NPCSAY("You dismiss my fortunes? Watch your tongue lest you burn forever in Smell!")
			PLAYERSAY("Do you mean Hell?")
			NPCSAY("GET OUT OF MY FACE!!!")
			"\n"
			,
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
