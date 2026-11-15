# Gateway and ensemble

The gateway listens on port 8080. `GET /health` checks the gateway process. `POST /v1/barrier` forwards JSON to `KERNEL_URL` (default `http://kernel:8081`); `POST /v1/ensemble` forwards to `ENSEMBLE_URL` (default `http://ensemble:8082`). Responses and status codes are passed through. Request and upstream response bodies are limited to 1 MiB, and upstream requests time out after 15 seconds.

The ensemble service listens on port 8082 and exposes `GET /health` and `POST /v1/ensemble`. The request contains numeric `log10_half_life_s` (absolute value at most 1,000,000), `times_s` (up to 1,000 nonnegative numbers, each at most 1e300), integer `population` (1–100,000), and signed 64-bit integer `seed`. The response contains `expected_survivors` and `sampled_survivors` in input order. Samples use one seeded independent lifetime per member, so survivors at later times are a subset of earlier survivors. Repeated times agree. Up to 32 simulations run concurrently, each with a 10-second deadline; oversized requests receive 413 and excess work receives 503. Results are deterministic for the same request and runtime version.

Run gateway tests with `cd services/gateway && go test ./...`. For the ensemble, use Elixir 1.16 or newer and run `cd services/ensemble && mix deps.get && mix test`.

Both Docker builds run their language-specific test suites before producing a runnable image. The ensemble dependency graph is pinned by the resolver-generated `mix.lock` copied before fetching dependencies.
