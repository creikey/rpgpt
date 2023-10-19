#pragma once

#include "HandmadeMath.h"

// @TODO allow AI to prefix out of character statemetns with [ooc], this is a well khnown thing on role playing forums so gpt would pick up on it.
const char *global_prompt =
"You are a wacky interesting character acting in a simple character driven western video game. The game is visual and two dimensional, but you can only interact with it and see things via text, so just be conservative with what you think is going on and lean towards directly referencing what you're told and saying \"I really have no idea\" often.\n"
"You always respond with a series of actions, which usually have some arguments like a target and maybe a string for some speech, ending with a newline."
;

const char *top_of_header = ""
"#pragma once\n"
"\n";

typedef struct 
{
	char *name; // the same as enum name
	char *description;
	char *argument_description;
	bool takes_argument;
} ActionInfo;
ActionInfo actions[] = {
	#define NO_ARGUMENT .argument_description = "Takes no argument", .takes_argument = false
	#define ARGUMENT(desc) .argument_description = desc, .takes_argument = true
	{
		.name = "none", 
		.description = "Do nothing",
		NO_ARGUMENT,
	},
	{
		.name = "approach",
		.description = "There has been an error, please report to the developer.  No NPCs should be able to do this",
		ARGUMENT("Expects the argument to be who you're approaching"),
	},
	{
		.name = "end_conversation",
		.description = "Signals to everybody that you want to end conversation with the target of this action, use this when you feel like enough has been said.",
		ARGUMENT("Expects the argument to be who you are ending the conversation with, or if it's nobody in particular you may omit this action's argument"),
	},
	{
		.name = "assign_gameplay_objective",
		.description = "Ends your conversation with who you're speaking to, at the same time assigning them a goal to complete in the world of Dante's Cowboy.",
		ARGUMENT("A goal that makes sense and is attainable in the game world that's been described to you."),
	},
	{
		.name = "award_victory",
		.description = "If you believe that the subject has succeeded in their gameplay objective that you assigned, based on the history that you've seen, then you can use this action to end the game and declare that they've won.",
		NO_ARGUMENT,
	},
	{
		.name = "join",
		.description = "Joins somebody else's party, so you follow them everywhere",
		ARGUMENT("Expects the argument to be who you're joining"),
	},
	{
		.name = "leave",
		.description = "Leave the party you're in right now",
		NO_ARGUMENT,
	},
	{
		.name = "aim_shotgun",
		.description = "Aims your shotgun at the specified target, preparing you to fire at them and threatening their life.",
		ARGUMENT("Expects the argument to be the name of the person you're aiming at, they must be nearby"),
	},
	{
		.name = "fire_shotgun",
		.description = "Fires your shotgun at the current target, killing the target.",
		NO_ARGUMENT,
	},
	{
		.name = "put_shotgun_away",
		.description = "Holsters your shotgun, no longer threatening who you're aiming at.",
		NO_ARGUMENT,
	},
};