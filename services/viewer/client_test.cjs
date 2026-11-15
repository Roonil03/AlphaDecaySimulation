const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const source = fs.readFileSync(path.join(__dirname, 'app.js'), 'utf8');
const catalog = [{ symbol: 'Po', z: 84, a: 212, q_mev: 8.9542,
  parent_binding_mev: 1655.7716856, partial_half_life_s: 295.1e-9,
  source_url: 'https://example.org/measurement', branch_source_url: 'https://example.org/branch' },
  { symbol: 'Sm', z: 62, a: 146, q_mev: 2.52876, parent_binding_mev: 1210.9,
    partial_half_life_s: 2.9e15, source_url: 'https://example.org/sm' }];
const barrier = { outer_turning_fm: 26.37, log10_half_life_s: -6.2,
  research_log10_half_life_s: -6.630368823101762, shell_log10_half_life_s: -6.500350233085122 };
const response = (data, status = 200) => ({ ok: status >= 200 && status < 300, status,
  json: async () => data, text: async () => typeof data === 'string' ? data : JSON.stringify(data) });
const deferred = () => {
  let resolve;
  const promise = new Promise((done) => { resolve = done; });
  return { promise, resolve };
};

function harness(override = () => undefined) {
  const elements = new Map(), calls = [], timers = new Map(), operations = [];
  let timerID = 0;
  function element(id = '') {
    if (id && elements.has(id)) return elements.get(id);
    let text = '';
    const el = { value: '', children: [], style: {}, width: 260, height: 170, disabled: false,
      append(...items) { this.children.push(...items); },
      replaceChildren(...items) { this.children = items; text = ''; },
      getContext() { return { clearRect() { operations.push('clear'); }, beginPath() {}, arc() {},
        fill() { operations.push('paint'); } }; },
      setPointerCapture() {},
      get textContent() { return text; },
      set textContent(value) { text = String(value); this.children = []; },
    };
    if (id) elements.set(id, el);
    return el;
  }
  Object.assign(element('model'), { value: 'wkb' });
  for (const [key, value] of Object.entries({ z: '84', a: '212', energy: '8.9542', binding: '' }))
    element(key).value = value;
  const context = vm.createContext({ document: { getElementById: element, createElement: () => element() },
    URLSearchParams, console,
    setInterval(fn) { timers.set(++timerID, fn); return timerID; },
    clearInterval(id) { timers.delete(id); },
    async fetch(url, options = {}) {
      const body = options.body ? JSON.parse(options.body) : undefined;
      calls.push({ url, body });
      const replacement = override(url, body);
      if (replacement !== undefined) return replacement;
      if (url === '/api/isotopes') return response(catalog);
      if (url === '/v1/barrier') return response(barrier);
      if (url === '/v1/ensemble') return response({ sampled_survivors: [1000, 707, 500, 354, 250],
        expected_survivors: [1000, 707.107, 500, 353.553, 250] });
      if (url.startsWith('/api/scene?')) return response('<svg>current scene</svg>');
      throw Error('Unexpected fetch: ' + url);
    },
  });
  vm.runInContext(source, context, { filename: 'app.js' });
  return { el: element, calls, timers, operations,
    async settle() { for (let i = 0; i < 12; ++i) await new Promise(setImmediate); },
    select(id, value) { element(id).value = value; return element(id).onchange(); },
    click(id) { return element(id).onclick(); },
    tick() { for (const fn of [...timers.values()]) fn(); },
  };
}

test('manual Next during playback reaches the final frame and stops safely', async () => {
  const h = harness();
  await h.settle();
  h.click('play');
  const queuedTick = [...h.timers.values()][0];
  for (let i = 0; i < 9; ++i) h.click('next');
  queuedTick(); // A callback already queued by the browser must remain bounded.
  assert.match(h.el('step-label').textContent, /250 of 1,000.*2 half-lives/);
  assert.equal(h.timers.size, 0);
  assert.equal(h.el('play').textContent, 'Play');
  h.click('previous');
  h.click('previous');
  assert.match(h.el('step-label').textContent, /\(1 half-life\)$/);
  for (let i = 0; i < 9; ++i) h.click('previous');
  assert.match(h.el('step-label').textContent, /1000 of 1,000.*0 half-lives/);
});

test('changing to custom invalidates a pending calculation and clears population', async () => {
  const pending = deferred();
  let barrierCalls = 0;
  const h = harness((url) => url === '/v1/barrier' && ++barrierCalls === 2 ? pending.promise : undefined);
  await h.settle();
  const calculating = h.select('model', 'shell');
  h.select('isotope', 'custom');
  assert.equal(h.operations.at(-1), 'clear');
  assert.equal(h.el('play').disabled, true);
  h.click('play');
  assert.equal(h.timers.size, 0);
  pending.resolve(response(barrier));
  await calculating;
  await h.settle();
  assert.equal(h.el('half').textContent, '—');
  assert.match(h.el('scene-title').textContent, /Custom isotope/);
  assert.equal(h.el('bars').children.length, 0);
  assert.equal(h.operations.at(-1), 'clear');
});

test('backend failure on model change clears stale data and preserves the error', async () => {
  const h = harness((url, body) => url === '/v1/barrier' && body.research_model
    ? response('backend calculation failed', 503) : undefined);
  await h.settle();
  h.click('play');
  await h.select('model', 'shell');
  assert.match(h.el('error').textContent, /HTTP 503: backend calculation failed/);
  assert.equal(h.el('half').textContent, '—');
  assert.equal(h.el('bars').children.length, 0);
  assert.equal(h.operations.at(-1), 'clear');
  assert.equal(h.el('play').disabled, true);
  assert.equal(h.timers.size, 0);
});

test('a stale scene error cannot overwrite a new selection', async () => {
  const scene = deferred();
  const h = harness((url) => url.startsWith('/api/scene?') ? scene.promise : undefined);
  await h.settle();
  h.select('isotope', 'custom');
  scene.resolve(response('old scene failed', 500));
  await h.settle();
  assert.equal(h.el('error').textContent, '');
  assert.match(h.el('scene-title').textContent, /Custom isotope/);
});

test('Ren reports selected and measured half-lives, a physical discrepancy and references', async () => {
  const h = harness();
  await h.settle();
  await h.select('model', 'shell');
  assert.equal(h.el('selected-name').textContent, 'Ren shell half-life');
  assert.match(h.el('half').textContent, /316.*ns/);
  assert.equal(h.el('measured').textContent, '295.1 ns');
  assert.match(h.el('comparison').textContent, /20\.8\d ns longer.*\+7\./);
  assert.equal(h.el('reference').children[0].href, catalog[0].source_url);
  assert.match(h.el('model-estimates').textContent, /Ren shell: 316.*ns/);
  await h.select('isotope', '1');
  assert.match(h.el('scope').textContent, /Qi extrapolation.*78–108/);
});

test('no exterior barrier still calculates survival and explains the geometry', async () => {
  const h = harness((url, body) => url === '/v1/barrier' && body.q_mev === 50
    ? response({ ...barrier, outer_turning_fm: 4.72, log10_half_life_s: -20.1591745389 }) : undefined);
  await h.settle();
  h.select('isotope', 'custom');
  h.el('energy').value = '50';
  const before = h.calls.length;
  await h.click('run');
  assert.match(h.el('scene-title').textContent, /No exterior barrier/);
  assert.match(h.el('scene').children[0].textContent, /zero width \(P = 1\)/);
  assert.equal(h.el('play').disabled, false);
  assert.equal(h.el('bars').children.length, 5);
  assert.equal(h.el('error').textContent, '');
  assert(h.calls.slice(before).some((call) => call.url === '/v1/ensemble'));
  assert(!h.calls.slice(before).some((call) => call.url.startsWith('/api/scene')));
  assert.match(h.el('measured').textContent, /No reference/);
});

test('binding bounds are checked before submission and custom edits invalidate results', async () => {
  const h = harness();
  await h.settle();
  h.select('isotope', 'custom');
  h.el('binding').value = '10001';
  const before = h.calls.length;
  await h.click('run');
  assert.equal(h.calls.length, before);
  assert.match(h.el('error').textContent, /10000/);
  h.el('binding').value = '10000';
  await h.click('run');
  assert.equal(h.el('play').disabled, false);
  // WKB compatibility payloads omit optional research fields, even if entered.
  const payload = h.calls.findLast((call) => call.url === '/v1/barrier').body;
  assert.equal(payload.research_model, undefined);
  assert.equal(payload.parent_binding_mev, undefined);
  h.el('energy').oninput();
  assert.equal(h.el('half').textContent, '—');
  assert.equal(h.el('play').disabled, true);
  assert.equal(h.operations.at(-1), 'clear');
});
