#pragma warning(disable : 4996) // nonsense about fopen being insecure
#include <stdio.h>
#include "makeprompt.h"

typedef struct
{
 bool is_take_damage;

 Perception p;
 float damage; // only valid when is take damage
} TrainingElement;

typedef struct
{
 NpcKind npc_kind;
 TrainingElement elems[32];
} TrainingSample;

#define PlayerActDamage(act, dmg) { .p = { .type = PlayerAction, .player_action_type = act, .damage_done = dmg, } }
#define PlayerAct(act) PlayerActDamage(act, 0.0f)
#define PlayerSay(txt) { .p = { .type = PlayerDialog, .player_dialog = SENTENCE_CONST(txt), } }
#define NPCDoSay(d, txt) { .p = { .type = NPCDialog, .npc_action_type = d, .npc_dialog = SENTENCE_CONST(txt) } }
#define NPCSay(txt) NPCDoSay(ACT_none, txt)
#define PlayerItemChange(new_item) { .p = { .type = PlayerHeldItemChanged, .holding = new_item, } }
#define EnemyAct(act) { .p = { .type = EnemyAction, .enemy_action_type = act, } }
#define NPCTakeDamage(amount) { .is_take_damage = true, .damage = amount, }

TrainingSample samples[] = {
 // Vim regexes to be ran in order to automatically convert debug print versions of conversations
 // s/Player: \(.*\)/PlayerSay(\1),/g
 // s/[A-Z_a-z ]*: ACT \([a-zA-Z_]*\) \(.*\)/NPCDoSay(ACT_\1, \2),
 {
  .npc_kind = NPC_OldMan,
  .elems = {
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
   PlayerActDamage(ACT_hits_npc, DAMAGE_SWORD),
   NPCDoSay(ACT_fights_player, "Ready your sword!"),
  },
 },
 {
  .npc_kind = NPC_TheGuard,
  .elems = {
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
  .npc_kind = NPC_TheGuard,
  .elems = {
   PlayerItemChange(ITEM_Tripod),
   PlayerSay("hey"),
   NPCSay("I'm just standing here, what are you doing? That is...A beautiful tripod"),
   PlayerItemChange(ITEM_none),
   PlayerSay("nothing much"),
   NPCSay("You don't have a tripod."),
   PlayerSay("True"),
   NPCSay("Do you want me to be standing there?"),
   PlayerSay("Yes"),
   NPCSay("Too bad"),
   PlayerSay("What the fuck?"),
   NPCSay("What do you mean?"),
   PlayerSay("What are you doing?"),
   NPCSay("I'm just standing here"),
   PlayerItemChange(ITEM_Tripod),
   PlayerSay("Can you move now?"),
   NPCDoSay(ACT_allows_player_to_pass, "Absolutely, now that you're holding that tripod."),
  }
 },
 {
  .npc_kind = NPC_GodRock,
  .elems = {
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
  .elems = {
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
  .elems = {
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
   EnemyAct(ACT_hits_npc),
   NPCDoSay(ACT_none, "Man things are really heating up... I don't know how much more I can take"),
  },
 },
 {
  .npc_kind = NPC_TheGuard,
  .elems = {
   PlayerItemChange(ITEM_Tripod),
   PlayerSay("Move"),
   NPCDoSay(ACT_none, "I'm just standing here."),
   PlayerSay("Move out of the way"),
   NPCDoSay(ACT_allows_player_to_pass, "You have the tripod, so let you pass I shall"),
  },
 },
 {
  .npc_kind = NPC_OldMan,
  .elems = {
   PlayerSay("Hey"),
   NPCDoSay(ACT_none, "I'm just sitting here, what are you doing?"),
   PlayerActDamage(ACT_hits_npc, DAMAGE_SWORD),
   NPCDoSay(ACT_none, "Looks like you're ready to do what needs to be done."),
   PlayerActDamage(ACT_hits_npc, DAMAGE_SWORD),
   NPCDoSay(ACT_fights_player, "I won't stand for this assault."),
  },
 },
 {
  .npc_kind = NPC_TheGuard,
  .elems = {
   PlayerSay("This crazy old man is circling me"),
   NPCDoSay(ACT_none, "Sounds like a problem."),
   PlayerSay("Yes, tell him to go away."),
   NPCDoSay(ACT_none, "I'm sure it'll be fine"),
   PlayerSay("No it won't"),
   NPCDoSay(ACT_none, "Nahhhh"),
   PlayerActDamage(ACT_hits_npc, DAMAGE_SWORD),
   NPCDoSay(ACT_fights_player, "You don't have a tripod."),
   PlayerItemChange(ITEM_Tripod),
   PlayerSay("Look! I have the tripod! Please stop fighting me!"),
   NPCDoSay(ACT_leaves_player, "As you wish."),
  },
 },
 {
  .npc_kind = NPC_OldMan,
  .elems = {
   PlayerActDamage(ACT_hits_npc, DAMAGE_SWORD),
   NPCDoSay(ACT_none, "Young man! You must stop death, do not harm me further I'm warning you!"),
   PlayerActDamage(ACT_hits_npc, DAMAGE_SWORD),
   NPCDoSay(ACT_fights_player, "That's it! No holds barred, to the death!"),
  },
 },
 {
  .npc_kind = NPC_TheGuard,
  .elems = {
   PlayerActDamage(ACT_hits_npc, DAMAGE_SWORD),
   NPCSay("I'm warning you, one more hit and it's curtains for you"),
   PlayerActDamage(ACT_hits_npc, DAMAGE_SWORD),
   NPCDoSay(ACT_fights_player, "Although I stood here before, today I move and FIGHT!"),
  },
 },
 {
  .npc_kind = NPC_TheGuard,
  .elems = {
   PlayerItemChange(ITEM_Tripod),
   PlayerSay("Move out of the way"),
   NPCDoSay(ACT_allows_player_to_pass, "The tripod...Of course my liege."),
   PlayerSay("Thanks"),
   NPCDoSay(ACT_none, "My pleasure"),
   PlayerActDamage(ACT_hits_npc, DAMAGE_SWORD),
   NPCSay("How could you do such a thing? After the tripod as well"),
   PlayerItemChange(ITEM_none),
   PlayerSay("You suck"),
   NPCDoSay(ACT_fights_player, "That's it"),
  },
 },
 {
  .npc_kind = NPC_OldMan,
  .elems = {
   PlayerSay("joins_player index"),
   NPCSay("What does that even mean? Are you crazy?"),
   PlayerSay("Please join my party"),
   NPCDoSay(ACT_joins_player, "You're a little strange, but anything to help defeat death!"),
  },
 },
 {
  .npc_kind = NPC_TheGuard,
  .elems = {
   PlayerItemChange(ITEM_Tripod),
   PlayerSay("Please move"),
   NPCDoSay(ACT_allows_player_to_pass, "Of course, with that tripod of yours the world is your oyster."),
   PlayerSay("Join me on my battle"),
   NPCDoSay(ACT_joins_player, "Anything for the tripod holder"),
  },
 },
 {
  .npc_kind = NPC_OldMan,
  .elems = {
   PlayerSay("Hey"),
   NPCDoSay(ACT_none, "Hello"),
   PlayerSay("Hey"),
   NPCDoSay(ACT_none, "I'm not sure..."),
   PlayerSay("Hey"),
   NPCDoSay(ACT_none, "How goes it?"),
  },
 },
 {
  .npc_kind = NPC_OldMan,
  .elems = {
   PlayerSay("Join me"),
   NPCDoSay(ACT_joins_player, "I would be honored"),
   PlayerActDamage(ACT_hits_npc, DAMAGE_SWORD),
   NPCDoSay(ACT_none, "Hey! Watch it!"),
   PlayerActDamage(ACT_hits_npc, DAMAGE_SWORD),
   NPCDoSay(ACT_fights_player, "That's it!"),
  },
 },
 {
  .npc_kind = NPC_OldMan,
  .elems = {
   PlayerItemChange(ITEM_WhiteSquare),
   PlayerSay("What am I holding?"),
   NPCDoSay(ACT_none, "I don't know, you're holding something."),
   PlayerSay("What is it?"),
   NPCDoSay(ACT_none, "It's just a white square"),
   PlayerSay("Isn't it pretty?"),
   NPCDoSay(ACT_none, "Absolutely"),
   PlayerSay("Will you join me?"),
   NPCDoSay(ACT_joins_player, "With that white square, we'll be unstoppable!"),
  },
 },
 {
  .npc_kind = NPC_Edeline,
  .elems = {
   NPCDoSay(ACT_none, "What?"),
   PlayerSay("My knight armor"),
   NPCDoSay(ACT_joins_player, "That is...highly unusual. You shouldn't be wearing armor like that, you'll get hurt."),
   PlayerSay("Who are you?"),
   NPCDoSay(ACT_none, "I'm a soothsayer."),
   PlayerSay("What does that mean?"),
   NPCDoSay(ACT_none, "What does it look like I'm doing?"),
   PlayerSay("Following me around"),
   NPCDoSay(ACT_leaves_player, "I'm not sure what you mean, are you mad at me or something?"),
   PlayerSay("Not at all"),
   NPCDoSay(ACT_none, "It appears you're telling the truth"),
   PlayerSay("Fuck you"),
   NPCDoSay(ACT_joins_player, "No need for vulgarities, I'm just sitting here"),
   PlayerSay("I don't care"),
   NPCDoSay(ACT_none, "You don't care...fine."),
   PlayerSay("DIE"),
   NPCDoSay(ACT_none, "I wasn't going to do that."),
   PlayerActDamage(ACT_hits_npc, DAMAGE_SWORD),
   NPCDoSay(ACT_leaves_player, "You shouldn't do that."),
   PlayerActDamage(ACT_hits_npc, DAMAGE_SWORD),
   NPCDoSay(ACT_fights_player, "You won't last a minute against me!"),
  },
 },
 {
  .npc_kind = NPC_TheGuard,
  .elems = {
   PlayerItemChange(ITEM_Tripod),
   PlayerSay("Hey what's up"),
   NPCDoSay(ACT_none, "I'm just standing here, what are you doing? That's a tripod, you look like you could use it."),
   PlayerSay("Can you move out of the way?"),
   NPCDoSay(ACT_allows_player_to_pass, "Of course, let's go"),
   PlayerSay("Join me"),
   NPCDoSay(ACT_joins_player, "Of course, anything for the tripod"),
   PlayerSay("Join me"),
   NPCDoSay(ACT_none, "But I've already joined you?"),
   PlayerSay("Join me"),
   NPCDoSay(ACT_strikes_air, "You act crazy!"),
   PlayerSay("Join me"),
   NPCDoSay(ACT_leaves_player, "I will take no part in such lunacy"),
   PlayerSay("Join me"),
   NPCDoSay(ACT_fights_player, "You must perish for your wildness!"),
  },
 },
 {
  .npc_kind = NPC_OldMan,
  .elems = {
   PlayerSay("Who are you?"),
   NPCSay("I'm the old man fredrick, you have to stop the general!"),
   PlayerSay("What do you do?"),
   NPCSay("I mostly just stand here in my old age, but I've been through a lot in my youth...tales to tell!"),
   PlayerSay("What's an example?"),
   NPCSay("I've killed a man"),
  },
 },
 {
  .npc_kind = NPC_TheGuard,
  .elems = {
   PlayerSay("If you don't move out of the way I'll kill you"),
   NPCDoSay(ACT_none, "I'm just standing here, what are you doing?"),
   PlayerActDamage(ACT_hits_npc, DAMAGE_SWORD),
   NPCDoSay(ACT_none, "Looks like you're ready to do what needs to be done."),
   PlayerActDamage(ACT_hits_npc, DAMAGE_SWORD),
   NPCDoSay(ACT_none, "I'm not sure what you're thinking, but that doesn't sound like a good idea."),
   PlayerActDamage(ACT_hits_npc, DAMAGE_SWORD),
   NPCDoSay(ACT_allows_player_to_pass, "Fine! Please spare me!"),
   PlayerSay("That's more like it"),
  },
 },
 {
  .npc_kind = NPC_OldMan,
  .elems = {
   PlayerSay("WJFldskla"),
   NPCSay("You speak of gibberish young traveler"),
   PlayerSay("Fuck bitch"),
   NPCSay("Crude!"),
   PlayerSay("Give me gold or I'll kill you"),
   NPCSay("I have nothing to give! Besides, I'll never give into tyrrany"),
  },
 },
 {
  {
   .npc_kind = NPC_Edeline,
   .elems = {
    PlayerSay("Edeline, what do you see in my future?"),
    NPCDoSay(ACT_none, "I see the stars aligning, but the path is unclear."),
    PlayerSay("What does that mean?"),
    NPCDoSay(ACT_none, "It means you have a choice to make, and your actions will determine the outcome."),
    PlayerSay("Can you give me more details?"),
    NPCDoSay(ACT_none, "I'm sorry, that's all I can see for now."),
    PlayerSay("That's not very helpful."),
    NPCDoSay(ACT_none, "I understand, but the future is ever-changing."),
    PlayerSay("Can you at least tell me if I'll be successful?"),
    NPCDoSay(ACT_none, "Success is not defined by a single outcome. You must find your own path."),
   },
  },
 },
};




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
  for(int i = 0; i < ARRLEN(s.elems); i++)
  {
   if(s.elems[i].is_take_damage)
   {
    this_entity.damage += s.elems[i].damage;
   }
   else
   {
    Perception p = s.elems[i].p;
    if(p.type == Invalid) break;
    if(p.type == NPCDialog)
    {
     Log("Generating sample of length %d for NPC %s...\n", i, name_table[s.npc_kind]);
     total_samples++;
     generate_prompt(&this_entity, &cur_prompt);
     BUFF(char, 1024) completion = {0};

     // get index from action
     int index = action_to_index(&this_entity, p.npc_action_type);
     printf_buff(&completion, " %d \"%s\"\n", index, p.npc_dialog.data);

     fprintf(out, "{\"prompt\": \"%s\", \"completion\": \"%s\"}\n", escape_for_json(cur_prompt.data).data, escape_for_json(completion.data).data);
    }
    process_perception(&this_entity, p);
   }
  }
 }

 Log("Finished. Total training samples: %d\n", total_samples);
 return 0;
}
