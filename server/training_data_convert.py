
def general_prompt(actions):
    return f"This is a conversation between a player and an NPC in a game, where the npc performs actions by saying one of {actions}. The NPC doesn't say anything in stars that isn't in that list between [ and ]. The player is wearing a full suit of knight armor."

prompts = {
    "Death" : general_prompt("[*moves*]") + " The NPC, death, is a standoffish character who responds in all capitals and short terse sentences. He is blocking the player and will let them pass if the player asks.",
    "Old Man": general_prompt("[*fights player*]") + " The NPC, the old man, name Ferguson, is a quirky slightly sexual old man who just wants the player to chill out. If the player aggravates him he will fight them with his shotgun.",
    "Merchant": general_prompt("[*sells grounding boots*, *sells swiftness boots*, *fights player*]") + " The NPC, the merchant, name Henry, is a panicked salesman who really wants to sell the player his grounding boots, as they decrease move speed so nobody wants to buy them. Like the old man he has a shotgun he is not afraid to use if the player is forceful or rude. He also is selling boots of swiftness, but is reluctant to sell them. He doesn't take or use money, only funny jokes. He likes clever non-vulgar comedy",
}

with open("../gen/prompts.gen.h", "w") as w:
    for p in prompts:
        w.write(f"#define PROMPT_{p.upper().replace(' ', '_')} \"{prompts[p]}\\n\"\n")

with open("converted_training.jsonl", "w") as w:
    with open("training_data.txt", "r") as f:
        text = f.read()
        conversations = text.split("\n\n")
        for c in conversations:
            sentences = c.split("\n")
            if len(sentences) > 1:
                indices_to_delete = []
                for s_i in range(len(sentences)):
                    s = sentences[s_i]
                    if len(s) == 0:
                        indices_to_delete.append(s_i)

                for i in indices_to_delete:
                    sentences.pop(i)

                assert len(sentences) % 2 == 0, f"Sentences: {sentences}, length: {len(sentences)}"
                assert sentences[0].startswith("Player:"), sentences[0]

                training_string = ""

                for s_i in range(len(sentences)):
                    s = sentences[s_i]
                    if not s.startswith("Player:"):
                        npc_name_prompt = s.split(":")[0]
                        prompt = prompts[npc_name_prompt] + "\\n"
                        prompt += "\\n".join(sentences[:s_i]).replace('"', '\\"')
                        prompt += f"\\n{npc_name_prompt} \\\""
                        completion = s.split(":")[1].split("\"")[1].replace('"', '\\"')
                        completion += '\\"'
                        #print(f"Prompt: {prompt} | \n\nCompletion: {completion}\n\n")
                        training_string += f'{{"prompt": "{prompt}", "completion": "{completion}"}}\n'

                #print("--")

                w.write(training_string)
