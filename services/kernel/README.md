# Alpha barrier kernel

A deliberately simplified spherical external-barrier estimate, not a fitted nuclear-decay prediction. Build with `docker build -t alpha-kernel services/kernel` from the repository root; the image runs the release tests during build. Run with `docker run --rm -p 8081:8081 alpha-kernel`. `GET /health` returns `ok`.

`POST /v1/barrier` accepts JSON with all fields required, for example:

```json
{"parent_z":92,"parent_a":238,"q_mev":4.27,"l":0,"radius_fm":9.0,"preformation":0.1,"assault_hz":1e21}
```

The response contains `log10_half_life_s`, `log10_penetrability`, and `outer_turning_fm`, all JSON numbers. Malformed/missing/unknown fields return a 4xx response; unsupported numeric values return 422. Integers Z=3..130, A=5..400, A-Z>=2 and l=0..100 are supported; Q=0.01..100 MeV, radius=0.1..100 fm, 0<preformation<=1 and 1e-300<=assault_hz<=1e30. These bounds make arithmetic finite; they do not certify physical applicability. All numeric inputs must be finite.

## Model

For the daughter (Z-2, A-4), use reduced mass energy `mu = 931.49410242 * 4*(A-4)/A` MeV, `hbar*c = 197.3269804` MeV fm, and `e²/(4 pi epsilon0) = 1.43996448` MeV fm. For r in fm, `V(r)=C/r+D/r²` MeV, with `C=2*(Z-2)*1.43996448` and `D=(hbar*c)²*l*(l+1)/(2*mu)`. The positive outer turning point is `b=(C+sqrt(C²+4*Q*D))/(2*Q)`. The supplied radius R is the inner boundary; no nuclear potential or inner turning point is calculated. For R<b, `S=integral_R^b sqrt(2*mu*(V-Q))/(hbar*c) dr`, `log10(P)=-2*S/ln(10)` and `log10(T_1/2/s)=log10(ln(2))-log10(preformation)-log10(assault_hz)-log10(P)`. If R>=b there is no forbidden external interval and P=1. Values remain in logarithmic form to avoid underflow.

C implements the potential; C++ uses 4096-panel composite Simpson integration after `r=R exp(ln(b/R)(2t-t²))` to smooth the outer endpoint. On x86-64 the quadrature square-root hot path uses SSE2 `sqrtsd`; elsewhere it uses `std::sqrt`. Tests compare both paths. The model uses l(l+1), without the Langer correction. Approximate mass numbers, point charges, a sharp specified boundary, fixed preformation/assault rate, and omitted nuclear, deformation and screening effects limit its use; it does not infer decay energies or guarantee experimental half-lives. The test suite checks the extreme Coulomb case analytically and compares 4096- and 8192-panel results for an extreme centrifugal case.

The WKB penetrability and centrifugal convention are consistent with [CPC WKB treatment](https://cpc.ihep.ac.cn/article/doi/10.1088/1674-1137/ae0307) and [centrifugal WKB expression](https://www.sciencedirect.com/science/article/pii/S0375947426001545). The test compares the l=0 action to the exact Coulomb integral `sqrt(2*mu*Q)*b/(hbar*c) * [acos(sqrt(R/b))-sqrt((R/b)*(1-R/b))]`.

Run tests locally with `cargo test --release`. A repeatable single-thread numerical timing (no HTTP) is `cargo test --release benchmark -- --ignored --nocapture`; it evaluates the U-238 example above 10,000 times. Report architecture, CPU, compiler and elapsed time with results. For Docker, use the build stage: `docker build --target build -t alpha-kernel-build services/kernel`, then `docker run --rm alpha-kernel-build cargo test --release benchmark -- --ignored --nocapture`.


Recorded benchmark: 10,000 calls in 1,240 ms (124,000 ns/call), Linux x86-64 Docker Desktop on an 11th Gen Intel Core i9-11900H @ 2.50 GHz, Rust 1.85.1, release profile. This is one run, not an HTTP throughput measurement or a speedup comparison.
