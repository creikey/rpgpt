package main

import (
 "fmt"
 "net/http"
 "io"
 "os"
 "context"
 gogpt "github.com/sashabaranov/go-gpt3"
)

var c *gogpt.Client

func index(w http.ResponseWriter, req *http.Request) {
 //time.Sleep(4 * time.Second)
 (w).Header().Set("Access-Control-Allow-Origin", "*")
 req.Body = http.MaxBytesReader(w, req.Body, 1024 * 1024) // no sending huge files to crash the server
 promptBytes, err := io.ReadAll(req.Body)
 if err != nil {
  w.WriteHeader(http.StatusBadRequest)
  return
 } else {
  promptString := string(promptBytes)

  fmt.Println("Println line prompt string: ", promptString)

  ctx := context.Background()
  req := gogpt.CompletionRequest{
   Model:     "curie:ft-personal-2023-02-28-23-44-34",
   MaxTokens: 30,
   Prompt:    promptString,
   Temperature: 0.9,
   Stop: []string{"\""},
   N: 1,
  }
  resp, err := c.CreateCompletion(ctx, req)
  if err != nil {
   w.WriteHeader(http.StatusInternalServerError)
   return
  }
  response := resp.Choices[0].Text
  fmt.Println("Println response: ", response)
  fmt.Fprintf(w, "%s", response)
 }
}

func main() {
 c = gogpt.NewClient(os.Getenv("OPENAI_KEY"))

 http.HandleFunc("/", index)

 http.ListenAndServe(":8090", nil)
}

