// Thin view over the virtual device host: it shows frames and forwards raw
// button levels. All product behaviour runs in the firmware code on the host.
'use strict';

const $ = (selector) => document.querySelector(selector);
const screen = $('#screen');
const context = screen.getContext('2d');
const image = context.createImageData(240, 320);
const consoleView = $('#console');

let frame = -1;
let buttons = 0;
let status = null;
let soundOn = true;
let audio = null;
let lastBeeps = null;
let consoleTotal = 0;
let connected = true;

// ---- frames ---------------------------------------------------------------
function draw(buffer) {
  const pixels = new Uint16Array(buffer);
  const out = image.data;
  for (let i = 0, o = 0; i < pixels.length; ++i, o += 4) {
    const c = pixels[i];
    out[o] = ((c >> 11) * 527 + 23) >> 6;
    out[o + 1] = (((c >> 5) & 63) * 259 + 33) >> 6;
    out[o + 2] = ((c & 31) * 527 + 23) >> 6;
    out[o + 3] = 255;
  }
  context.putImageData(image, 0, 0);
}

async function pollFrame() {
  try {
    const response = await fetch(`/api/frame?since=${frame}`, { cache: 'no-store' });
    setConnected(true);
    $('#screenOff').hidden = response.headers.get('X-Screen-On') !== '0';
    if (response.status === 200) {
      draw(await response.arrayBuffer());
      frame = Number(response.headers.get('X-Frame'));
    }
  } catch {
    setConnected(false);
  }
  setTimeout(pollFrame, 33);
}

function setConnected(value) {
  if (connected === value) return;
  connected = value;
  $('#offline').hidden = value;
}

// ---- buttons --------------------------------------------------------------
// Requests are sent in order. A press is held at least 70 ms so a quick click
// still passes the firmware's 30 ms debounce, exactly like a real tap.
let sendChain = Promise.resolve();
function sendButtons() {
  const mask = buttons;
  sendChain = sendChain.then(() => fetch('/api/buttons', { method: 'POST', body: String(mask) }).catch(() => {}));
}
const pressedAt = [0, 0, 0];
function press(bit) {
  if (buttons & (1 << bit)) return;
  unlockAudio();
  pressedAt[bit] = performance.now();
  buttons |= 1 << bit;
  document.querySelector(`.hw[data-bit="${bit}"]`).classList.add('down');
  sendButtons();
}
function release(bit) {
  if (!(buttons & (1 << bit))) return;
  const wait = Math.max(0, 70 - (performance.now() - pressedAt[bit]));
  setTimeout(() => {
    buttons &= ~(1 << bit);
    document.querySelector(`.hw[data-bit="${bit}"]`).classList.remove('down');
    sendButtons();
  }, wait);
}
for (const key of document.querySelectorAll('.hw')) {
  const bit = Number(key.dataset.bit);
  key.addEventListener('pointerdown', (event) => { event.preventDefault(); key.setPointerCapture(event.pointerId); press(bit); });
  key.addEventListener('pointerup', () => release(bit));
  key.addEventListener('pointercancel', () => release(bit));
  key.addEventListener('contextmenu', (event) => event.preventDefault());
}
const keyBits = { ArrowLeft: 0, KeyA: 0, Space: 1, Enter: 1, KeyS: 1, ArrowDown: 1, ArrowRight: 2, KeyD: 2 };
function typing(event) { return event.target instanceof HTMLInputElement; }
window.addEventListener('keydown', (event) => {
  if (typing(event) || !(event.code in keyBits)) return;
  event.preventDefault();
  if (!event.repeat) press(keyBits[event.code]);
});
window.addEventListener('keyup', (event) => {
  if (typing(event) || !(event.code in keyBits)) return;
  event.preventDefault();
  release(keyBits[event.code]);
});
window.addEventListener('blur', () => { for (let bit = 0; bit < 3; ++bit) release(bit); });

// ---- buzzer ---------------------------------------------------------------
function unlockAudio() {
  if (!audio) { try { audio = new AudioContext(); } catch { audio = null; } }
  if (audio && audio.state === 'suspended') audio.resume();
}
function beep() {
  const light = $('#buzzer');
  light.classList.add('on');
  setTimeout(() => light.classList.remove('on'), 120);
  if (!soundOn || !audio) return;
  const oscillator = audio.createOscillator();
  const gain = audio.createGain();
  oscillator.type = 'square';
  oscillator.frequency.value = 2700;  // Typical passive buzzer tone.
  gain.gain.value = 0.06;
  oscillator.connect(gain).connect(audio.destination);
  oscillator.start();
  oscillator.stop(audio.currentTime + 0.08);  // board_.beep(80)
}

// ---- status ---------------------------------------------------------------
// Order of application::Screen in ProductView.h.
const screens = ['홈', '편지', '루틴', '완료 확인', '포기 확인', '단계 저장', '결과', '도감', '캐릭터 도감', '보상 도감',
  '시간 필요', '저장 오류', '먹이 주기', '성장', '진화', '새 캐릭터', '캐릭터 확인', '먹이 상세', '요약 시작', '하루 요약', '요약 끝'];
const apiNames = {
  unconfigured: ['서버 미설정', 'bad'], unpaired: ['페어링 필요', 'warn'], 'waiting-network': ['Wi-Fi 대기', 'warn'],
  'waiting-clock': ['시간 대기', 'warn'], 'battery-unknown': ['배터리 미확인', 'warn'], ready: ['연동됨', 'ok'],
  working: ['통신 중', 'warn'], retry: ['재시도 대기', 'warn'], unauthorized: ['인증 실패 (401)', 'bad'],
  'invalid-response': ['응답 오류', 'bad'], 'merge-conflict': ['병합 충돌', 'bad'], 'storage-error': ['저장 오류', 'bad'],
  'claim-uncertain': ['페어링 결과 불확실', 'bad'],
};
function formatTime(seconds) {
  return new Date(seconds * 1000).toLocaleString('ko-KR', { timeZone: 'Asia/Seoul', month: 'numeric', day: 'numeric', hour: '2-digit', minute: '2-digit', second: '2-digit' });
}
function renderStatus(s) {
  const [apiLabel, apiTone] = apiNames[s.api] || [s.api, 'warn'];
  const rows = [
    ['서버', `<span class="pill ${apiTone}">${apiLabel}</span>${s.http ? ` <span class="muted">HTTP ${s.http}</span>` : ''}`],
    ['모드', s.demo ? '데모 (가상 시간)' : '실제 (PC 시계)'],
    ['기기 시각', formatTime(s.unixSeconds) + (s.clockOffset ? ` (오프셋 ${s.clockOffset}s)` : '')],
    ['화면', `${screens[s.screen] ?? s.screen} · 선택 ${s.selected}${s.screenOn ? '' : ' · 절전'}`],
    ['루틴', `${s.routines}개 · 이벤트 ${s.events}개 · 세대 ${s.generation}`],
    ['기기 UID', s.deviceUid],
    ['UUID', s.uuid || '—'],
  ];
  if (s.fault) rows.push(['오류', '<span class="pill bad">저장 상태 오류</span>']);
  if (!s.assets) rows.push(['자료', '<span class="pill bad">ui.pak 없음</span>']);
  $('#stats').innerHTML = rows.map(([k, v]) => `<dt>${k}</dt><dd>${v}</dd>`).join('');
  $('#version').textContent = `펌웨어 ${s.version} · 동일 C++ 코드 실행 중`;
  $('#endpoint').textContent = `엔드포인트 ${s.endpoint}`;
  setToggle('wifi', s.wifi);
  setToggle('motion', s.motion);
  setToggle('idle', s.idleSeconds > 0);
  setToggle('sound', soundOn);
}
function setToggle(name, on) {
  const button = document.querySelector(`[data-toggle="${name}"]`);
  button.classList.toggle('active', on);
  button.setAttribute('aria-pressed', String(on));
}
async function pollStatus() {
  try {
    const s = await (await fetch('/api/status', { cache: 'no-store' })).json();
    if (lastBeeps !== null && s.beeps > lastBeeps) beep();
    lastBeeps = s.beeps;
    status = s;
    renderStatus(s);
  } catch { /* shown by the frame poller */ }
  setTimeout(pollStatus, 250);
}

// ---- console --------------------------------------------------------------
function appendConsole(text) {
  if (!text) return;
  const atBottom = consoleView.scrollTop + consoleView.clientHeight >= consoleView.scrollHeight - 8;
  consoleView.textContent = (consoleView.textContent + text).slice(-40000);
  if (atBottom) consoleView.scrollTop = consoleView.scrollHeight;
}
async function pollConsole() {
  try {
    const response = await fetch(`/api/console?since=${consoleTotal}`, { cache: 'no-store' });
    const total = Number(response.headers.get('X-Console-Total'));
    const text = await response.text();
    if (total < consoleTotal) consoleView.textContent = '';  // host restarted
    consoleTotal = total;
    appendConsole(text);
  } catch { /* offline */ }
  setTimeout(pollConsole, 400);
}
async function run(line) {
  try {
    await fetch('/api/command', { method: 'POST', body: line });
  } catch {
    appendConsole(`! 전송 실패: ${line}\n`);
  }
}

document.addEventListener('click', (event) => {
  const button = event.target.closest('button');
  if (!button) return;
  if (button.dataset.cmd) {
    if (button.dataset.confirm && !confirm(button.dataset.confirm)) return;
    run(button.dataset.cmd);
  }
  const toggle = button.dataset.toggle;
  if (!toggle || !status) return;
  if (toggle === 'wifi') run(status.wifi ? 'wifi off' : 'wifi on');
  if (toggle === 'motion') run(status.motion ? 'motion off' : 'motion on');
  if (toggle === 'idle') run(status.idleSeconds > 0 ? 'idle off' : 'idle 60');
  if (toggle === 'sound') { soundOn = !soundOn; unlockAudio(); setToggle('sound', soundOn); }
});
$('#pairForm').addEventListener('submit', (event) => {
  event.preventDefault();
  const code = $('#pairCode').value.trim();
  if (!/^[0-9]{10}$/.test(code)) { appendConsole('! 페어링 코드는 숫자 10자리입니다\n'); return; }
  run(`api pair ${code}`);
  $('#pairCode').value = '';
});
$('#cmdForm').addEventListener('submit', (event) => {
  event.preventDefault();
  const line = $('#cmd').value.trim();
  if (line) run(line);
  $('#cmd').value = '';
});

pollFrame();
pollStatus();
pollConsole();
