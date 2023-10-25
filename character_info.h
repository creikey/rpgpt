#pragma once

#include "HandmadeMath.h"
#include "tuning.h"

// @TODO allow AI to prefix out of character statemetns with [ooc], this is a well khnown thing on role playing forums so gpt would pick up on it.
const char *global_prompt =
	"You are a wacky interesting character acting in a simple character driven western video game. The game is visual and two dimensional, but you can only interact with it and see things via text, so just be conservative with what you think is going on and lean towards directly referencing what you're told and saying \"I really have no idea\" often.\n"
	"You always respond with a series of actions, which usually have some arguments like a target and maybe a string for some speech, ending with a newline.";

const char *top_of_header = ""
							"#pragma once\n"
							"\n";

// text chunk must be a literal, not a pointer
// and this returns a s8 that points at the text chunk memory
#define TextChunkString8(t) S8((u8 *)(t).text, (t).text_length)
#define TextChunkVArg(t) S8VArg(TextChunkString8(t))
#define TextChunkLitC(s)                        \
	{                                           \
		.text = s, .text_length = sizeof(s) - 1 \
	} // sizeof includes the null terminator. Not good.
#define TextChunkLit(s) (TextChunk) TextChunkLitC(s)

// shorthand to save typing.
#define TCS8(t) TextChunkString8(t)
typedef struct TextChunk
{
	char text[MAX_SENTENCE_LENGTH];
	int text_length;
} TextChunk;

typedef enum
{
	ARGTYPE_invalid = 0,
	ARGTYPE_text,
	ARGTYPE_character,
} ArgType;

typedef struct ArgumentSpecification
{
	TextChunk name;
	TextChunk description;
	ArgType expected_type;
} ArgumentSpecification;

#define MAX_ARGS 3

typedef enum 
{
	ACT_invalid = 0,
	ACT_none,
	ACT_say_to,
} ActionKind;

typedef struct SituationAction
{
	TextChunk name;
	TextChunk description;
	BUFF(ArgumentSpecification, MAX_ARGS)
	args;
	ActionKind gameplay_action;
} SituationAction;
SituationAction gamecode_actions[] = {
	{
		.name = TextChunkLitC("none"),
		.description = TextChunkLitC("Do nothing"),
		.gameplay_action = ACT_none,
	},
	{
		.name = TextChunkLitC("say_to"),
		.description = TextChunkLitC("Say something to the target"),
		.args.cur_index = 2,
		.args.data = {
			{
				.name = TextChunkLitC("target"),
				.description = TextChunkLitC("The target of your speech. Must be a valid target specified earlier, must match exactly that target"),
				.expected_type = ARGTYPE_character,
			},
			{
				.name = TextChunkLitC("speech"),
				.description = TextChunkLitC("The content of your speech, is a string that's whatever you want it to be."),
				.expected_type = ARGTYPE_text,
			},
		},
		.gameplay_action = ACT_say_to,
	},
};

SituationAction *act_by_enum(ActionKind en) {
	ARR_ITER(SituationAction, gamecode_actions) {
		if(it->gameplay_action == en) {
			return it;
		}
	}
	assert(false);
	return 0;
}