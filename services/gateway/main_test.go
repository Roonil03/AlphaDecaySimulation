package main

import (
	"io"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
)

func TestRoutingAndValidation(t *testing.T) {
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/v1/ensemble" || r.Header.Get("Content-Type") != "application/json" {
			t.Error("wrong upstream request")
		}
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(422)
		io.WriteString(w, `{"error":"bad"}`)
	}))
	defer upstream.Close()
	h := handler(upstream.URL, upstream.URL, upstream.Client())
	for _, tc := range []struct {
		method, path, body string
		status             int
	}{{"GET", "/health", "", 200}, {"GET", "/v1/ensemble", "", 405}, {"POST", "/v1/ensemble", "bad", 400}, {"POST", "/v1/ensemble", `{}`, 422}, {"POST", "/v1/barrier", strings.Repeat(" ", maxBody+1), 413}} {
		w := httptest.NewRecorder()
		h.ServeHTTP(w, httptest.NewRequest(tc.method, tc.path, strings.NewReader(tc.body)))
		if w.Code != tc.status {
			t.Errorf("%s %s: got %d want %d", tc.method, tc.path, w.Code, tc.status)
		}
	}
}
