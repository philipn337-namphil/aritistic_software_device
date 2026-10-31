// 3D view of the routine device. The case, keys and display are the product
// CAD export (models/device.glb); the screen is the firmware's live
// framebuffer and the 3D keys drive the real input path.
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { RoomEnvironment } from 'three/addons/environments/RoomEnvironment.js';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';
import { EffectComposer } from 'three/addons/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/addons/postprocessing/RenderPass.js';
import { GTAOPass } from 'three/addons/postprocessing/GTAOPass.js';
import { OutputPass } from 'three/addons/postprocessing/OutputPass.js';
import { createDeviceLink, decodeFrame } from './device-link.js?v=catalog';

const $ = (selector) => document.querySelector(selector);

// The seven PLA filament colours of the product (colour sheet values).
const PLA = {
  jadeWhite: '#FFFFFF', black: '#000000', almondYellow: '#F0E191', lilacPurple: '#AE96D4',
  appleGreen: '#C2E189', milkyPink: '#F7CED7', babyBlue: '#A4DBE8',
};
// Keys: Baby Blue on the white case (as in the renders), Jade White elsewhere.
// mark: the designer's certification print for that body colour (models/marks/).
const COLORS = [
  { name: 'Jade White #FFFFFF', body: PLA.jadeWhite, key: PLA.babyBlue, strap: PLA.babyBlue, mark: 'jade-white' },
  { name: 'Black #000000', body: PLA.black, key: PLA.jadeWhite, strap: PLA.black, mark: 'black' },
  { name: 'Almond Yellow #F0E191', body: PLA.almondYellow, key: PLA.jadeWhite, strap: PLA.almondYellow, mark: 'almond-yellow' },
  { name: 'Lilac Purple #AE96D4', body: PLA.lilacPurple, key: PLA.jadeWhite, strap: PLA.lilacPurple, mark: 'lilac-purple' },
  { name: 'Apple Green #C2E189', body: PLA.appleGreen, key: PLA.jadeWhite, strap: PLA.appleGreen, mark: 'apple-green' },
  { name: 'Milky Pink #F7CED7', body: PLA.milkyPink, key: PLA.jadeWhite, strap: PLA.milkyPink, mark: 'milky-pink' },
  { name: 'Baby Blue #A4DBE8', body: PLA.babyBlue, key: PLA.jadeWhite, strap: PLA.babyBlue, mark: 'baby-blue' },
];

// ---- renderer / scene -----------------------------------------------------
const canvas = $('#scene');
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: true });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
// Khronos PBR Neutral keeps base colours close to the sheet values (ACES shifts hues).
renderer.toneMapping = THREE.NeutralToneMapping;
renderer.toneMappingExposure = 0.92;
renderer.shadowMap.enabled = true;
renderer.shadowMap.type = THREE.PCFSoftShadowMap;

const scene = new THREE.Scene();
const pmrem = new THREE.PMREMGenerator(renderer);
scene.environment = pmrem.fromScene(new RoomEnvironment(renderer), 0.04).texture;
// The CAD materials are one flat colour; the form reads through the lighting
// (as in the designer's renders): a weak environment so faces turned away from
// the key light fall into shade, plus ambient occlusion in creases (below).
scene.environmentIntensity = 0.35;

const camera = new THREE.PerspectiveCamera(30, 1, 0.1, 200);
const HOME = { position: new THREE.Vector3(6, 4, 30), target: new THREE.Vector3(0, 2.0, 0) };
camera.position.copy(HOME.position);

const controls = new OrbitControls(camera, canvas);
controls.target.copy(HOME.target);
controls.enableDamping = true;
controls.enablePan = false;
controls.minDistance = 6;
controls.maxDistance = 48;
controls.minPolarAngle = 0.35;
controls.maxPolarAngle = Math.PI - 0.6;

// Matched to the designer's renders (55 front, 29 three-quarter): a key light
// high in front so the flat face stays even while the carved sides fall off and
// the screen recess takes a shadow on its top edge; weak fill, rim from behind.
const key = new THREE.DirectionalLight(0xffffff, 2.3);
key.position.set(2, 14, 13);
key.castShadow = true;
key.shadow.mapSize.set(2048, 2048);
Object.assign(key.shadow.camera, { left: -10, right: 10, top: 12, bottom: -10, near: 1, far: 50 });
key.shadow.radius = 4;
key.shadow.bias = -0.0005;
key.shadow.normalBias = 0.03;
const fill = new THREE.DirectionalLight(0xf3eeff, 0.3);
fill.position.set(-10, 2, 6);
const rim = new THREE.DirectionalLight(0xffffff, 0.6);
rim.position.set(-4, 10, -14);
const sky = new THREE.HemisphereLight(0xffffff, 0x8f8499, 0.3);
scene.add(key, fill, rim, sky);

// Ambient occlusion darkens the carved sides, key wells and seams.
const composer = new EffectComposer(renderer);
composer.addPass(new RenderPass(scene, camera));
const ao = new GTAOPass(scene, camera, 1, 1);
ao.updateGtaoMaterial({ radius: 0.45, distanceExponent: 1.5, thickness: 1, scale: 1.2 });
ao.blendIntensity = 0.7;
composer.addPass(ao);
composer.addPass(new OutputPass());

const ground = new THREE.Mesh(new THREE.PlaneGeometry(80, 80), new THREE.ShadowMaterial({ opacity: 0.13 }));
ground.rotation.x = -Math.PI / 2;
ground.position.y = -6.6;
ground.receiveShadow = true;
scene.add(ground);

// ---- device ---------------------------------------------------------------
// The case, keys and display come from the product CAD export (models/device.glb,
// metres, +Z = front, +Y = up). Only materials, the live LCD, the key travel,
// the product sticker and the lanyard are added here.
const MODEL = { url: 'models/device.glb', scale: 9 / 0.0804, seam: 0.0809 };
// Node indices in that export: front shell, back shell, keys ● ▽ ● (D4, D5, D6),
// display module and the board's USB-C receptacle (behind the bottom opening).
const PART = { front: 0, back: 78, keys: [2, 3, 1], display: 77, usb: [59, 63] };
// Dark backdrop behind the USB-C opening so the empty interior never shows (metres).
const USB_BACKDROP = { size: [0.011, 0.0055, 0.0055], center: [0, -0.03575, 0.0812] };
const LCD_ACTIVE = { w: 0.0306, h: 0.0408 };   // 2.0" 240 × 320 IPS active area
const KEY_TRAVEL = 0.0012;

const device = new THREE.Group();
device.rotation.y = -0.32;
scene.add(device);
const model = new THREE.Group();
model.scale.setScalar(MODEL.scale);
model.position.z = -MODEL.seam * MODEL.scale;   // parting line at z = 0
device.add(model);

const bodyMaterial = new THREE.MeshPhysicalMaterial({ roughness: 0.62, clearcoat: 0.2, clearcoatRoughness: 0.5, envMapIntensity: 0.8 });
const keyMaterial = new THREE.MeshPhysicalMaterial({ roughness: 0.3, clearcoat: 0.8, clearcoatRoughness: 0.15 });
const displayMaterial = new THREE.MeshPhysicalMaterial({ color: '#0b0a0d', roughness: 0.15, clearcoat: 1 });
const usbMaterial = new THREE.MeshStandardMaterial({ color: '#c8ccd3', metalness: 0.9, roughness: 0.3 });

const lcdCanvas = document.createElement('canvas');
lcdCanvas.width = 240; lcdCanvas.height = 320;
const lcdContext = lcdCanvas.getContext('2d');
const lcdImage = lcdContext.createImageData(240, 320);
const lcdTexture = new THREE.CanvasTexture(lcdCanvas);
lcdTexture.colorSpace = THREE.SRGBColorSpace;
lcdTexture.anisotropy = renderer.capabilities.getMaxAnisotropy();
const lcdMaterial = new THREE.MeshBasicMaterial({ map: lcdTexture, toneMapped: false, color: 0x111111 });

const buttons = [];   // filled once the model has loaded

// One mesh per part: the export splits every body into many primitives.
function mergePart(node, material) {
  node.updateMatrixWorld(true);
  const inverse = node.matrixWorld.clone().invert(), geometries = [];
  node.traverse((child) => {
    if (!child.isMesh) return;
    const g = child.geometry.clone();
    for (const name of Object.keys(g.attributes)) if (name !== 'position' && name !== 'normal') g.deleteAttribute(name);
    if (!g.attributes.normal) g.computeVertexNormals();
    g.applyMatrix4(new THREE.Matrix4().multiplyMatrices(inverse, child.matrixWorld));
    geometries.push(g.index ? g.toNonIndexed() : g);
  });
  const merged = mergeGeometries(geometries, false);
  const mesh = new THREE.Mesh(merged, material);
  mesh.applyMatrix4(node.matrix);
  mesh.castShadow = true;
  mesh.receiveShadow = true;
  return mesh;
}

// Certification print on the left side of the back shell, toward the key end:
// the designer's artwork (800 × 160, transparent), one tint per body colour.
// Text runs bottom → top with its top edge toward the back, as on the unit.
const MARK_ASPECT = 800 / 160;
const markTextures = new Map();
function markTexture(name) {
  if (!markTextures.has(name)) {
    const texture = new THREE.TextureLoader().load(`models/marks/${name}.png`);
    texture.colorSpace = THREE.SRGBColorSpace;
    texture.anisotropy = renderer.capabilities.getMaxAnisotropy();
    markTextures.set(name, texture);
  }
  return markTextures.get(name);
}
const markMaterial = new THREE.MeshStandardMaterial({
  transparent: true, roughness: 0.6, side: THREE.DoubleSide, depthWrite: false, polygonOffset: true, polygonOffsetFactor: -2,
});
// The print is wrapped onto the real back-shell surface by casting rays at it.
function addSticker(backShell) {
  const S = { height: 0.7, y: -1.5, z: -1.05 };
  S.length = S.height * MARK_ASPECT;
  const geometry = new THREE.PlaneGeometry(S.length, S.height, 48, 6);
  const p = geometry.attributes.position, ray = new THREE.Raycaster(), origin = new THREE.Vector3();
  device.updateMatrixWorld(true);
  const toWorld = device.matrixWorld, toLocal = device.matrixWorld.clone().invert();
  const direction = new THREE.Vector3(1, 0, 0).transformDirection(toWorld);
  let lastX = -3.3;
  for (let i = 0; i < p.count; ++i) {
    const y = S.y + p.getX(i), z = S.z - p.getY(i);
    origin.set(-10, y, z).applyMatrix4(toWorld);
    ray.set(origin, direction);
    const hit = ray.intersectObject(backShell, false)[0];
    if (hit) lastX = hit.point.applyMatrix4(toLocal).x;
    p.setXYZ(i, lastX - 0.004, y, z);
  }
  geometry.computeVertexNormals();
  const mark = new THREE.Mesh(geometry, markMaterial);
  mark.receiveShadow = true;
  device.add(mark);
}

// Lanyard from the strap CAD export (models/strap.glb, metres): node 0 is the
// cord loop, node 1 the split ring (Ø11.6 mm, 1.5 mm wire, in the XY plane,
// centre at -3.8, -110, -2.7 mm). The export is twice the size shown in the
// designer's renders (front render 55: ring Ø5.8 mm, 0.7 mm wire on the 60 mm
// wide case; renders 26/30: whole strap ≈ 120 mm), so it is drawn at half
// scale, parts kept in place. The ring threads the two lanyard holes in the
// case top (x ±1.9 mm, z 80.6 mm, bridge underside y 38.8 mm) and stands
// ≈ 4 mm above it, as in render 55.
const STRAP = { url: 'models/strap.glb', cord: 0, ring: 1, scale: 0.5, ringCenter: [-0.0038, -0.110, -0.0027], hangAt: [0, 0.0410, 0.0806] };
const strapGroup = new THREE.Group();
strapGroup.scale.setScalar(STRAP.scale);
strapGroup.position.set(...STRAP.hangAt.map((v, i) => v - STRAP.ringCenter[i] * STRAP.scale));
model.add(strapGroup);
const strapMaterial = new THREE.MeshPhysicalMaterial({ roughness: 0.55, clearcoat: 0.3 });
const ringMaterial = new THREE.MeshStandardMaterial({ color: '#dfe3ea', metalness: 0.9, roughness: 0.2 });
new GLTFLoader().load(STRAP.url, async (gltf) => {
  const node = (index) => gltf.parser.getDependency('node', index);
  strapGroup.add(mergePart(await node(STRAP.cord), strapMaterial), mergePart(await node(STRAP.ring), ringMaterial));
});

new GLTFLoader().load(MODEL.url, async (gltf) => {
  const node = (index) => gltf.parser.getDependency('node', index);
  const back = mergePart(await node(PART.back), bodyMaterial);
  model.add(mergePart(await node(PART.front), bodyMaterial), back);
  for (const [bit, index] of PART.keys.entries()) {
    const key = mergePart(await node(index), keyMaterial);
    key.userData = { bit, rest: key.position.z, travel: KEY_TRAVEL, depth: 0 };
    model.add(key);
    buttons[bit] = key;
  }
  for (const index of PART.usb) model.add(mergePart(await node(index), usbMaterial));
  // Contact tongue centred in the receptacle opening (front face 8.9 × 3.2 mm, centre z 80.68 mm).
  const tongue = new THREE.Mesh(new THREE.BoxGeometry(0.0066, 0.003, 0.0007), new THREE.MeshStandardMaterial({ color: '#3a3940', roughness: 0.5 }));
  tongue.position.set(0, -0.0372, 0.08068);
  model.add(tongue);
  const backdrop = new THREE.Mesh(new THREE.BoxGeometry(...USB_BACKDROP.size), new THREE.MeshBasicMaterial({ color: '#08080a' }));
  backdrop.position.set(...USB_BACKDROP.center);
  model.add(backdrop);
  const display = mergePart(await node(PART.display), displayMaterial);
  model.add(display);
  display.geometry.computeBoundingBox();
  const box = display.geometry.boundingBox.clone().applyMatrix4(display.matrix), center = box.getCenter(new THREE.Vector3());
  const lcd = new THREE.Mesh(new THREE.PlaneGeometry(LCD_ACTIVE.w, LCD_ACTIVE.h), lcdMaterial);
  lcd.position.set(center.x, center.y, box.max.z + 0.00005);
  model.add(lcd);
  addSticker(back);
  $('#loading').hidden = true;
}, undefined, () => { $('#loading').textContent = '3D 모델(models/device.glb)을 불러오지 못했습니다'; });

function applyColor(index) {
  const c = COLORS[index];
  bodyMaterial.color.set(c.body);
  keyMaterial.color.set(c.key);
  strapMaterial.color.set(c.strap);
  markMaterial.map = markTexture(c.mark);
  markMaterial.needsUpdate = true;
  document.querySelectorAll('.swatch').forEach((s, i) => s.setAttribute('aria-checked', String(i === index)));
  try { localStorage.setItem('yeso-3d-color', String(index)); } catch { /* optional */ }
}

// ---- live link ------------------------------------------------------------
let screenOn = true;
let soundOn = true;
let audio = null;
let shakeUntil = 0;

function unlockAudio() {
  if (!audio) { try { audio = new AudioContext(); } catch { audio = null; } }
  if (audio?.state === 'suspended') audio.resume();
}
function beep() {
  shakeUntil = performance.now() + 140;
  if (!soundOn || !audio) return;
  const oscillator = audio.createOscillator(), gain = audio.createGain();
  oscillator.type = 'square'; oscillator.frequency.value = 2700; gain.gain.value = 0.06;
  oscillator.connect(gain).connect(audio.destination);
  oscillator.start(); oscillator.stop(audio.currentTime + 0.08);
}

const apiLabels = {
  ready: ['서버 연동됨', 'ok'], working: ['동기화 중', 'ok'], unpaired: ['페어링 필요', 'warn'], retry: ['서버 재시도 대기', 'warn'],
  'waiting-network': ['Wi-Fi 대기', 'warn'], unauthorized: ['인증 실패', 'bad'], 'invalid-response': ['서버 응답 오류', 'bad'],
  'claim-uncertain': ['페어링 확인 필요', 'bad'], 'storage-error': ['저장 오류', 'bad'], 'merge-conflict': ['병합 충돌', 'bad'],
};
const link = createDeviceLink({
  onFrame(pixels) {
    decodeFrame(pixels, lcdImage);
    lcdContext.putImageData(lcdImage, 0, 0);
    lcdTexture.needsUpdate = true;
  },
  onScreen(on) {
    screenOn = on;
    lcdMaterial.color.setScalar(on ? 1 : 0.02);
  },
  onStatus(status) {
    const [label, tone] = status.demo ? ['데모 모드', 'warn'] : apiLabels[status.api] || [status.api, 'warn'];
    $('#chipText').textContent = screenOn ? label : `${label} · 절전`;
    $('#chip').className = `chip ${tone}`;
  },
  onBeep: beep,
  onConnection(ok) { $('#offline').hidden = ok; },
  onMode(mode) { $('.brand p').textContent = mode === 'wasm' ? '실제 펌웨어가 이 브라우저 안에서 동작합니다' : '실제 펌웨어로 동작하는 가상 기기'; },
});

// ---- input ----------------------------------------------------------------
const raycaster = new THREE.Raycaster();
const pointer = new THREE.Vector2();
const pointerBits = new Map();

function pick(event) {
  const rect = canvas.getBoundingClientRect();
  pointer.set(((event.clientX - rect.left) / rect.width) * 2 - 1, -((event.clientY - rect.top) / rect.height) * 2 + 1);
  raycaster.setFromCamera(pointer, camera);
  const hit = raycaster.intersectObjects(buttons, false)[0];
  return hit ? hit.object.userData.bit : -1;
}
function pressKey(bit) {
  unlockAudio();
  if (link.press(bit) && buttons[bit]) buttons[bit].userData.depth = 1;
}
function releaseKey(bit) {
  link.release(bit, () => { if (buttons[bit]) buttons[bit].userData.depth = 0; });
}
canvas.addEventListener('pointerdown', (event) => {
  const bit = pick(event);
  if (bit < 0) return;
  controls.enabled = false;
  canvas.setPointerCapture(event.pointerId);
  pointerBits.set(event.pointerId, bit);
  pressKey(bit);
});
function endPointer(event) {
  const bit = pointerBits.get(event.pointerId);
  if (bit === undefined) return;
  pointerBits.delete(event.pointerId);
  releaseKey(bit);
  controls.enabled = true;
}
canvas.addEventListener('pointerup', endPointer);
canvas.addEventListener('pointercancel', endPointer);
canvas.addEventListener('pointermove', (event) => {
  if (!pointerBits.size) canvas.style.cursor = pick(event) >= 0 ? 'pointer' : 'grab';
});

const keyBits = { ArrowLeft: 0, KeyA: 0, Space: 1, Enter: 1, KeyS: 1, ArrowDown: 1, ArrowRight: 2, KeyD: 2 };
window.addEventListener('keydown', (event) => {
  if (!(event.code in keyBits) || event.target.closest?.('button, a')) return;
  event.preventDefault();
  if (!event.repeat) pressKey(keyBits[event.code]);
});
window.addEventListener('keyup', (event) => {
  if (!(event.code in keyBits)) return;
  event.preventDefault();
  releaseKey(keyBits[event.code]);
});
window.addEventListener('blur', () => { for (let bit = 0; bit < 3; ++bit) releaseKey(bit); });

// ---- toolbar --------------------------------------------------------------
const swatches = $('#swatches');
COLORS.forEach((c, i) => {
  const b = document.createElement('button');
  b.className = 'swatch'; b.style.background = c.body; b.title = c.name;
  b.setAttribute('role', 'radio'); b.setAttribute('aria-label', c.name);
  b.addEventListener('click', () => applyColor(i));
  swatches.append(b);
});
let initialColor = 0;
try { initialColor = Number(localStorage.getItem('yeso-3d-color')) || 0; } catch { /* optional */ }
applyColor(Math.min(initialColor, COLORS.length - 1));

function toggle(button, on) { button.setAttribute('aria-pressed', String(on)); }
$('#strap').addEventListener('click', (e) => { strapGroup.visible = !strapGroup.visible; toggle(e.currentTarget, strapGroup.visible); });
$('#sound').addEventListener('click', (e) => { soundOn = !soundOn; unlockAudio(); toggle(e.currentTarget, soundOn); });
let resetFrom = null;
$('#reset').addEventListener('click', () => { resetFrom = { t: 0, p: camera.position.clone(), q: controls.target.clone() }; });

// ---- loop -----------------------------------------------------------------
function resize() {
  const w = window.innerWidth, h = window.innerHeight;
  renderer.setSize(w, h, false);
  composer.setPixelRatio(renderer.getPixelRatio());
  composer.setSize(w, h);
  camera.aspect = w / h;
  // Keep the whole device visible on narrow (portrait) screens.
  const portrait = w / h < 0.8;
  camera.fov = portrait ? 44 : 30;
  // On phones the toolbar sits at the bottom: shift the device up above it.
  if (portrait) camera.setViewOffset(w, h, 0, h * 0.14, w, h); else camera.clearViewOffset();
  camera.updateProjectionMatrix();
}
window.addEventListener('resize', resize);
resize();

// Console hook for checking exact views: __yeso3d.view([x, y, z], [tx, ty, tz]).
window.__yeso3d = {
  view(position, target = [0, 0, 0]) { camera.position.set(...position); controls.target.set(...target); controls.update(); },
  turn(y) { device.rotation.y = y; },
  tune: { renderer, scene, key, fill, rim, sky, ao },
};

const clock = new THREE.Clock();
renderer.setAnimationLoop(() => {
  const dt = Math.min(clock.getDelta(), 0.05), t = clock.elapsedTime;
  for (const b of buttons) {
    const target = b.userData.rest - b.userData.travel * b.userData.depth;
    b.position.z += (target - b.position.z) * Math.min(1, dt * 30);
  }
  // Gentle idle float plus a short buzz when the firmware beeps.
  const shake = performance.now() < shakeUntil ? 0.035 : 0;
  device.position.set((Math.random() - 0.5) * shake, Math.sin(t * 1.1) * 0.12 + (Math.random() - 0.5) * shake, 0);
  if (resetFrom) {
    resetFrom.t = Math.min(1, resetFrom.t + dt * 1.8);
    const k = 1 - Math.pow(1 - resetFrom.t, 3);
    camera.position.lerpVectors(resetFrom.p, HOME.position, k);
    controls.target.lerpVectors(resetFrom.q, HOME.target, k);
    if (resetFrom.t >= 1) resetFrom = null;
  }
  controls.update();
  composer.render(dt);
});
