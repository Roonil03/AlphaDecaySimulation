// A benign integration fixture: announce its process IDs, wait, then return JSON.
package main

import (
	"encoding/json"
	"os"
	"time"
)

func main() {
	var request struct {
		Started  string `json:"started"`
		Release  string `json:"release"`
		Finished string `json:"finished"`
	}
	if err := json.NewDecoder(os.Stdin).Decode(&request); err != nil {
		os.Exit(2)
	}
	pids, _ := json.Marshal(map[string]int{"core": os.Getpid(), "scheduler": os.Getppid()})
	if err := os.WriteFile(request.Started, pids, 0600); err != nil {
		os.Exit(3)
	}
	deadline := time.Now().Add(90 * time.Second)
	for {
		if _, err := os.Stat(request.Release); err == nil {
			break
		}
		if time.Now().After(deadline) {
			os.Exit(4)
		}
		time.Sleep(25 * time.Millisecond)
	}
	if err := os.WriteFile(request.Finished, []byte("finished"), 0600); err != nil {
		os.Exit(5)
	}
	json.NewEncoder(os.Stdout).Encode(map[string]any{"schema_version": 1, "fixture": true})
}
