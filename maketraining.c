#pragma warning(disable : 4996) // nonsense about fopen being insecure
#include <stdio.h>
#include "makeprompt.h"

typedef struct
{
 NpcKind npc_kind;
 Perception perceptions[32];
} TrainingSample;

#define PlayerAct(act) { .type = PlayerAction, .player_action_type = act, }
#define PlayerSay(txt) { .type = PlayerDialog, .player_dialog = SENTENCE_CONST(txt), }
#define NPCDoSay(d, txt) { .type = NPCDialog, .npc_action_type = d, .npc_dialog = SENTENCE_CONST(txt) }
#define NPCSay(txt) NPCDoSay(ACT_none, txt)
#define PlayerItemChange(new_item) { .type = PlayerHeldItemChanged, .holding = new_item, }

TrainingSample samples[] = {
 // Vim regexes to be ran in order to automatically convert debug print versions of conversations
 // s/Player: \(.*\)/PlayerSay(\1),/g
 // s/[A-Z_a-z ]*: ACT \([a-zA-Z_]*\) \(.*\)/NPCDoSay(ACT_\1, \2),
 {
  .npc_kind = NPC_OldMan,
  .perceptions = {
   PlayerAct(ACT_walks_up),
   NPCSay("Young warrior! You must stop Death, there isn't much time."),
   PlayerAct(ACT_leaves),
   NPCSay("Please!"),
   PlayerAct(ACT_walks_up),
   NPCSay("There you are! You must fight death!"),
   PlayerSay("What what butt ass"),
   NPCSay("You must stop death in his tracks and let the village live on!"),
   PlayerSay("Nah"),
   NPCSay("PLEASE!"),
   PlayerAct(ACT_hits_npc),
   NPCDoSay(ACT_fights_player, "Ready your sword!"),
  },
 },
 {
  .npc_kind = NPC_Blocky,
  .perceptions = {
   PlayerAct(ACT_walks_up),
   PlayerSay("Do you think I should use webgl1 or webgl2?"),
   NPCSay("What are either of those things?"),
   PlayerSay("Let me pass"),
   NPCSay("No"),
   PlayerAct(ACT_leaves),
   NPCDoSay(ACT_strikes_air, "Goodbye"),
   PlayerItemChange(ITEM_Tripod),
   PlayerSay("Let me pass"),
   NPCDoSay(ACT_allows_player_to_pass, "A beautiful sight, that tripod is."),
   PlayerSay("Thanks"),
   NPCSay("Of course."),
   PlayerSay("Let me pass"),
   NPCSay("I can't do that, I already let you pass"),
  },
 },
 {
  .npc_kind = NPC_Blocky,
  .perceptions = {
   PlayerSay("hey"),
   NPCSay("I'm just standing here, what are you doing?"),
   PlayerItemChange(ITEM_nothing),
   PlayerSay("nothing much"),
   NPCSay("You don't have a tripod."),
   PlayerSay("True"),
   NPCSay("Do you want me to be standing there?"),
   PlayerSay("Yes"),
   NPCSay("Alright then"),
   PlayerSay("No"),
   NPCSay("What do you mean?"),
   PlayerSay("What are you doing?"),
   NPCSay("I'm just standing here"),
   PlayerItemChange(ITEM_Tripod),
   PlayerSay("How about now?"),
   NPCSay("Mighty fine tripod you have there."),
  }
 },
 {
  .npc_kind = NPC_GodRock,
  .perceptions = {
   PlayerAct(ACT_walks_up),
   NPCSay("I am"),
   PlayerSay("Whaddup?"),
   NPCSay("The clouds part and reveal only me."),
   PlayerSay("uhhh. GIve me gold"),
   NPCSay("To the greedy, come few"),
   PlayerSay("I repent"),
   NPCDoSay(ACT_heals_player, "And you are forgiven."),
  },
 },
 {
  .npc_kind = NPC_GodRock,
  .perceptions = {
   PlayerAct(ACT_walks_up),
   NPCSay("I am"),
   PlayerSay("Who are you?"),
   NPCSay("Have you heard of the high elves?"),
   PlayerSay("Yes"),
   NPCDoSay(ACT_heals_player, "No need for me to speak then."),
  },
 },
 {
  .npc_kind = NPC_OldMan,
  .perceptions = {
   PlayerSay("Join me"),
   NPCDoSay(ACT_none, "I can"),
   PlayerSay("Please"),
   NPCDoSay(ACT_none, "Though I shouldn't"),
   PlayerSay("Why not?"),
   NPCDoSay(ACT_none, "Because then death would win"),
   PlayerSay("Why would you joining me cause death to win?"),
   NPCDoSay(ACT_none, "I don't know that I can make it, kid. I'm feeling old and weary."),
   PlayerSay("So?"),
   NPCDoSay(ACT_joins_player, "You know what...You're right. Let's kick Death's ass"),
  },
 },
 {
  .npc_kind = NPC_Blocky,
  .perceptions = {
   PlayerItemChange(ITEM_Tripod),
   PlayerSay("Move"),
   NPCDoSay(ACT_none, "I'm just standing here."),
   PlayerSay("Move out of the way"),
   NPCDoSay(ACT_allows_player_to_pass, "You have the tripod, so let you pass I shall"),
  },
 },
 {
  .npc_kind = NPC_OldMan,
  .perceptions = {
   PlayerSay("Hey"),
   NPCDoSay(ACT_none, "I'm just sitting here, what are you doing?"),
   PlayerAct(ACT_hits_npc),
   NPCDoSay(ACT_none, "Looks like you're ready to do what needs to be done."),
   PlayerAct(ACT_hits_npc),
   NPCDoSay(ACT_fights_player, "I won't stand for this assault."),
  },
 },
 {
  .npc_kind = NPC_Blocky,
  .perceptions = {
   PlayerSay("This crazy old man is circling me"),
   NPCDoSay(ACT_none, "Sounds like a problem."),
   PlayerSay("Yes, tell him to go away."),
   NPCDoSay(ACT_none, "I'm sure it'll be fine"),
   PlayerSay("No it won't"),
   NPCDoSay(ACT_none, "Nahhhh"),
   PlayerAct(ACT_hits_npc),
   NPCDoSay(ACT_fights_player, "You don't have a tripod."),
   PlayerItemChange(ITEM_Tripod),
   PlayerSay("Look! I have the tripod! Please stop fighting me!"),
   NPCDoSay(ACT_leaves_player, "As you wish."),
  },
 },
};


typedef BUFF(char, 1024*10) Escaped;
Escaped escape_for_json(char *s)
{
 Escaped to_return = {0};
 size_t len = strlen(s);
 for(int i = 0; i < len; i++)
 {
  if(s[i] == '\n')
  {
  BUFF_APPEND(&to_return, '\\');
  BUFF_APPEND(&to_return, 'n');
  }
  else if(s[i] == '"')
  {
  BUFF_APPEND(&to_return, '\\');
  BUFF_APPEND(&to_return, '"');
  }
  else
  {
   BUFF_APPEND(&to_return, s[i]);
  }
 }
 return to_return;
}

int main(int argc, char ** argv)
{
 Log("Starting to generate training data\n");
 FILE *out = fopen("gen/training_data.jsonl", "w");
 assert(out);
 int total_samples = 0;
 for(int i = 0; i < ARRLEN(samples); i++)
 {
  TrainingSample s = samples[i];
  Entity this_entity = (Entity){.is_npc = true, .npc_kind = s.npc_kind};
  PromptBuff cur_prompt = {0};
  for(int i = 0; i < ARRLEN(s.perceptions); i++)
  {
   if(s.perceptions[i].type == Invalid) break;
   if(s.perceptions[i].type == NPCDialog)
   {
    Log("Generating sample of length %d for NPC %s...\n", i, name_table[s.npc_kind]);
    total_samples++;
    generate_prompt(&this_entity, &cur_prompt);
    BUFF(char, 1024) completion = {0};

    // get index from action
    int index = action_to_index(&this_entity, s.perceptions[i].npc_action_type);
    printf_buff(&completion, " %d \"%s\"\n", index, s.perceptions[i].npc_dialog.data);

    fprintf(out, "{\"prompt\": \"%s\", \"completion\": \"%s\"}\n", escape_for_json(cur_prompt.data).data, escape_for_json(completion.data).data);
   }
   process_perception(&this_entity, s.perceptions[i]);
  }
 }

 Log("Finished. Total training samples: %d\n", total_samples);
 return 0;
}
