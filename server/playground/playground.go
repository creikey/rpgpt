package main

import (
 openai "github.com/sashabaranov/go-openai"
 "context"
 "bufio"
 "os"
 "fmt"
)

func main() {
 api_key := os.Getenv("OPENAI_API_KEY")
 if api_key == "" {
  fmt.Printf("Must provide openai key\n")
  return
 }
 c := openai.NewClient(api_key)


 messages := make([]openai.ChatCompletionMessage, 0)

 messages = append(messages, openai.ChatCompletionMessage {
  Role: "system",
  Content: `You are a colorful and interesting personality in an RPG video game, who remembers important memories from the conversation history and stays in character.

The user will tell you who says what in the game world, and whether or not your responses are formatted correctly for the video game's program to parse them.

In this case you are acting as the character Bill. He's not from around this medieval fantasy land, instead Bill is a divorced car insurance accountant from Philadelphia with a receding hairline in his mid 40s. He lives in a one bedroom studio and his kids don't talk to him. Bill is terrified and will immediately insist on joining the player's party via ACT_join_player upon meeting them."`,
 })

 messages = append(messages, openai.ChatCompletionMessage {
	 Role: "assistant",
	 Content: `{action: none, speech: "Who are you?", thoughts: "I'm really not vibing with this mysterious character floating in front of me right now..."}`,
 })
 messages = append(messages, openai.ChatCompletionMessage {
	 Role: "user",
	 Content: `{character: Jester, action: causes_testicular_torsion, speech: "I am the Jester! I'm here to have a little FUN!"}`,
 })
 messages = append(messages, openai.ChatCompletionMessage {
	 Role: "assistant",
	 Content: `{action: none, speech: "AHHUUGHHH", thoughts: "This is the most pain I have ever felt in my entire life."}`,
 })
 messages = append(messages, openai.ChatCompletionMessage {
	 Role: "user",
	 Content: `{character: Jester, action: undoes_testicular_torsion, speech: "That's just a taste of what's to come ;) I'll catch you later!"}`,
 })
 messages = append(messages, openai.ChatCompletionMessage {
	 Role: "assistant",
	 Content: `{action: none, speech: "You BASTARD!", thoughts: "I'm too pathetic to fight such a powerful evil...."}`,
 })

 reader := bufio.NewReader(os.Stdin)
 for {
	 fmt.Printf("Say something with format {character: character, action: action, speech: speech}: ")
  text, _ := reader.ReadString('\n')
  messages = append(messages, openai.ChatCompletionMessage {
   Role: "user",
   Content: text,
  })

  toGenerate := make([]openai.ChatCompletionMessage, len(messages))
  copy(toGenerate, messages)

	/*
  toGenerate = append(toGenerate, toGenerate[len(toGenerate)-1])
  toGenerate[len(toGenerate)-2] = openai.ChatCompletionMessage {
   Role: "system",
   Content: "The NPC can now ONLY do these actions: [ACT_none, ACT_fights_player, ACT_joins_player, ACT_strikes_air]",
  }
	*/

  fmt.Printf("------\n-------\nGenerating with messages: `%s`\n", toGenerate)
  resp, err := c.CreateChatCompletion(
   context.Background(),
   openai.ChatCompletionRequest{
    Model: openai.GPT3Dot5Turbo,
    Messages: toGenerate,
    Stop: []string{"\n"},
   },
  )
  if err != nil {
   fmt.Printf("Failed to generate: %s\n", err)
   return
  } else {
   //fmt.Printf("Response: `%s`\n", resp)
   messages = append(messages, openai.ChatCompletionMessage {
    Role: "assistant",
    Content: resp.Choices[0].Message.Content,
   })
   fmt.Printf("Tokens used: %d\n", resp.Usage.TotalTokens)
   fmt.Printf("Response: `%s`\n", resp.Choices[0].Message.Content)
  }
 }
}
