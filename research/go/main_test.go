package main

import (
	"context"
	"encoding/json"
	"os"
	"testing"
	"time"
)

func TestMain(m *testing.M) {
	if len(os.Args) > 1 && os.Args[1] == "--run" {
		var req map[string]any
		if json.NewDecoder(os.Stdin).Decode(&req) != nil {
			os.Exit(3)
		}
		if req["sleep"] == true {
			time.Sleep(time.Second)
		}
		if req["fail"] == true {
			os.Stderr.WriteString("bad request")
			os.Exit(4)
		}
		json.NewEncoder(os.Stdout).Encode(req)
		os.Exit(0)
	}
	os.Exit(m.Run())
}
func TestExecuteOrderAndFailures(t *testing.T) {
	exe, _ := os.Executable()
	cases := []Case{{"a", json.RawMessage(`{"v":1}`)}, {"b", json.RawMessage(`{"fail":true}`)}, {"c", json.RawMessage(`{"sleep":true}`)}}
	got, err := execute(context.Background(), exe, cases, 2, 100*time.Millisecond)
	if err != nil {
		t.Fatal(err)
	}
	if got[0].ID != "a" || !json.Valid(got[0].Response) || got[1].Error == "" || got[2].Error != "context deadline exceeded" {
		t.Fatalf("unexpected results: %+v", got)
	}
}
func TestDuplicateIDs(t *testing.T) {
	_, err := execute(context.Background(), "unused", []Case{{ID: "a"}, {ID: "a"}}, 1, time.Second)
	if err == nil {
		t.Fatal("duplicate accepted")
	}
}
func TestCancellation(t *testing.T) {
	exe, _ := os.Executable()
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	got, err := execute(ctx, exe, []Case{{"a", json.RawMessage(`{}`)}}, 1, time.Second)
	if err != nil || got[0].Error != "context canceled" {
		t.Fatalf("%+v %v", got, err)
	}
}
