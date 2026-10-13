export const SLOT_COUNT = 15;
export const MAX_TEXT = 1023;
export const VID = 0x3434;
export const PID = 0x0183;

const encoder = new TextEncoder();
const decoder = new TextDecoder('utf-8', { fatal: true, ignoreBOM: true });
export function textBytes(text) {
  if (typeof text !== 'string') throw new Error('Text must be a string.');
  if (/[\uD800-\uDFFF]/u.test(text)) throw new Error('Text contains an incomplete Unicode character.');
  if (/[\x00-\x08\x0b-\x1f\x7f-\x9f]/u.test(text)) throw new Error('Use text, tabs, and line breaks; other control characters are unsupported.');
  return encoder.encode(text);
}

export const LAYERS = ['fn3', 'fn4'];
export const MACRO_COUNT = 30;
export const ASSIGNMENT_BYTES = 34;
export const emptyMacro = () => ({ type: 'text', text: '', actions: [] });
const identity = () => Array.from({ length: SLOT_COUNT }, (_, i) => i);

export function normalizeLibrary(value) {
  if (Array.isArray(value)) value = { macros: value.map(text => ({ type: 'text', text, actions: [] })) };
  const layers = {};
  for (const layer of LAYERS) {
    // Older editors shared one pool. Copy it independently into each bank.
    const macros = value?.macros ?? value?.layers?.[layer]?.macros;
    const steps = value?.macros ? (value.layers?.[layer] ?? identity()) : value?.layers?.[layer]?.steps;
    if (!Array.isArray(macros) || macros.length !== SLOT_COUNT) throw new Error(`Supply exactly 15 macros for ${layer.toUpperCase()}.`);
    if (!Array.isArray(steps) || steps.length !== SLOT_COUNT || steps.some(slot => slot !== null && (!Number.isInteger(slot) || slot < 0 || slot >= SLOT_COUNT))) throw new Error(`Layer ${layer.toUpperCase()} needs 15 valid macro assignments.`);
    layers[layer] = { macros: macros.map((macro, i) => {
      if (!macro || !['text', 'normal'].includes(macro.type) || typeof macro.text !== 'string' || !Array.isArray(macro.actions)) throw new Error(`Macro ${i + 1} has an invalid type or content.`);
      return { type: macro.type, text: macro.text, actions: macro.actions.map(action => ({ ...action })) };
    }), steps: [...steps] };
  }
  return { layers };
}

export function macroBytes(macro) {
  if (macro.type === 'text') return textBytes(macro.text);
  if (macro.type !== 'normal' || !Array.isArray(macro.actions)) throw new Error('Invalid macro type.');
  const bytes = [1, 78]; // reserved normal-macro prefix; never valid plain text
  const opcodes = { tap: 1, down: 2, up: 3, delay: 4 };
  for (const action of macro.actions) {
    const op = opcodes[action.type];
    if (!op) throw new Error('Use tap, down, up, or delay actions.');
    const value = op === 4 ? action.ms : action.key;
    if (!Number.isInteger(value) || value < (op === 4 ? 0 : 4) || value > (op === 4 ? 65535 : 255)) throw new Error(op === 4 ? 'Delays must be 0–65,535 milliseconds.' : 'Choose an 8-bit QMK keycode (4–255).');
    bytes.push(op);
    for (let shift = op === 4 ? 12 : 4; shift >= 0; shift -= 4) bytes.push(((value >>> shift) & 15) + 1);
    if (bytes.length > MAX_TEXT) throw new Error('Normal macro exceeds 1,023 bytes.');
  }
  return Uint8Array.from(bytes);
}

function decodeMacro(part) {
  if (part.length > MAX_TEXT) throw new Error('The keyboard contains an oversized macro.');
  if (part[0] !== 1) {
    let text;
    try { text = decoder.decode(part); textBytes(text); }
    catch { throw new Error('The keyboard contains invalid UTF-8 or unsupported macro actions. Restore a backup.'); }
    return { type: 'text', text, actions: [] };
  }
  if (part[1] !== 78) throw new Error('Unsupported normal macro format.');
  const types = [null, 'tap', 'down', 'up', 'delay'], actions = [];
  for (let offset = 2; offset < part.length;) {
    const op = part[offset++], count = op === 4 ? 4 : 2;
    if (!types[op] || offset + count > part.length) throw new Error('Incomplete normal macro action.');
    let value = 0;
    for (let i = 0; i < count; i++) {
      const nibble = part[offset++];
      if (nibble < 1 || nibble > 16) throw new Error('Invalid normal macro action.');
      value = (value << 4) | (nibble - 1);
    }
    if (op !== 4 && value < 4) throw new Error('Invalid normal macro keycode.');
    actions.push(op === 4 ? { type: 'delay', ms: value } : { type: types[op], key: value });
  }
  return { type: 'normal', text: '', actions };
}

export function encodeLibrary(value, size) {
  const library = normalizeLibrary(value), bytes = new Uint8Array(size);
  const end = size - ASSIGNMENT_BYTES;
  if (end < SLOT_COUNT) throw new Error('Insufficient keyboard storage.');
  let offset = 0;
  for (const layer of LAYERS) for (let i = 0; i < SLOT_COUNT; i++) {
    let part;
    try { part = macroBytes(library.layers[layer].macros[i]); }
    catch (error) { throw new Error(`${layer.toUpperCase()} macro ${i + 1}: ${error.message}`); }
    if (part.length > MAX_TEXT) throw new Error(`${layer.toUpperCase()} macro ${i + 1} exceeds 1,023 bytes.`);
    if (offset + part.length + 1 > end) throw new Error('The macros exceed the keyboard’s available storage.');
    bytes.set(part, offset); offset += part.length + 1;
  }
  bytes.set([75, 77, 51], end);
  LAYERS.forEach((layer, bank) => bytes.set(library.layers[layer].steps.map(slot => slot === null ? 255 : slot), end + 3 + bank * SLOT_COUNT));
  return bytes;
}

export function decodeLibrary(bytes, { format = 3 } = {}) {
  const oldTail = bytes.length - 49;
  const oldMapped = oldTail >= 0 && bytes[oldTail] === 75 && bytes[oldTail + 1] === 77 && bytes[oldTail + 2] === 50;
  const tail = bytes.length - ASSIGNMENT_BYTES;
  const mapped = tail >= 0 && bytes[tail] === 75 && bytes[tail + 1] === 77 && bytes[tail + 2] === 51;
  const legacy = !mapped && (oldMapped || format < 3);
  const end = mapped ? tail : oldMapped ? oldTail : bytes.length - 1, macros = [];
  let offset = 0;
  for (let i = 0; i < (legacy ? SLOT_COUNT : MACRO_COUNT); i++) {
    const next = bytes.indexOf(0, offset);
    if (next < 0 || next >= end) throw new Error('The keyboard contains an incomplete macro buffer. Restore a backup.');
    macros.push(decodeMacro(bytes.slice(offset, next))); offset = next + 1;
  }
  const layers = {};
  LAYERS.forEach((layer, bank) => {
    const mapOffset = mapped ? tail + 3 + bank * SLOT_COUNT : oldTail + 3 + (bank + 1) * SLOT_COUNT;
    const steps = mapped || oldMapped ? Array.from(bytes.slice(mapOffset, mapOffset + SLOT_COUNT), slot => slot === 255 ? null : slot) : identity();
    layers[layer] = { macros: legacy ? macros : macros.slice(bank * SLOT_COUNT, (bank + 1) * SLOT_COUNT), steps };
  });
  return normalizeLibrary({ layers });
}

export function encodeTexts(texts, size) { return encodeLibrary(texts, size); }
export function decodeTexts(bytes) {
  const macros = decodeLibrary(bytes).layers.fn3.macros;
  if (macros.some(macro => macro.type !== 'text')) throw new Error('This library includes normal macros.');
  return macros.map(macro => macro.text);
}

export function importLibrary(value) {
  if (['kafka-macros-v3', 'kafka-macros-v4'].includes(value?.format)) return normalizeLibrary(value.library);
  if (['kafka-texts-v1', 'kafka-texts-v2'].includes(value?.format) && Array.isArray(value.texts) && value.texts.length <= SLOT_COUNT) return normalizeLibrary([...value.texts, ...Array(SLOT_COUNT - value.texts.length).fill('')]);
  throw new Error('Choose a Kafka macro or text backup.');
}

export class Keyboard {
  constructor(device, timeout = 2500) {
    this.device = device;
    this.timeout = timeout;
    this.pending = null;
    this.listener = event => {
      const bytes = new Uint8Array(event.data.buffer, event.data.byteOffset, event.data.byteLength);
      if (this.pending && this.pending.match(bytes)) this.pending.resolve(bytes.slice());
    };
    device.addEventListener('inputreport', this.listener);
  }

  async request(prefix, match = bytes => bytes[0] === prefix[0]) {
    if (this.pending) throw new Error('A keyboard request is already running.');
    const packet = new Uint8Array(32);
    packet.set(prefix);
    const reply = new Promise((resolve, reject) => {
      const timer = setTimeout(() => reject(new Error('Keyboard did not respond. Flash the supplied firmware once, then reconnect.')), this.timeout);
      this.pending = { match, resolve: value => { clearTimeout(timer); resolve(value); }, reject: error => { clearTimeout(timer); reject(error); } };
    });
    // Attach a handler before sending, even if sendReport itself fails.
    reply.catch(() => {});
    try {
      await this.device.sendReport(0, packet);
      return await reply;
    } catch (error) {
      this.pending?.reject(error);
      throw error;
    } finally {
      this.pending = null;
    }
  }

  async inspect() {
    const version = await this.request([1]);
    this.version = version[1] * 256 + version[2];
    if (this.version < 11) throw new Error('This editor requires VIA protocol 11 or newer. Use the supplied firmware.');
    const count = await this.request([12]);
    if (![15, MACRO_COUNT].includes(count[1])) throw new Error(`This firmware exposes ${count[1]} macros. The supplied Kafka firmware exposes 30.`);
    const size = await this.request([13]);
    this.size = size[1] * 256 + size[2];
    if (this.size < SLOT_COUNT + 1 || this.size > 32768) throw new Error('Unexpected keyboard storage size.');
    const capability = await this.request([0xb0], data => data[0] === 0xb0 || data[0] === 0xff);
    if (String.fromCharCode(...capability.slice(1, 5)) !== 'KFU8' || ![1, 2, 3].includes(capability[5]) || capability[6] !== count[1] || count[1] !== (capability[5] === 3 ? MACRO_COUNT : SLOT_COUNT) || capability[7] * 256 + capability[8] !== MAX_TEXT || capability[9] !== 4) {
      throw new Error('Install the UTF-8 Kafka firmware supplied with this editor. Older ASCII firmware cannot type Unicode.');
    }
    this.unicode = true;
    this.format = capability[5];
    this.legacy = this.format < 3;
    if (!this.legacy && (capability[10] !== 2 || capability[11] !== 15 || ![3, 4].every((layer, i) => capability[12 + i] === layer))) throw new Error('Unexpected encoder layer configuration.');
    return { version: this.version, size: this.size, slots: count[1], legacy: this.legacy };
  }

  async readBuffer(progress = () => {}) {
    const bytes = new Uint8Array(this.size);
    for (let offset = 0; offset < bytes.length; offset += 28) {
      const count = Math.min(28, bytes.length - offset);
      const prefix = [14, offset >>> 8, offset & 255, count];
      const response = await this.request(prefix, data => prefix.every((value, index) => data[index] === value));
      if (response.length !== 32) throw new Error('Unexpected HID response length.');
      bytes.set(response.slice(4, 4 + count), offset);
      progress(Math.min(1, (offset + count) / bytes.length));
    }
    return bytes;
  }

  async writeChunk(offset, bytes) {
    const prefix = [15, offset >>> 8, offset & 255, bytes.length, ...bytes];
    await this.request(prefix, data => prefix.every((value, index) => data[index] === value));
  }

  async save(texts, progress = () => {}) {
    if (!this.unicode) throw new Error('Verify the UTF-8 firmware by reconnecting before saving.');
    if (this.legacy) throw new Error('Install the independent-layer firmware before saving. You can read and export this older firmware first.');
    const bytes = encodeLibrary(texts, this.size);
    // QMK refuses to play any macro while its last storage byte is nonzero.
    // Leave that marker set if a write/verification is interrupted.
    await this.writeChunk(this.size - 1, [1]);
    for (let offset = 0; offset < bytes.length - 1; offset += 28) {
      const chunk = bytes.slice(offset, Math.min(offset + 28, bytes.length - 1));
      await this.writeChunk(offset, chunk);
      progress(0.45 * (offset + chunk.length) / bytes.length);
    }
    const readback = await this.readBuffer(value => progress(0.45 + value * 0.5));
    for (let i = 0; i < bytes.length - 1; i++) {
      if (readback[i] !== bytes[i]) throw new Error('Keyboard verification failed. Text playback remains disabled; save again.');
    }
    if (readback.at(-1) !== 1) throw new Error('The keyboard’s write marker changed. Close other keyboard editors and reconnect.');
    await this.writeChunk(this.size - 1, [0]);
    const last = [14, (this.size - 1) >>> 8, (this.size - 1) & 255, 1];
    const reply = await this.request(last, data => last.every((value, index) => data[index] === value));
    if (reply[4] !== 0) throw new Error('Could not enable text playback. Save again.');
    progress(1);
  }

  dispose() {
    this.pending?.reject(new Error('Keyboard disconnected. Reconnect before trying again.'));
    this.device.removeEventListener('inputreport', this.listener);
  }
}
