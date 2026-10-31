// Connection to the virtual device, shared by the 2D and 3D pages. Two runtimes
// expose the same firmware code:
//   host — the PC simulator (yeso-sim.exe) over its small HTTP API
//   wasm — the WebAssembly build running inside this page (hosted demo, phones)
// ?mode=host or ?mode=wasm forces one; otherwise the page uses the PC host
// when it answers and falls back to WebAssembly.

export function createDeviceLink({ onFrame, onScreen, onStatus, onBeep, onConnection, onConsole, onMode }) {
  let buttons = 0;
  let lastBeeps = null;
  let connected = true;
  let runtime = null;   // { setButtons(mask), command(line) → Promise<string> }
  const pressedAt = [0, 0, 0];

  function setConnected(value) {
    if (connected === value) return;
    connected = value;
    onConnection?.(value);
  }
  function status(s) {
    if (lastBeeps !== null && s.beeps > lastBeeps) onBeep?.();
    lastBeeps = s.beeps;
    onStatus?.(s);
  }

  // ---- PC host -------------------------------------------------------------
  function startHost() {
    let frame = -1, consoleTotal = 0, chain = Promise.resolve();
    async function pollFrame() {
      try {
        const response = await fetch(`/api/frame?since=${frame}`, { cache: 'no-store' });
        setConnected(true);
        onScreen?.(response.headers.get('X-Screen-On') !== '0');
        if (response.status === 200) {
          onFrame(new Uint16Array(await response.arrayBuffer()));
          frame = Number(response.headers.get('X-Frame'));
        }
      } catch {
        setConnected(false);
      }
      setTimeout(pollFrame, 33);
    }
    async function pollStatus() {
      try { status(await (await fetch('/api/status', { cache: 'no-store' })).json()); } catch { /* frame poller reports */ }
      setTimeout(pollStatus, 250);
    }
    async function pollConsole() {
      try {
        const response = await fetch(`/api/console?since=${consoleTotal}`, { cache: 'no-store' });
        const total = Number(response.headers.get('X-Console-Total'));
        const text = await response.text();
        onConsole(text, total < consoleTotal);
        consoleTotal = total;
      } catch { /* offline */ }
      setTimeout(pollConsole, 400);
    }
    pollFrame();
    pollStatus();
    if (onConsole) pollConsole();
    return {
      setButtons(mask) {
        chain = chain.then(() => fetch('/api/buttons', { method: 'POST', body: String(mask) }).catch(() => {}));
      },
      command: (line) => fetch('/api/command', { method: 'POST', body: line }).then((r) => r.text()).catch(() => ''),
    };
  }

  // ---- WebAssembly ---------------------------------------------------------
  async function startWasm() {
    const { default: createYesoModule } = await import('./wasm/yeso.js');
    const M = await createYesoModule({ locateFile: (file) => `wasm/${file}`, print: () => {}, printErr: () => {} });
    const pak = await fetch('assets/ui.pak');
    if (!pak.ok) throw new Error('assets/ui.pak missing');
    M.FS.writeFile('/ui.pak', new Uint8Array(await pak.arrayBuffer()));
    // Records, pairing and settings persist in this browser (IndexedDB).
    M.FS.mkdir('/data');
    M.FS.mount(M.IDBFS ?? M.FS.filesystems.IDBFS, {}, '/data');
    await new Promise((resolve) => M.FS.syncfs(true, resolve));
    M.ccall('sim_begin', 'number', ['string', 'string'], ['/data', '/ui.pak']);

    let frame = -1, consoleTotal = 0, syncing = false;
    setInterval(() => M._sim_tick(), 4);
    const draw = () => {
      onScreen?.(M._sim_screen_on() === 1);
      const frames = M._sim_frames();
      if (frames !== frame) {
        frame = frames;
        onFrame(new Uint16Array(M.HEAPU16.buffer, M._sim_pixels(), 240 * 320));
      }
      requestAnimationFrame(draw);
    };
    requestAnimationFrame(draw);
    setInterval(() => {
      status(JSON.parse(M.UTF8ToString(M._sim_status())));
      if (onConsole) {
        const total = M._sim_console_total();
        if (total !== consoleTotal) { onConsole(M.UTF8ToString(M._sim_console_since(consoleTotal)), false); consoleTotal = total; }
      }
    }, 250);
    setInterval(() => {
      if (syncing) return;
      syncing = true;
      M.FS.syncfs(false, () => { syncing = false; });
    }, 2000);
    return {
      setButtons: (mask) => M._sim_buttons(mask),
      command: async (line) => M.UTF8ToString(M.ccall('sim_command', 'number', ['string'], [line])),
    };
  }

  async function hostAvailable() {
    try {
      const controller = new AbortController();
      setTimeout(() => controller.abort(), 1500);
      return (await fetch('/api/status', { cache: 'no-store', signal: controller.signal })).ok;
    } catch {
      return false;
    }
  }
  (async () => {
    const forced = new URLSearchParams(location.search).get('mode');
    const mode = forced === 'host' || forced === 'wasm' ? forced : (await hostAvailable()) ? 'host' : 'wasm';
    onMode?.(mode);
    try {
      runtime = mode === 'host' ? startHost() : await startWasm();
      if (buttons) runtime.setButtons(buttons);
    } catch (error) {
      console.error(error);
      setConnected(false);
    }
  })();

  // A press is held at least 70 ms so a quick click still passes the
  // firmware's 30 ms debounce, like a real tap.
  function press(bit) {
    if (buttons & (1 << bit)) return false;
    pressedAt[bit] = performance.now();
    buttons |= 1 << bit;
    runtime?.setButtons(buttons);
    return true;
  }
  function release(bit, done) {
    if (!(buttons & (1 << bit))) return;
    const wait = Math.max(0, 70 - (performance.now() - pressedAt[bit]));
    setTimeout(() => {
      buttons &= ~(1 << bit);
      runtime?.setButtons(buttons);
      done?.();
    }, wait);
  }
  const run = (line) => (runtime ? runtime.command(line) : Promise.resolve(''));
  return { press, release, run, isPressed: (bit) => Boolean(buttons & (1 << bit)) };
}

// RGB565 little-endian framebuffer to RGBA ImageData.
export function decodeFrame(pixels, image) {
  const out = image.data;
  for (let i = 0, o = 0; i < pixels.length; ++i, o += 4) {
    const c = pixels[i];
    out[o] = ((c >> 11) * 527 + 23) >> 6;
    out[o + 1] = (((c >> 5) & 63) * 259 + 33) >> 6;
    out[o + 2] = ((c & 31) * 527 + 23) >> 6;
    out[o + 3] = 255;
  }
}
