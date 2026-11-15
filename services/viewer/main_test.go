package main

import (
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"strings"
	"testing"
)

func TestSceneAndValidation(t *testing.T) {
	h := handler("http://localhost:1", "missing")
	for _, tc := range []struct {
		query  string
		status int
	}{{"radius=9&outer=30&yaw=.5&pitch=.4", 200}, {"radius=9&outer=8&yaw=0&pitch=0", 400}, {"radius=NaN&outer=30&yaw=0&pitch=0", 400}} {
		w := httptest.NewRecorder()
		h.ServeHTTP(w, httptest.NewRequest("GET", "/api/scene?"+tc.query, nil))
		if w.Code != tc.status {
			t.Fatalf("%s: %d", tc.query, w.Code)
		}
		if tc.status == 200 && (!strings.Contains(w.Body.String(), "<polyline") || !strings.Contains(w.Body.String(), `class="core"`)) {
			t.Fatal("missing projected geometry")
		}
	}
	a := sceneSVG(9, 30, 0, .4)
	b := sceneSVG(9, 30, 1, .4)
	if a == b {
		t.Fatal("camera did not change projection")
	}
}
func TestCatalogAndProxy(t *testing.T) {
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/v1/barrier" || r.Method != "POST" {
			t.Error("unexpected proxy request")
		}
		io.WriteString(w, `{"outer_turning_fm":30}`)
	}))
	defer upstream.Close()
	file, err := os.CreateTemp(t.TempDir(), "catalog-*.json")
	if err != nil {
		t.Fatal(err)
	}
	file.WriteString(`[{"symbol":"Po","z":84,"a":212,"q_mev":8.9542}]`)
	file.Close()
	h := handler(upstream.URL, file.Name())
	w := httptest.NewRecorder()
	h.ServeHTTP(w, httptest.NewRequest("GET", "/api/isotopes", nil))
	if w.Code != 200 || !strings.Contains(w.Body.String(), "Po") {
		t.Fatal("catalog not served")
	}
	w = httptest.NewRecorder()
	h.ServeHTTP(w, httptest.NewRequest("POST", "/v1/barrier", strings.NewReader(`{}`)))
	if w.Code != 200 || !strings.Contains(w.Body.String(), "outer_turning_fm") {
		t.Fatal("gateway not proxied")
	}
}

func TestEmbeddedClient(t *testing.T) {
	h := handler("http://localhost:1", "missing")
	for _, tc := range []struct{ path, content string }{{"/", `src="/app.js"`}, {"/app.js", "function measuredComparison"}} {
		w := httptest.NewRecorder()
		h.ServeHTTP(w, httptest.NewRequest("GET", tc.path, nil))
		if w.Code != 200 || !strings.Contains(w.Body.String(), tc.content) {
			t.Fatalf("client asset %s not served: %d", tc.path, w.Code)
		}
	}
}
