package main

import (
	"embed"
	"fmt"
	"io"
	"log"
	"math"
	"net/http"
	"net/http/httputil"
	"net/url"
	"os"
	"strconv"
	"strings"
	"time"

	"github.com/fogleman/ln/ln"
)

//go:embed index.html app.js
var assets embed.FS

func sceneSVG(radius, outer, yaw, pitch float64) string {
	// Normalize to the outer turning radius so the entire geometry stays in frame.
	eye := ln.Vector{X: 4 * math.Cos(pitch) * math.Cos(yaw), Y: 4 * math.Cos(pitch) * math.Sin(yaw), Z: 4 * math.Sin(pitch)}
	render := func(r float64) string {
		var scene ln.Scene
		scene.Add(ln.NewSphere(ln.Vector{}, r))
		paths := scene.Render(eye, ln.Vector{}, ln.Vector{Z: 1}, 800, 600, 38, 0.1, 20, 0.03)
		var b strings.Builder
		for _, path := range paths {
			b.WriteString(path.ToSVG())
		}
		return b.String()
	}
	return fmt.Sprintf(`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 800 600" role="img" aria-label="Three dimensional nuclear radius and outer turning radius"><g transform="translate(0,600) scale(1,-1)"><g class="shell">%s</g><g class="core">%s</g></g></svg>`, render(1), render(radius/outer))
}
func handler(gateway, catalog string) http.Handler {
	mux := http.NewServeMux()
	target, err := url.Parse(gateway)
	if err != nil {
		panic(err)
	}
	proxy := httputil.NewSingleHostReverseProxy(target)
	proxy.ErrorHandler = func(w http.ResponseWriter, r *http.Request, err error) {
		http.Error(w, "Gateway unavailable", http.StatusBadGateway)
	}
	mux.Handle("/v1/barrier", proxy)
	mux.Handle("/v1/ensemble", proxy)
	mux.HandleFunc("/api/isotopes", func(w http.ResponseWriter, r *http.Request) {
		if r.Method != "GET" {
			http.Error(w, "method not allowed", 405)
			return
		}
		http.ServeFile(w, r, catalog)
	})
	mux.HandleFunc("/api/scene", func(w http.ResponseWriter, r *http.Request) {
		if r.Method != "GET" {
			http.Error(w, "method not allowed", 405)
			return
		}
		vals := make([]float64, 4)
		for i, key := range []string{"radius", "outer", "yaw", "pitch"} {
			v, e := strconv.ParseFloat(r.URL.Query().Get(key), 64)
			if e != nil || math.IsNaN(v) || math.IsInf(v, 0) {
				http.Error(w, "invalid geometry", 400)
				return
			}
			vals[i] = v
		}
		if vals[0] <= 0 || vals[1] < vals[0] || vals[1] > 100000 || math.Abs(vals[2]) > 10000 || math.Abs(vals[3]) > 1.4 {
			http.Error(w, "geometry out of range", 400)
			return
		}
		w.Header().Set("Content-Type", "image/svg+xml")
		io.WriteString(w, sceneSVG(vals[0], vals[1], vals[2], vals[3]))
	})
	mux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path == "/app.js" {
			http.ServeFileFS(w, r, assets, "app.js")
			return
		}
		if r.URL.Path != "/" {
			http.NotFound(w, r)
			return
		}
		http.ServeFileFS(w, r, assets, "index.html")
	})
	return mux
}
func main() {
	gateway := os.Getenv("GATEWAY_URL")
	if gateway == "" {
		gateway = "http://localhost:8080"
	}
	catalog := os.Getenv("CATALOG_PATH")
	if catalog == "" {
		catalog = "observatory/isotopes.json"
	}
	server := http.Server{Addr: ":8090", Handler: handler(gateway, catalog), ReadHeaderTimeout: 5 * time.Second}
	log.Fatal(server.ListenAndServe())
}
