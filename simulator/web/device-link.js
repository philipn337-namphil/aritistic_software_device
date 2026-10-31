// Connection to the virtual device host shared by the 3D page: frames,
// raw button levels, status (beeps) and console commands.

export function createDeviceLink({ onFrame, onScreen, onStatus, onBeep, onConnection }) {
  let frame = -1;
  let buttons = 0;
  let lastBeeps = null;
  let connected = true;
  const pressedAt = [0, 0, 0];
  let chain = Promise.resolve();

  function setConnected(value) {
    if (connected === value) return;
    connected = value;
    onConnection?.(value);
  }

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
    try {
      const status = await (await fetch('/api/status', { cache: 'no-store' })).json();
      if (lastBeeps !== null && status.beeps > lastBeeps) onBeep?.();
      lastBeeps = status.beeps;
      onStatus?.(status);
    } catch { /* reported by the frame poller */ }
    setTimeout(pollStatus, 250);
  }

  function send() {
    const mask = buttons;
    chain = chain.then(() => fetch('/api/buttons', { method: 'POST', body: String(mask) }).catch(() => {}));
  }

  // A press is held at least 70 ms so a quick click still passes the
  // firmware's 30 ms debounce, like a real tap.
  function press(bit) {
    if (buttons & (1 << bit)) return false;
    pressedAt[bit] = performance.now();
    buttons |= 1 << bit;
    send();
    return true;
  }
  function release(bit, done) {
    if (!(buttons & (1 << bit))) return;
    const wait = Math.max(0, 70 - (performance.now() - pressedAt[bit]));
    setTimeout(() => {
      buttons &= ~(1 << bit);
      send();
      done?.();
    }, wait);
  }

  function run(line) {
    return fetch('/api/command', { method: 'POST', body: line }).then((r) => r.text()).catch(() => '');
  }

  pollFrame();
  pollStatus();
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
