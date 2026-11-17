// Command scheduler executes independent native-core requests with bounded concurrency.
package main

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"os"
	"os/exec"
	"os/signal"
	"sync"
	"time"
)

type Case struct {
	ID      string          `json:"id"`
	Request json.RawMessage `json:"request"`
}
type Campaign struct {
	Cases []Case `json:"cases"`
}
type Result struct {
	ID        string          `json:"id"`
	Response  json.RawMessage `json:"response,omitempty"`
	Error     string          `json:"error,omitempty"`
	ErrorKind string          `json:"error_kind,omitempty"`
}

func run(ctx context.Context, executable string, c Case, timeout time.Duration) Result {
	r := Result{ID: c.ID}
	if c.ID == "" || !json.Valid(c.Request) {
		r.Error = "case requires an id and valid JSON request"
		return r
	}
	ctx, cancel := context.WithTimeout(ctx, timeout)
	defer cancel()
	cmd := exec.CommandContext(ctx, executable, "--run")
	cmd.Stdin = bytes.NewReader(c.Request)
	var stdout, stderr limitedBuffer
	cmd.Stdout, cmd.Stderr = &stdout, &stderr
	err := cmd.Run()
	out := stdout.Bytes()
	if ctx.Err() != nil {
		r.Error = ctx.Err().Error()
		return r
	}
	if err != nil {
		var exit *exec.ExitError
		if errors.As(err, &exit) {
			r.Error = fmt.Sprintf("native core failed: %s", stderr.Bytes())
			var nativeError struct {
				Error string `json:"error"`
			}
			if json.Unmarshal(stderr.Bytes(), &nativeError) == nil && nativeError.Error != "" {
				r.ErrorKind = "domain"
			}
		} else {
			r.Error = err.Error()
		}
		return r
	}
	if !json.Valid(out) {
		r.Error = "native core returned invalid JSON"
		return r
	}
	r.Response = json.RawMessage(out)
	return r
}

// limitedBuffer prevents a faulty child from exhausting scheduler memory.
type limitedBuffer struct{ bytes.Buffer }

func (b *limitedBuffer) Write(p []byte) (int, error) {
	if b.Len()+len(p) > 1<<20 {
		return 0, errors.New("native output exceeds 1 MiB")
	}
	return b.Buffer.Write(p)
}

func execute(ctx context.Context, executable string, cases []Case, workers int, timeout time.Duration) ([]Result, error) {
	if workers < 1 || timeout <= 0 {
		return nil, errors.New("workers and timeout must be positive")
	}
	seen := map[string]bool{}
	for _, c := range cases {
		if c.ID == "" || seen[c.ID] {
			return nil, fmt.Errorf("empty or duplicate case id %q", c.ID)
		}
		seen[c.ID] = true
	}
	results := make([]Result, len(cases))
	jobs := make(chan int)
	var wg sync.WaitGroup
	for w := 0; w < workers; w++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for i := range jobs {
				results[i] = run(ctx, executable, cases[i], timeout)
			}
		}()
	}
	for i := range cases {
		jobs <- i
	}
	close(jobs)
	wg.Wait()
	return results, nil
}
func main() {
	executable := flag.String("core", "", "path to native executable")
	workers := flag.Int("workers", 1, "maximum concurrent core processes")
	inputPath := flag.String("input", "", "campaign JSON file (default stdin)")
	timeout := flag.Duration("timeout", 30*time.Second, "per-case timeout")
	flag.Parse()
	if *executable == "" {
		fmt.Fprintln(os.Stderr, "-core is required")
		os.Exit(2)
	}
	var c Campaign
	var input io.Reader = os.Stdin
	if *inputPath != "" {
		file, err := os.Open(*inputPath)
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(2)
		}
		defer file.Close()
		input = file
	}
	dec := json.NewDecoder(io.LimitReader(input, 16<<20))
	if err := dec.Decode(&c); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(2)
	}
	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt)
	defer stop()
	results, err := execute(ctx, *executable, c.Cases, *workers, *timeout)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(2)
	}
	if err = json.NewEncoder(os.Stdout).Encode(results); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}
