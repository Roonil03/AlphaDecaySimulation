package main

import (
	"bytes"
	"encoding/json"
	"io"
	"log"
	"net/http"
	"os"
	"strings"
	"time"
)

const maxBody = 1 << 20

func env(key, fallback string) string {
	if v := os.Getenv(key); v != "" {
		return strings.TrimRight(v, "/")
	}
	return fallback
}

func handler(kernel, ensemble string, client *http.Client) http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc("/health", func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodGet {
			w.Header().Set("Allow", "GET")
			http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
			return
		}
		w.Header().Set("Content-Type", "application/json")
		io.WriteString(w, `{"status":"ok"}`)
	})
	proxy := func(base, path string) http.HandlerFunc {
		return func(w http.ResponseWriter, r *http.Request) {
			if r.Method != http.MethodPost {
				w.Header().Set("Allow", "POST")
				http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
				return
			}
			body, err := io.ReadAll(http.MaxBytesReader(w, r.Body, maxBody))
			if err != nil {
				http.Error(w, "request body too large or unreadable", http.StatusRequestEntityTooLarge)
				return
			}
			if !json.Valid(body) {
				http.Error(w, "invalid JSON", http.StatusBadRequest)
				return
			}
			req, err := http.NewRequestWithContext(r.Context(), http.MethodPost, base+path, bytes.NewReader(body))
			if err != nil {
				http.Error(w, "upstream configuration error", http.StatusBadGateway)
				return
			}
			req.Header.Set("Content-Type", "application/json")
			resp, err := client.Do(req)
			if err != nil {
				http.Error(w, "upstream unavailable", http.StatusBadGateway)
				return
			}
			defer resp.Body.Close()
			data, err := io.ReadAll(io.LimitReader(resp.Body, maxBody+1))
			if err != nil || len(data) > maxBody {
				http.Error(w, "invalid upstream response", http.StatusBadGateway)
				return
			}
			if ct := resp.Header.Get("Content-Type"); ct != "" {
				w.Header().Set("Content-Type", ct)
			}
			w.WriteHeader(resp.StatusCode)
			w.Write(data)
		}
	}
	mux.HandleFunc("/v1/barrier", proxy(kernel, "/v1/barrier"))
	mux.HandleFunc("/v1/ensemble", proxy(ensemble, "/v1/ensemble"))
	return mux
}

func main() {
	server := &http.Server{Addr: ":8080", Handler: handler(env("KERNEL_URL", "http://kernel:8081"), env("ENSEMBLE_URL", "http://ensemble:8082"), &http.Client{Timeout: 15 * time.Second}), ReadHeaderTimeout: 5 * time.Second, ReadTimeout: 10 * time.Second, WriteTimeout: 20 * time.Second, IdleTimeout: 60 * time.Second}
	log.Fatal(server.ListenAndServe())
}
