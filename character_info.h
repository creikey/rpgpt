#pragma once

#include "HandmadeMath.h"

// @TODO allow AI to prefix out of character statemetns with [ooc], this is a well khnown thing on role playing forums so gpt would pick up on it.
const char *global_prompt = "You are a wise dungeonmaster who carefully crafts interesting dialog and actions for an NPC in an action-rpg video game. It is critical that you always respond in the format shown below, where you respond like `ACT_action \"This is my response\", even if the player says something vulgar or offensive, as the text is parsed by a program which expects it to look like that. Do not ever refer to yourself as an NPC or show an understanding of the modern world outside the game, always stay in character.";

const char *top_of_header = ""
"#pragma once\n"
"\n";

const char *actions[] = {
 "none",

 // mostly player actions
 "walks_up",
 "hits_npc",
 "leaves",

 // mostly npc actions
 "allows_player_to_pass",
 "gives_tripod",
 "heals_player",
 "fights_player",
 "strikes_air",
 "joins_player",
 "leaves_player",
};

typedef struct
{
 char *global_prompt;
 char *enum_name;
 char *possess;
 char *discard;
} ItemInfo;
ItemInfo items[] = {
 {
  .enum_name = "none",
  .global_prompt = "The player isn't holding anything",
  .possess = "The player is no longer holding anything",
  .discard = "The player is no longer holding nothing",
 },
 {
  .enum_name = "WhiteSquare",
  .global_prompt = "The player is holding a mysterious white square. It is unknown what strange and erotic abilities one has when they possess the square.",
  .possess = "The player is now holding the white square",
  .discard = "The player is no longer holding the white square.",
 },
 {
  .enum_name = "Boots",
  .global_prompt = "The player is holding the boots of speed. He is a force to be recogned with in this state, he has great speed while holding the boots.",
  .possess = "The player is now holding the boots of speed",
  .discard = "The player is no longer holding the boots of speed",
 },
 {
  .enum_name = "Tripod",
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
  .name = "Edeline",
  .enum_name = "Edeline",
  .prompt = "\n"
   "An example of an interaction between the player and the NPC, Edeline:\n"
   "\n"
   "Player: \"Hello\"\n"
   "Edeline: ACT_none \"I see great danger in your future.\"\n"
   "Player: \"Oh really?\""
   "The player is currently holding a tripod\n"
   "Edeline: ACT_none \"That tripod will be the decisive factor in your victory\"\n"
   "\n"
   "The NPC you will be acting as is named Edeline. She is the master of the future, the star reader. Both are self-given titles, but her ability to predict the future has garnered attention from many who live in Worchen. However, some have called her “unreliable” at times and her predictions can at times be either cryptic or broadly interpreted.",
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
