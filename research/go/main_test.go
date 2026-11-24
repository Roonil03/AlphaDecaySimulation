package main

import (
	"bytes"
	"context"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func TestMain(m *testing.M) {
	if len(os.Args) > 1 && os.Args[1] == "--scheduler-cli" {
		os.Args = append([]string{os.Args[0]}, os.Args[2:]...)
		flag.CommandLine = flag.NewFlagSet("scheduler", flag.ExitOnError)
		main()
		os.Exit(0)
	}
	if len(os.Args) > 1 && os.Args[1] == "--run" {
		var req map[string]any
		if json.NewDecoder(os.Stdin).Decode(&req) != nil {
			os.Exit(3)
		}
		if size, ok := req["stdout_bytes"].(float64); ok {
			io.WriteString(os.Stdout, "{}"+strings.Repeat(" ", int(size)-2))
			os.Exit(0)
		}
		if size, ok := req["stderr_bytes"].(float64); ok {
			io.WriteString(os.Stderr, "{}"+strings.Repeat(" ", int(size)-2))
		}
		if req["sleep"] == true {
			time.Sleep(5 * time.Second)
		}
		if req["domain"] == true {
			os.Stderr.WriteString(`{"schema_version":1,"error":"invalid request"}`)
			os.Exit(1)
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

func TestManifestCLI(t *testing.T) {
	exe, _ := os.Executable()
	valid := `{"cases":[]}`
	fixtures := []struct {
		name, input string
		accepted    bool
	}{
		{"empty", valid, true},
		{"one", `{"cases":[{"id":"one","request":{}}]}`, true},
		{"absent", `{}`, false},
		{"null", `{"cases":null}`, false},
		{"second-document", valid + `{}`, false},
		{"trailing-garbage", valid + `junk`, false},
		{"oversized", valid + strings.Repeat(" ", manifestLimit-len(valid)+1), false},
	}
	for _, fromFile := range []bool{false, true} {
		for _, fixture := range fixtures {
			t.Run(fmt.Sprintf("%s/file=%t", fixture.name, fromFile), func(t *testing.T) {
				args := []string{"--scheduler-cli", "-core", exe}
				if fromFile {
					path := filepath.Join(t.TempDir(), "manifest.json")
					if err := os.WriteFile(path, []byte(fixture.input), 0600); err != nil {
						t.Fatal(err)
					}
					args = append(args, "-input", path)
				}
				cmd := exec.Command(exe, args...)
				if !fromFile {
					cmd.Stdin = strings.NewReader(fixture.input)
				}
				var stdout, stderr bytes.Buffer
				cmd.Stdout, cmd.Stderr = &stdout, &stderr
				err := cmd.Run()
				if fixture.accepted {
					if err != nil || !json.Valid(stdout.Bytes()) {
						t.Fatalf("valid manifest failed: %v %s", err, stderr.String())
					}
				} else if err == nil || stdout.Len() != 0 {
					t.Fatalf("invalid manifest succeeded: err=%v stdout=%s", err, stdout.String())
				}
			})
		}
	}
}

func TestReadCampaign(t *testing.T) {
	for _, input := range []string{`{"cases":[]}`, ` {"cases":[{"id":"a","request":{}}]} `} {
		if _, err := readCampaign(strings.NewReader(input)); err != nil {
			t.Fatalf("valid manifest rejected: %v", err)
		}
	}
	for _, input := range []string{`{}`, `null`, `{"cases":null}`, `{"cases":{}}`, `{"cases":[]} {}`, `{"cases":[]} garbage`} {
		if _, err := readCampaign(strings.NewReader(input)); err == nil {
			t.Fatalf("invalid manifest accepted: %s", input)
		}
	}
	valid := `{"cases":[]}`
	if _, err := readCampaign(strings.NewReader(valid + strings.Repeat(" ", manifestLimit-len(valid)))); err != nil {
		t.Fatalf("manifest at bound rejected: %v", err)
	}
	for _, suffix := range []string{" ", "{}"} {
		if _, err := readCampaign(strings.NewReader(valid + strings.Repeat(" ", manifestLimit-len(valid)) + suffix)); err == nil {
			t.Fatal("oversized manifest accepted")
		}
	}
}

func TestLimitedBufferCopy(t *testing.T) {
	// A bytes.Buffer ReaderFrom method must not bypass the bounded Write path.
	var output limitedBuffer
	input := io.LimitReader(strings.NewReader(strings.Repeat("x", streamLimit+1)), streamLimit+1)
	n, err := io.Copy(&output, input)
	if err == nil || n != streamLimit || len(output.Bytes()) != streamLimit || !output.exceeded {
		t.Fatalf("output limit bypassed: bytes=%d err=%v", n, err)
	}
}

func TestEachOutputStreamLimit(t *testing.T) {
	exe, _ := os.Executable()
	for _, stream := range []string{"stdout_bytes", "stderr_bytes"} {
		for _, size := range []int{streamLimit, streamLimit + 1} {
			request, _ := json.Marshal(map[string]int{stream: size})
			got := run(context.Background(), exe, Case{stream, request}, 10*time.Second)
			if size == streamLimit && got.Error != "" {
				t.Fatalf("%s at bound rejected: %+v", stream, got)
			}
			if size > streamLimit && (got.Error == "" || len(got.Response) != 0) {
				t.Fatalf("%s above bound accepted: %+v", stream, got)
			}
		}
	}
}
func TestExecuteOrderAndFailures(t *testing.T) {
	exe, _ := os.Executable()
	cases := []Case{{"a", json.RawMessage(`{"v":1}`)}, {"b", json.RawMessage(`{"fail":true}`)}, {"c", json.RawMessage(`{"sleep":true}`)}}
	got, err := execute(context.Background(), exe, cases, 2, 2*time.Second)
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

func TestDomainRejection(t *testing.T) {
	exe, _ := os.Executable()
	got := run(context.Background(), exe, Case{"bad", json.RawMessage(`{"domain":true}`)}, 10*time.Second)
	if got.ErrorKind != "domain" {
		t.Fatalf("unexpected result: %+v", got)
	}
}
