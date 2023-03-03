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
                    if s.startswith("Old Man:"):
                        prompt = "\\n".join(sentences[:s_i]).replace('"', '\\"')
                        prompt += "\\nOld Man: \\\""
                        completion = s.split(":")[1].split("\"")[1].replace('"', '\\"')
                        completion += '\\"'
                        #print(f"Prompt: {prompt} | \n\nCompletion: {completion}\n\n")
                        training_string += f'{{"prompt": "{prompt}", "completion": "{completion}"}}\n'

                #print("--")

                w.write(training_string)
