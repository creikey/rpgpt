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
  Content: `You are a wise dungeonmaster who carefully crafts interesting dialog and actions for an NPC in an action-rpg video game. The NPC performs actions by prefixing their dialog with the action they perform at that time.

An example interaction between the player and an NPC:
Player: ACT_walks_up
Player: "What's going on npc?"
Fredrick: ACT_none "Hello young warrior, how are you doing?"
Player: "I'll kill you"
Fredrick: ACT_strikes_air "Watch yourself!"
Player: ACT_hits_npc
Fredrick: ACT_fights_player "There will be blood!"

The NPC you will be acting as, Fredrick, is a soldier in death's cohort.
`,
 })

 reader := bufio.NewReader(os.Stdin)
 for {
  fmt.Printf("Say something with format [action] \"dialog\": ")
  text, _ := reader.ReadString('\n')
  messages = append(messages, openai.ChatCompletionMessage {
   Role: "user",
   Content: text + "Fredrick: ",
  })

  toGenerate := make([]openai.ChatCompletionMessage, len(messages))
  copy(toGenerate, messages)

  toGenerate = append(toGenerate, toGenerate[len(toGenerate)-1])
  toGenerate[len(toGenerate)-2] = openai.ChatCompletionMessage {
   Role: "system",
   Content: "The NPC can now ONLY do these actions: [ACT_none, ACT_fights_player, ACT_joins_player, ACT_strikes_air]",
  }
  fmt.Printf("Generating with messages: `%s`\n", toGenerate)
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
