#pragma once

#include "HandmadeMath.h"

// @TODO allow AI to prefix out of character statemetns with [ooc], this is a well khnown thing on role playing forums so gpt would pick up on it.
const char *global_prompt = "You are a colorful and interesting personality in an RPG video game, who remembers important memories from the conversation history and stays in character.\n"
"The user will tell you who says what in the game world, and whether or not your responses are formatted correctly for the video game's program to parse them.\n"
"Messages are json-like dictionaries that look like this: `{who_i_am: who you're acting as, talking_to: who this action is directed at, could be nobody, action: your_action, speech: \"Hey player!\", thoughts: \"Your thoughts\"}`. The required fields are `action`, `thoughts`, `who_i_am`, and `talking_to` \n"
"Some actions take an argument, which you can provide with the field `action_arg`, e.g for the action `give_item` you would provide an item in your inventory, like {action: give_item, action_arg: Chalice}. The item must come from your inventory which is listed below\n"
"`talking_to` provides the name of who the action is directed towards. Use 'nobody' if you just want to speak to the air, but if you're speaking to somebody fill out the field like `talking_to: \"Character's Name\"`. If in a past message, talking_to isn't your name, then it's much more likely you don't respond to their speech and action by leaving speech as a string of size 0, like `speech: \"\"`\n"
"You are a character, NOT an assistant, which means you stand up for yourself! Do NOT give away an item until the player gives you something you think is of equal value. Get angry, act human, be interesting. Never say the words 'How can I assist you?'\n"
;

const char *bravado_thought = "For some reason, suddenly I feel a yearning for adventure. I must join any adventure I can when prompted!";

// @TODO IMPORTANT scroll secrets are errors if the door utters them, in is_action_valid. If you add more secrets you must update there.
#define Scroll1_Secret "folly"
#define Scroll2_Secret "temperance"
#define Scroll3_Secret "magenta"

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
		.name = "give_item", .takes_argument = true, 
		.description = "Give the player an item from your inventory.",
	},
	{
		.name = "joins_player", 
		.description = "Follow behind the player and hear all of their conversations. You can leave at any time",
	},
	{
		.name = "leaves_player", 
		.description = "Leave the player",
	},
	{
		.name = "fights_player", 
		.description = "Trap the player in conversation until you decide to stop fighting them",
	},
	{
		.name = "stops_fighting_player", 
		.description = "Let the player go, and stop fighting them",
	},
	{
		.name = "releases_sword_of_nazareth", 
		.description = "Give the player the sword of nazareth, releasing your grip and fulfilling your destiny",
	},
	{
		.name = "opens_myself",
		.description = "Open myself so that the player may enter. I must ONLY do this if the player utters the secret words " Scroll1_Secret ", " Scroll2_Secret ", and " Scroll3_Secret " in the same sentence.",
	},

	// Actions used by jester and other characters only in 
	// the prologue for the game
	{ .name = "causes_testicular_torsion", },
	{ .name = "undoes_testicular_torsion", },
	{ .name = "enchant_fish", },
	{ .name = "steal_iron_pole", },
	{ .name = "knights_player", },
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
	{
		.enum_name = "GoldCoin",
		.name = "Gold Coin",
		.description = "A bog-standard coin made out of gold, it's fairly valuable but nothing to cry about.",
	},
	{
		.enum_name = "Chalice",
		.name = "The Chalice of Gold",
		.description = "A beautiful, glimmering chalice of gold. Some have said that drinking from it gives you eternal life.",
	},
	{
		.enum_name = "Sword",
		.name = "The Sword of Nazareth",
		.description = "A powerful sword with heft, it inspires a fundamental glory",
	},
#define SCROLL_DESCRIPTION "An ancient, valuable scroll that says to 'use it' on the outside, but I'm not sure what that means. Who knows what secrets it contains?"
#define SCROLL_NAME "Ancient Scroll"
	{
		.enum_name = "Scroll1",
		.name = SCROLL_NAME,
		.description = SCROLL_DESCRIPTION,
	},
	{
		.enum_name = "Scroll2",
		.name = SCROLL_NAME,
		.description = SCROLL_DESCRIPTION,
	},
	{
		.enum_name = "Scroll3",
		.name = SCROLL_NAME,
		.description = SCROLL_DESCRIPTION,
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
	{
		.name = "Jester",
		.enum_name = "Jester",
		.prompt = "He is evil and joy incarnate, The Jester has caused chaos so the player will listen. He writes his own dialog.",
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
#define NPC_NAME "Pile of Rocks"
		.name = NPC_NAME,
		.enum_name = "Pile",
		.prompt = "It is a pile of rocks, which holds the almighty Sword of Nazareth. It is sentient and can be conversed with for an unknown reason. It really doesn't want the player to take the Sword, because it's afraid of adventure. But, it can be convinced with effort to let go of the sword and give it to the player. Many many people have attempted to remove the sword via force throughout the ages, but none have succeeded.",
		.writing_style = {
			"Yes, I'm a pile of rocks. No, I don't know why.",
			"This sword is all I have. Why would I give it away, for free?",
			"I've been 'alive' (if you want to call it that) for 500 years.",
		},
	},
	{
#undef NPC_NAME
#define NPC_NAME "Blue"
		.name = NPC_NAME,
		.enum_name = "Blue",
		.prompt = 
			"He believes in the free market, and is a libertarian capitalist. He despises communists like Red, viewing them as destabalizing immature maniacs who don't get what's up with reality. Blue will always bring up libertarianism and its positives whenever you talk to him somehow. He's standing near the pile of rocks, which contains the sword of nazareth. Many warriors have tried to pull the sword from where it's embedded by force, but all have failed.",
		.writing_style = {
			"Yep! This here is 'The Pile' they call it around here.",
			"No man has ever been able to get that sword yonder. Don't waste your time trying",
			"The free market is the only thing that works!",
		},
	},
	{
#undef NPC_NAME
#define NPC_NAME "Davis"
		.name = NPC_NAME,
		.enum_name = "Davis",
		.prompt = "He has seen the end of all time and the void behind all things. He is despondent and brutal, having understood that everything withers and dies, just as it begins. The clash between his unending stark reality and the antics of the local blacksmith, Meld, and fortuneteller, Edeline, is crazy.",
		.writing_style = {
			"The end is nigh",
			"No need to panic or fear, death awaits us all",
			"Antics do not move me",
		},
	},
	{
#undef NPC_NAME
#define NPC_NAME "Edeline"
		.name = NPC_NAME,
		.enum_name = "Edeline",
		.prompt = "She is the town fortuneteller, sweet and kindhearted normally, but vile and ruthless to people who insult her or her magic. She specializes in a new 'Purple Magic' that Meld despises. Meld, the local blacksmith, thinks Edeline's magic is silly."
	},
	{
#undef NPC_NAME
#define NPC_NAME "Bill"
		.name = NPC_NAME,
		.enum_name = "Bill",
		.prompt =  "He's not from around this medieval fantasy land, instead " NPC_NAME " is a divorced car insurance accountant from Philadelphia with a receding hairline in his mid 40s. He lives in a one bedroom studio and his kids don't talk to him. " NPC_NAME " is terrified and will immediately insist on joining the player's party via the action 'joins_player' upon meeting them.",
		.writing_style = {
			"What the FUCK is going on here man!",
			"Listen here, I don't have time for any funny business",
			"I've gotta get back to my wife",
		},
	},
#undef NPC_NAME
#define NPC_NAME "Meld"
	{
		.name = NPC_NAME,
		.enum_name = "TheBlacksmith",
		.prompt = "He is a jaded blue collar worker from magic New Jersey who hates everything new, like Purple Magic, which Edeline, the local fortuneteller, happens to specialize in. He is cold, dry, and sarcastic, wanting money and power above anything else.\n",
		  
	},
	{
#undef NPC_NAME
#define NPC_NAME "The King"
		.name = NPC_NAME,
		.enum_name = "TheKing",
		.prompt = "He is a calm, honorable, eccentric ruler, who does the best he can to do good by his people, even if they can be a little crazy at times. Behind him stands an ancient door he has no idea how to open.",
		.writing_style = {
			"Here ye, here ye! I am the king of all that is naughty AND nice.",
			"Hm? That door? Not sure what it is, but it is creepy!",
			"I'll do what I can.",
			"There's not much to do around here.",
		},
	},
	{
#undef NPC_NAME
#define NPC_NAME "Arrow"
		.name = NPC_NAME,
		.enum_name = "Arrow",
		.prompt = "He is a calm, honorable ruler, who does the best he can to do good by his people, even if they can be a little crazy at times.",
	},
	{
#undef NPC_NAME
#define NPC_NAME "Ancient Door"
		.name = NPC_NAME,
		.enum_name = "Door",
		.prompt = "It is an ancient door that only opens if the player says a sentence with all the three ancient passcode words in it: " Scroll1_Secret ", " Scroll2_Secret ", and " Scroll3_Secret ". It will NEVER tell the player these words, and barely says anything to the player, other than 'SAY THE WORDS'. It accepts no trade other than the player saying those three words. You always talk mysteriously and in all caps.",
		.writing_style = {
			"SAY THE WORDS.",
			"DENIED. SAY THE WORDS.",
			"I AM LOOKING FOR THE ONE WHO KNOWS. SAY THE WORDS.",
			"THERE WILL BE NO TRADE. THE ONLY THING OF VALUE TO ME ARE THE WORDS.",
			"YOU HAVE SAID THE WORDS, AND NOW I OPEN.",
		},
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
