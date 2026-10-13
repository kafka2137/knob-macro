import { Keyboard, SLOT_COUNT, MAX_TEXT, VID, PID, normalizeLibrary, encodeLibrary, decodeLibrary, macroBytes, importLibrary } from './protocol.mjs';
import { KEYCODES } from './keycodes.mjs';

const $ = id => document.getElementById(id);
const draftKey = 'kafka-macros-v4';
let library = normalizeLibrary(Array(SLOT_COUNT).fill(''));
let selected = 0, layer = 'fn3', keyboard = null, saved = null, busy = false, storageAvailable = true;
let recording = false, lastRecorded = null;
const pressed = new Set();
try {
  const stored = JSON.parse(localStorage.getItem(draftKey) || 'null');
  if (stored) library = normalizeLibrary(stored);
  else {
    const old = JSON.parse(localStorage.getItem('kafka-macros-v3') || localStorage.getItem('kafka-texts-v1') || 'null');
    if (old) library = normalizeLibrary(old);
  }
} catch { storageAvailable = false; }
const current = () => library.layers[layer].macros[selected];
const clone = value => JSON.parse(JSON.stringify(value));
const preview = macro => macro.type === 'text' ? macro.text.trim() || 'Empty text' : `${macro.actions.length} key actions`;

function message(text, error = false) { $('status').textContent = text; $('status').classList.toggle('error', error); }
function persist() {
  try { localStorage.setItem(draftKey, JSON.stringify(library)); } catch { storageAvailable = false; }
  if (!storageAvailable) $('library-note').textContent = 'Browser storage is unavailable. Export a backup before closing this page.';
}
function count() {
  let length = 0, invalid = false;
  try { length = macroBytes(current()).length; invalid = length > MAX_TEXT; } catch { invalid = true; }
  $('counter').textContent = invalid ? 'Invalid or oversized macro' : `${length.toLocaleString()} / 1,023 bytes`;
  $('counter').classList.toggle('invalid', invalid);
  $('draft-status').textContent = saved && JSON.stringify(saved) === JSON.stringify(library) ? 'Saved on keyboard' : 'Browser draft';
}
function controls() {
  $('connect').disabled = busy || recording;
  $('connect').textContent = keyboard ? 'Disconnect keyboard' : 'Connect keyboard ↗';
  $('read').disabled = busy || recording || !keyboard;
  $('save').disabled = busy || recording || !keyboard || keyboard.legacy;
  for (const id of ['import', 'export', 'text', 'macro-type', 'layer', 'add-action']) $(id).disabled = busy || recording;
  $('record').disabled = busy;
  $('record').textContent = recording ? 'Stop recording (Esc)' : 'Record keys';
  $('progress').hidden = !busy;
  $('connection').textContent = keyboard ? 'Keyboard connected' : 'Keyboard disconnected';
  $('dot').classList.toggle('connected', !!keyboard);
  document.querySelectorAll('#actions input, #actions select, #actions button, #assignments select').forEach(element => { element.disabled = busy || recording; });
}
function renderSlots() {
  $('slots').replaceChildren(...library.layers[layer].macros.map((macro, i) => {
    const button = document.createElement('button'); button.className = `slot${i === selected ? ' selected' : ''}`;
    button.setAttribute('aria-current', i === selected ? 'true' : 'false');
    const number = document.createElement('span'); number.className = 'number'; number.textContent = String(i + 1).padStart(2, '0');
    const body = document.createElement('span');
    const name = document.createElement('span'); name.className = 'name'; name.textContent = `Macro ${i + 1} · ${macro.type}`;
    const summary = document.createElement('span'); summary.className = 'preview'; summary.textContent = preview(macro);
    body.append(name, summary); button.append(number, body);
    button.addEventListener('click', () => { if (recording) stopRecording(); selected = i; render(); });
    return button;
  }));
}
function renderAssignments() {
  $('assignment-title').textContent = `${layer.toUpperCase()} encoder steps`;
  $('assignments').replaceChildren(...library.layers[layer].steps.map((slot, step) => {
    const label = document.createElement('label'); label.textContent = `Step ${step + 1}`;
    const select = document.createElement('select'); select.setAttribute('aria-label', `${layer.toUpperCase()} step ${step + 1}`);
    select.add(new Option('Unassigned', ''));
    library.layers[layer].macros.forEach((macro, i) => select.add(new Option(`Macro ${i + 1} · ${macro.type}`, String(i))));
    select.value = slot === null ? '' : String(slot);
    select.addEventListener('change', () => { library.layers[layer].steps[step] = select.value === '' ? null : Number(select.value); persist(); count(); });
    label.append(select); return label;
  }));
}
function renderActions() {
  $('actions').replaceChildren(...current().actions.map((action, i) => {
    const row = document.createElement('div'); row.className = 'action-row';
    const type = document.createElement('select'); type.setAttribute('aria-label', `Action ${i + 1} type`);
    for (const [value, name] of [['tap','Tap'],['down','Key down'],['up','Key up'],['delay','Delay']]) type.add(new Option(name, value));
    type.value = action.type;
    type.addEventListener('change', () => { current().actions[i] = type.value === 'delay' ? { type: 'delay', ms: 100 } : { type: type.value, key: action.key || 4 }; changed(true); });
    let value;
    if (action.type === 'delay') {
      value = document.createElement('input'); value.type = 'number'; value.min = 0; value.max = 65535; value.value = action.ms;
      value.setAttribute('aria-label', `Action ${i + 1} delay in milliseconds`);
      value.addEventListener('input', () => { action.ms = Number(value.value); changed(); });
    } else {
      value = document.createElement('select'); value.setAttribute('aria-label', `Action ${i + 1} key`);
      KEYCODES.forEach(key => value.add(new Option(key.name.replace(/^KC_/, ''), String(key.code))));
      value.value = String(action.key);
      value.addEventListener('change', () => { action.key = Number(value.value); changed(); });
    }
    row.append(type, value);
    if (action.type === 'delay') { const unit = document.createElement('span'); unit.textContent = 'ms'; row.append(unit); }
    for (const [label, offset] of [['↑', -1], ['↓', 1], ['×', 0]]) {
      const button = document.createElement('button'); button.textContent = label;
      button.setAttribute('aria-label', `${offset ? 'Move' : 'Remove'} action ${i + 1}${offset < 0 ? ' up' : offset > 0 ? ' down' : ''}`);
      button.addEventListener('click', () => {
        if (!offset) current().actions.splice(i, 1);
        else { const next = i + offset; if (next < 0 || next >= current().actions.length) return; [current().actions[i], current().actions[next]] = [current().actions[next], current().actions[i]]; }
        changed(true);
      }); row.append(button);
    }
    return row;
  }));
}
function changed(actions = false) { persist(); count(); renderSlots(); renderAssignments(); if (actions) renderActions(); controls(); }
function render() {
  renderSlots(); renderAssignments(); renderActions();
  $('slot-number').textContent = `MACRO ${String(selected + 1).padStart(2, '0')}`;
  $('slot-title').textContent = `Macro ${selected + 1}`;
  $('macro-type').value = current().type; $('layer').value = layer; $('text').value = current().text;
  $('unicode-note').hidden = current().type !== 'text';
  $('text-panel').hidden = current().type !== 'text'; $('normal-panel').hidden = current().type !== 'normal';
  count(); controls();
}
async function run(action) {
  if (busy || recording) return; busy = true; $('progress').value = 0; controls();
  try { await action(); } catch (error) { message(error.message || String(error), true); }
  finally { busy = false; controls(); count(); }
}
async function disconnect() {
  const currentKeyboard = keyboard; keyboard = null; saved = null; currentKeyboard?.dispose();
  if (currentKeyboard?.device.opened) await currentKeyboard.device.close().catch(() => {});
  $('device-detail').textContent = 'Connect your Q8 by USB to read and save its macros.'; controls(); count();
}
$('text').addEventListener('input', () => { current().text = $('text').value; changed(); });
$('macro-type').addEventListener('change', () => { current().type = $('macro-type').value; persist(); render(); });
$('layer').addEventListener('change', () => { layer = $('layer').value; render(); });
$('add-action').addEventListener('click', () => { current().actions.push({ type: 'tap', key: 4 }); changed(true); });

// Browser KeyboardEvent.code maps to USB/QMK basic keycodes, independently of
// printable characters. Shortcuts intercepted by the browser can be entered
// through the action picker instead.
const codes = { Enter:40, Escape:41, Backspace:42, Tab:43, Space:44, Minus:45, Equal:46, BracketLeft:47, BracketRight:48, Backslash:49, Semicolon:51, Quote:52, Backquote:53, Comma:54, Period:55, Slash:56, CapsLock:57, PrintScreen:70, ScrollLock:71, Pause:72, Insert:73, Home:74, PageUp:75, Delete:76, End:77, PageDown:78, ArrowRight:79, ArrowLeft:80, ArrowDown:81, ArrowUp:82, NumLock:83, NumpadDivide:84, NumpadMultiply:85, NumpadSubtract:86, NumpadAdd:87, NumpadEnter:88, Numpad0:98, NumpadDecimal:99, IntlBackslash:100, ContextMenu:101, ControlLeft:224, ShiftLeft:225, AltLeft:226, MetaLeft:227, ControlRight:228, ShiftRight:229, AltRight:230, MetaRight:231 };
for (let i = 0; i < 26; i++) codes[`Key${String.fromCharCode(65 + i)}`] = 4 + i;
for (let i = 1; i <= 9; i++) { codes[`Digit${i}`] = 29 + i; codes[`Numpad${i}`] = 88 + i; }
codes.Digit0 = 39;
for (let i = 1; i <= 12; i++) codes[`F${i}`] = 57 + i;
function recordAction(action) {
  const now = performance.now();
  if (lastRecorded !== null && now - lastRecorded >= 20) current().actions.push({ type:'delay', ms:Math.min(65535, Math.round(now - lastRecorded)) });
  lastRecorded = now; current().actions.push(action); persist(); count();
}
function stopRecording() {
  if (!recording) return;
  for (const key of pressed) current().actions.push({ type:'up', key });
  pressed.clear(); recording = false; lastRecorded = null; changed(true); message('Recording stopped. Review key actions before saving.');
}
$('record').addEventListener('click', () => {
  if (recording) { stopRecording(); return; }
  recording = true; pressed.clear(); lastRecorded = null; controls();
  message('Recording key presses and releases. Press Escape or Stop recording to finish. Browser-reserved shortcuts can be added manually.');
});
window.addEventListener('keydown', event => {
  if (!recording) return; event.preventDefault();
  if (event.code === 'Escape') { stopRecording(); return; }
  const key = codes[event.code]; if (!key || event.repeat || pressed.has(key)) return;
  pressed.add(key); recordAction({ type:'down', key });
});
window.addEventListener('keyup', event => {
  if (!recording) return; event.preventDefault();
  const key = codes[event.code]; if (!pressed.delete(key)) return; recordAction({ type:'up', key });
});
window.addEventListener('blur', stopRecording);

$('connect').addEventListener('click', () => run(async () => {
  if (keyboard) { await disconnect(); message('Disconnected. Your drafts are preserved.'); return; }
  if (!navigator.hid || !window.isSecureContext) throw new Error('Use Chrome or Edge on HTTPS or localhost. WebHID is unavailable here.');
  const devices = await navigator.hid.requestDevice({ filters:[{ vendorId:VID, productId:PID, usagePage:0xff60, usage:0x61 }] });
  if (!devices.length) { message('Connection cancelled.'); return; }
  const device = devices[0]; if (!device.opened) await device.open(); const candidate = new Keyboard(device);
  try {
    const info = await candidate.inspect(); keyboard = candidate;
    $('device-detail').textContent = `${device.productName || 'Keychron Q8'} · ${info.size.toLocaleString()} bytes · ${info.legacy ? 'older firmware: read/export only' : 'text and normal macros'}`;
    $('storage').textContent = `15 macros per layer · ${info.size.toLocaleString()} bytes available`;
    message(info.legacy ? 'Read and export a backup before installing the independent-layer firmware. Saving is disabled on this older firmware.' : 'Connected. Read the keyboard’s library or save your drafts.');
  } catch (error) { candidate.dispose(); await device.close().catch(() => {}); throw error; }
}));
$('read').addEventListener('click', () => {
  const hasDrafts = JSON.stringify(library) !== JSON.stringify(normalizeLibrary(Array(SLOT_COUNT).fill('')));
  if (hasDrafts && !confirm('Replace browser drafts with the keyboard’s macros and assignments? Export a backup first if you need these drafts.')) return;
  run(async () => {
    message('Reading macros and assignments…'); const bytes = await keyboard.readBuffer(value => { $('progress').value = value; });
    if (bytes.at(-1) !== 0) throw new Error('A previous save was interrupted. Import a backup, then save again after installing the matching firmware.');
    library = decodeLibrary(bytes, { format: keyboard.format }); saved = clone(library); persist(); render(); message('Read all macros and layer assignments.');
  });
});
$('save').addEventListener('click', () => run(async () => {
  encodeLibrary(library, keyboard.size);
  if (!confirm('Replace all keyboard macros and FN3/FN4 encoder assignments with these drafts?')) return;
  message('Saving and verifying the library. Keep the keyboard connected…');
  await keyboard.save(library, value => { $('progress').value = value; }); saved = clone(library);
  message('All macros and assignments saved and verified. No reflash is needed for later edits.');
}));
$('export').addEventListener('click', () => {
  const blob = new Blob([JSON.stringify({ format:'kafka-macros-v4', library }, null, 2) + '\n'], { type:'application/json' });
  const url = URL.createObjectURL(blob), link = document.createElement('a'); link.href = url; link.download = 'kafka-macros.json'; link.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000); message('Backup exported.');
});
$('import').addEventListener('click', () => $('file').click());
$('file').addEventListener('change', async event => {
  const file = event.target.files[0]; event.target.value = ''; if (!file) return;
  try {
    if (file.size > 1048576) throw new Error('This backup file is too large.');
    const imported = importLibrary(JSON.parse(await file.text())); encodeLibrary(imported, 31827);
    if (!confirm('Replace all browser drafts with this backup? The keyboard changes only when you save.')) return;
    library = imported; persist(); render(); message('Backup imported. Connect and save to update the keyboard.');
  } catch (error) { message(error.message || 'Could not import this backup.', true); }
});
navigator.hid?.addEventListener('disconnect', event => { if (keyboard?.device === event.device) { disconnect(); message('Keyboard disconnected. Drafts are preserved. Reconnect and retry an interrupted save.', true); } });
window.addEventListener('beforeunload', event => { if (busy || recording) { event.preventDefault(); event.returnValue = ''; } });
persist(); render();
if (!navigator.hid) message('WebHID is unavailable. Edit and export drafts here; use Chrome or Edge to connect.', true);
