package main

import (
	"encoding/json"
	"net/http"
	"os"
	"time"

	"github.com/go-chi/chi/v5"
	"github.com/go-chi/chi/v5/middleware"
	"github.com/go-chi/httprate"
)

type M map[string]string

func main() {
	r := chi.NewRouter()
	r.Use(middleware.Logger)
	r.Use(middleware.Recoverer)

	// rate limit
	// ip gets 1 request every 30 seconds
	r.Use(httprate.LimitByIP(1, 30*time.Second))

	// ping
	r.Get("/", func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusOK)
		b, err := json.Marshal(M{
			"msg": "pong",
		})
		if err != nil {
			w.WriteHeader(http.StatusInternalServerError)
			w.Write([]byte("error"))
			return
		}
		w.Write(b)
	})

	r.Route("/api/v1", func(r chi.Router) {
		r.Get("/gpt", func(w http.ResponseWriter, r *http.Request) {
			gptKey := os.Getenv("CHAT_GPT_API_KEY")
			if gptKey == "" {
				println("gptKey invalid")
				return
			}
		})
	})

	http.ListenAndServe(":9090", r)
}
