const $ = (id) => document.getElementById(id);
const fractions = [0, 0.5, 1, 1.5, 2];
const modelNames = { wkb: "WKB", research: "Qi UDL", shell: "Ren shell" };
const halfLifeLabel = (n) => `${n} ${n === 1 ? "half-life" : "half-lives"}`;
let isotopes = [], yaw = 0.75, pitch = 0.45, last = null;
let requestID = 0, geometry = null, drawID = 0, drawPending = false;
let population = null, step = 0, timer = null;

async function json(path, body) {
  const r = await fetch(path, body ? {
    method: "POST", headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  } : {});
  if (!r.ok) throw Error(`HTTP ${r.status}: ${(await r.text()).slice(0, 250)}`);
  return r.json();
}

function number(value) {
  return Number(value.toPrecision(4)).toString();
}
function duration(logSeconds) {
  const units = [[Math.log10(31557600), "y"], [Math.log10(86400), "d"],
    [Math.log10(3600), "h"], [Math.log10(60), "min"], [0, "s"],
    [-3, "ms"], [-6, "μs"], [-9, "ns"], [-12, "ps"],
    [-15, "fs"], [-18, "as"], [-21, "zs"], [-24, "ys"]];
  if (!Number.isFinite(logSeconds)) return "Unavailable";
  for (const [scale, unit] of units) {
    if (logSeconds >= scale) {
      const value = 10 ** (logSeconds - scale);
      return Number.isFinite(value) ? `${number(value)} ${unit}` : `10^${number(logSeconds)} s`;
    }
  }
  return `10^${number(logSeconds)} s`;
}
function measuredComparison(logHalf, measured) {
  const delta = logHalf - Math.log10(measured);
  const ratio = 10 ** delta;
  const predicted = 10 ** logHalf;
  if (Number.isFinite(predicted) && predicted > 0 && Number.isFinite(ratio) && ratio > 0) {
    const difference = Math.abs(predicted - measured);
    if (difference / measured < 0.00005) return "Matches the measured partial half-life within 0.01%.";
    return `${duration(Math.log10(difference))} ${delta > 0 ? "longer" : "shorter"} than measured ` +
      `(${delta > 0 ? "+" : "−"}${number(Math.abs(ratio - 1) * 100)}%; ${number(ratio)}× measured).`;
  }
  return `Prediction / measured partial half-life = 10^${number(delta)}.`;
}

function stopPlayback() {
  if (timer !== null) clearInterval(timer);
  timer = null;
  $("play").textContent = "Play";
}
function clearPopulation(message = "Run the model to view population changes.") {
  stopPlayback();
  population = null;
  step = 0;
  const canvas = $("population");
  canvas.getContext("2d").clearRect(0, 0, canvas.width, canvas.height);
  $("step-label").textContent = message;
  $("survival-text").textContent = "Waiting for model output.";
  $("bars").replaceChildren();
  for (const key of ["previous", "play", "next"]) $(key).disabled = true;
}
function clearResults(title) {
  geometry = null;
  ++drawID;
  $("scene").replaceChildren();
  $("scene-title").textContent = title;
  for (const key of ["q", "outer", "half", "measured"]) $(key).textContent = "—";
  $("selected-name").textContent = "Selected model half-life";
  for (const key of ["comparison", "model-estimates", "reference", "scope"]) $(key).replaceChildren();
  clearPopulation();
}
function invalidate(title = "Choose inputs and calculate the model") {
  ++requestID;
  clearResults(title);
  $("error").textContent = "";
  $("run").disabled = false;
}

async function draw() {
  ++drawID;
  if (!geometry || drawPending) return;
  drawPending = true;
  try {
    while (geometry) {
      const version = drawID;
      try {
        const query = new URLSearchParams({ ...geometry, yaw, pitch });
        const r = await fetch("/api/scene?" + query);
        if (!r.ok) throw Error(await r.text());
        const svg = await r.text();
        if (version === drawID && geometry) {
          $("scene").innerHTML = svg;
          break;
        }
      } catch (error) {
        if (version === drawID && geometry) {
          $("error").textContent = "Geometry unavailable: " + error.message;
          break;
        }
        // A stale scene failure must not overwrite a newer selection or error.
      }
    }
  } finally {
    drawPending = false;
  }
}

function showStep() {
  if (!population) return;
  step = Math.max(0, Math.min(population.times.length - 1, step));
  const canvas = $("population"), ctx = canvas.getContext("2d");
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  const alive = Math.round(64 * population.sampled_survivors[step] / 1000), points = [];
  for (let i = 0; i < 64; i++) {
    const x = i % 4 - 1.5, y = Math.floor(i / 4) % 4 - 1.5, z = Math.floor(i / 16) - 1.5;
    points.push({ x: 130 + (x - y) * 25, y: 85 + (x + y) * 10 - z * 22,
      depth: x + y + z, alive: (i * 37) % 64 < alive });
  }
  points.sort((a, b) => a.depth - b.depth);
  for (const p of points) {
    ctx.beginPath(); ctx.arc(p.x, p.y, 5, 0, 2 * Math.PI);
    ctx.fillStyle = p.alive ? "#8ef0d3" : "#405168";
    ctx.fill();
  }
  const time = population.times[step];
  $("step-label").textContent = `t = ${time === 0 ? "0 s" : duration(Math.log10(time))} · ` +
    `${population.sampled_survivors[step]} of 1,000 sampled survivors` +
    (population.capped ? " (capped display times)" : ` (${halfLifeLabel(fractions[step])})`);
  if (step === population.times.length - 1) stopPlayback();
}
$("previous").onclick = () => { if (population) { --step; showStep(); } };
$("next").onclick = () => { if (population) { ++step; showStep(); } };
$("play").onclick = () => {
  if (!population) return;
  if (timer !== null) { stopPlayback(); return; }
  step = 0;
  showStep();
  $("play").textContent = "Pause";
  timer = setInterval(() => {
    if (!population) { stopPlayback(); return; }
    step = Math.min(population.times.length - 1, step + 1);
    showStep();
  }, 800);
};

function showReference(iso, half) {
  $("measured").textContent = Number.isFinite(iso.partial_half_life_s)
    ? duration(Math.log10(iso.partial_half_life_s)) : "No reference";
  if (Number.isFinite(iso.partial_half_life_s) && Number.isFinite(half))
    $("comparison").textContent = measuredComparison(half, iso.partial_half_life_s);
  if (iso.source_url) {
    const source = document.createElement("a");
    source.href = iso.source_url;
    source.target = "_blank";
    source.rel = "noopener noreferrer";
    source.textContent = "Measured half-life source";
    $("reference").append(source);
    if (iso.branch_source_url) {
      const branch = document.createElement("a");
      branch.href = iso.branch_source_url;
      branch.target = "_blank";
      branch.rel = "noopener noreferrer";
      branch.textContent = " · Ground-state branch source";
      $("reference").append(branch);
    }
  }
  $("scope").textContent = "Qi and Ren fits apply to even-even ℓ=0 channels; published-fit overlap means this comparison is not independent validation. " +
    (iso.z < 78 || iso.z > 108 ? "Qi extrapolation: Z is outside the fitted 78–108 range. " : "") +
    (iso.binding_extrapolated ? "The catalog binding energy is extrapolated. " : "") +
    (!iso.source_url ? "Custom inputs have no measured reference here." : "Measured partial half-life includes the ground-state branching correction.");
}

async function run() {
  invalidate("Calculating…");
  const id = requestID;
  $("run").disabled = true;
  try {
    const custom = $("isotope").value === "custom";
    const iso = custom ? {
      symbol: "Z=" + $("z").value, z: Number($("z").value), a: Number($("a").value),
      q_mev: Number($("energy").value),
      parent_binding_mev: $("binding").value === "" ? null : Number($("binding").value),
    } : isotopes[Number($("isotope").value)];
    if (!iso || !Number.isInteger(iso.z) || !Number.isInteger(iso.a) ||
        iso.z < 3 || iso.z > 130 || iso.a < 5 || iso.a > 400 || iso.a - iso.z < 2 ||
        !Number.isFinite(iso.q_mev) || iso.q_mev < .01 || iso.q_mev > 100 ||
        (iso.parent_binding_mev != null && (!Number.isFinite(iso.parent_binding_mev) ||
          iso.parent_binding_mev <= 0 || iso.parent_binding_mev > 10000)))
      throw Error("Use integer Z (3–130), A (5–400), A−Z ≥ 2, Q in [0.01, 100] MeV and optional binding in (0, 10000] MeV.");
    const selected = $("model").value;
    const input = { parent_z: iso.z, parent_a: iso.a, q_mev: iso.q_mev,
      l: 0, radius_fm: 9, preformation: .1, assault_hz: 1e21 };
    if (selected !== "wkb") {
      input.research_model = true;
      if (iso.parent_binding_mev != null) input.parent_binding_mev = iso.parent_binding_mev;
    }
    // Unsupported model fields are omitted by the kernel. HTTP/network failures are real errors.
    const result = await json("/v1/barrier", input);
    if (id !== requestID) return;
    const half = result[selected === "research" ? "research_log10_half_life_s" :
      selected === "shell" ? "shell_log10_half_life_s" : "log10_half_life_s"];
    if (!Number.isFinite(half)) throw Error("Selected prediction unavailable for this isotope. Qi/Ren require even-even ℓ=0 inputs; Ren also needs binding energy. Select WKB or supply supported inputs.");
    if (!Number.isFinite(result.outer_turning_fm) || result.outer_turning_fm <= 0)
      throw Error("The backend returned an invalid turning radius.");
    $("scene-title").textContent = `${iso.symbol}-${iso.a}`;
    $("q").textContent = iso.q_mev + " MeV";
    $("outer").textContent = result.outer_turning_fm.toFixed(2) + " fm";
    $("selected-name").textContent = `${modelNames[selected]} half-life`;
    $("half").textContent = duration(half);
    $("model-estimates").textContent = [["WKB", result.log10_half_life_s],
      ["Qi UDL", result.research_log10_half_life_s], ["Ren shell", result.shell_log10_half_life_s]]
      .filter(([, value]) => Number.isFinite(value)).map(([name, value]) => `${name}: ${duration(value)}`).join(" · ");
    showReference(iso, half);
    if (result.outer_turning_fm <= 9) {
      $("scene-title").textContent += " · No exterior barrier";
      const note = document.createElement("p");
      note.style.cssText = "padding:150px 28px 100px;max-width:600px";
      note.textContent = "The outer turning radius is at or inside the assumed 9 fm nuclear radius. The exterior WKB barrier has zero width (P = 1); the half-life and population below remain available.";
      $("scene").append(note);
    } else {
      geometry = { radius: 9, outer: result.outer_turning_fm };
      draw();
    }
    const capped = half < -300 || half > 299;
    const h = 10 ** Math.max(-300, Math.min(299, half));
    const times = fractions.map((x) => x * h);
    const pop = await json("/v1/ensemble", { log10_half_life_s: half,
      times_s: times, population: 1000, seed: 42 });
    if (id !== requestID) return;
    population = { ...pop, times, capped };
    step = 0;
    for (const key of ["previous", "play", "next"]) $(key).disabled = false;
    showStep();
    $("survival-text").textContent = capped
      ? `${modelNames[selected]} survival at capped display times; the half-life exceeds the ensemble's time range.`
      : `${modelNames[selected]} half-life · expected ${pop.expected_survivors[2].toFixed(0)} survivors at one half-life; sampled ${pop.sampled_survivors[2]}.`;
    $("bars").replaceChildren(...pop.sampled_survivors.map((n, i) => {
      const bar = document.createElement("div");
      bar.className = "bar";
      bar.style.height = n / 10 + "%";
      bar.title = `${capped ? (times[i] === 0 ? "0 s" : duration(Math.log10(times[i]))) : halfLifeLabel(fractions[i])}: ${n} sampled survivors`;
      return bar;
    }));
  } catch (error) {
    if (id === requestID) {
      clearResults("Model unavailable");
      $("error").textContent = error.message;
      $("step-label").textContent = "Population result unavailable.";
      $("survival-text").textContent = "Population result unavailable.";
    }
  } finally {
    if (id === requestID) $("run").disabled = false;
  }
}

$("run").onclick = run;
$("isotope").onchange = () => {
  const custom = $("isotope").value === "custom";
  $("custom").hidden = !custom;
  if (custom) invalidate("Custom isotope · enter inputs and calculate");
  else return run();
};
for (const key of ["z", "a", "energy", "binding"])
  $(key).oninput = () => invalidate("Custom inputs changed · calculate to update");
$("model").onchange = run;
$("reset").onclick = () => { yaw = .75; pitch = .45; draw(); };
$("top").onclick = () => { yaw = 0; pitch = 1.35; draw(); };
$("stage").onpointerdown = (e) => {
  if (e.target.tagName === "BUTTON") return;
  last = [e.clientX, e.clientY];
  $("stage").setPointerCapture(e.pointerId);
};
$("stage").onpointermove = (e) => {
  if (!last) return;
  yaw += (e.clientX - last[0]) * .01;
  pitch = Math.max(-1.35, Math.min(1.35, pitch + (e.clientY - last[1]) * .01));
  last = [e.clientX, e.clientY];
  draw();
};
$("stage").onpointerup = () => { last = null; };
$("stage").onpointercancel = () => { last = null; };
clearPopulation();
(async () => {
  try {
    isotopes = await json("/api/isotopes");
    if (!Array.isArray(isotopes) || !isotopes.length) throw Error("Isotope catalog is empty.");
    isotopes.forEach((x, i) => {
      const option = document.createElement("option");
      option.value = i;
      option.textContent = `${x.symbol}-${x.a}`;
      $("isotope").append(option);
    });
    const option = document.createElement("option");
    option.value = "custom"; option.textContent = "Custom isotope";
    $("isotope").append(option);
    const po = isotopes.findIndex((x) => x.symbol === "Po" && x.a === 212);
    $("isotope").value = po >= 0 ? String(po) : "0";
    await run();
  } catch (error) {
    $("error").textContent = "Could not load isotope catalog: " + error.message;
  }
})();
