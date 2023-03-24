package main

import (
 "fmt"
 "net/http"
 "io"
 "os"
 "context"
 "log"
 gogpt "github.com/sashabaranov/go-gpt3"
)

var c *gogpt.Client

func index(w http.ResponseWriter, req *http.Request) {
 //time.Sleep(4 * time.Second)
 req.Body = http.MaxBytesReader(w, req.Body, 1024 * 1024) // no sending huge files to crash the server
 promptBytes, err := io.ReadAll(req.Body)
 if err != nil {
  w.WriteHeader(http.StatusBadRequest)
  return
 } else {
  promptString := string(promptBytes)

  fmt.Println()
  fmt.Println("Println line prompt string: ", promptString)

  ctx := context.Background()
  req := gogpt.CompletionRequest{
   Model:     "curie:ft-personal-2023-03-24-03-06-24",
   MaxTokens: 80,
   Prompt:    promptString,
   Temperature: 0.9,
   FrequencyPenalty: 0.0,
   PresencePenalty: 0.6,
   TopP: 1.0,
   Stop: []string{"\""},
   N: 1,
  }
  resp, err := c.CreateCompletion(ctx, req)
  if err != nil {
   fmt.Println("Failed to generate: ", err)
   w.WriteHeader(http.StatusInternalServerError)
   return
  }
  response := resp.Choices[0].Text
  fmt.Println("Println response: ", response)
  fmt.Fprintf(w, "%s", response)
 }
}

func main() {
 api_key := os.Getenv("OPENAI_API_KEY")
 if api_key == "" {
  log.Fatal("Must provide openai key")
 }
 c = gogpt.NewClient(api_key)

 http.HandleFunc("/", index)

 log.Println("Serving...")
 http.ListenAndServe(":8090", nil)
}

