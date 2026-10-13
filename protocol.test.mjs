import { test } from 'node:test';
import assert from 'node:assert/strict';
import { Keyboard, encodeTexts, decodeTexts, encodeLibrary, decodeLibrary, normalizeLibrary, importLibrary, macroBytes } from './dist/protocol.mjs';

class Device extends EventTarget {
  constructor(size = 31827) { super(); this.memory = new Uint8Array(size); this.writes = []; this.corrupt = false; this.silent = false; this.unicode = true; this.format = 3; }
  async sendReport(id, request) {
    assert.equal(id, 0); assert.equal(request.length, 32);
    if (this.silent) return;
    const data = request.slice();
    const offset = data[1] * 256 + data[2], count = data[3];
    switch (data[0]) {
      case 1: data[1] = 0; data[2] = 13; break;
      case 12: data[1] = this.format === 3 ? 30 : 15; break;
      case 13: data[1] = this.memory.length >>> 8; data[2] = this.memory.length & 255; break;
      case 0xb0:
        if (this.unicode) data.set([75,70,85,56,this.format,this.format === 3 ? 30 : 15,3,255,4,2,15,3,4], 1);
        else data[0] = 0xff;
        break;
      case 14:
        assert(count <= 28); assert(offset + count <= this.memory.length);
        data.set(this.memory.slice(offset, offset + count), 4);
        if (this.corrupt && offset === 0) data[4] ^= 1;
        break;
      case 15:
        assert(count <= 28); assert(offset + count <= this.memory.length);
        this.memory.set(data.slice(4, 4 + count), offset);
        this.writes.push({ offset, bytes: data.slice(4, 4 + count) });
        break;
      default: throw new Error('Unexpected command');
    }
    // Reply before sendReport resolves, as some transports can do.
    const event = new Event('inputreport'); event.data = new DataView(data.buffer); this.dispatchEvent(event);
  }
}

test('fifteen maximum length texts fit and round trip', () => {
  const texts = Array.from({ length: 15 }, (_, i) => String.fromCharCode(65 + i).repeat(1023));
  assert.deepEqual(decodeTexts(encodeTexts(texts, 31827)), texts);
});
test('reject unsafe controls, lone surrogates, oversized UTF-8 texts and insufficient capacity', () => {
  for (const text of ['a\0b', '\x01', '\x85', '\uD800', '\uDC00', 'ą'.repeat(512), '😀'.repeat(256), 'x'.repeat(1024)]) assert.throws(() => encodeTexts([text, ...Array(14).fill('')], 31827));
  assert.throws(() => encodeTexts(Array(15).fill('abc'), 20));
  assert.throws(() => decodeTexts(new Uint8Array(100).fill(1)));
  const bytes = new Uint8Array(100); bytes[0] = 1; assert.throws(() => decodeTexts(bytes));
});
test('save guards playback, writes all packets, verifies, then enables playback', async () => {
  const device = new Device(); const keyboard = new Keyboard(device);
  await keyboard.inspect();
  const texts = Array.from({ length: 15 }, (_, i) => `Text ${i + 1}\nZażółć gęślą jaźń\t中文 العربية 👩🏽‍💻`);
  await keyboard.save(texts);
  assert.deepEqual(device.writes[0], { offset: device.memory.length - 1, bytes: Uint8Array.of(1) });
  assert.deepEqual(device.writes.at(-1), { offset: device.memory.length - 1, bytes: Uint8Array.of(0) });
  assert.deepEqual(decodeTexts(await keyboard.readBuffer()), texts);
  assert.equal(device.memory.at(-1), 0);
  keyboard.dispose();
});

test('UTF-8 round trip preserves Polish, scripts, emoji sequences, combining marks and BOM', () => {
  const texts = ['ąćęłńóśźżĄĆĘŁŃÓŚŹŻ', '中文 العربية हिन्दी 한국어 Ελληνικά Русский', '😀👩🏽‍💻🏳️‍🌈', 'e\u0301\uFE0F', '\uFEFFhello', String.fromCodePoint(0x10ffff), 'ą'.repeat(511) + 'a', ...Array(8).fill('')];
  assert.deepEqual(decodeTexts(encodeTexts(texts, 31827)), texts);
});

test('malformed stored UTF-8 is rejected', () => {
  for (const part of [[0x80], [0xc0,0xaf], [0xed,0xa0,0x80], [0xf4,0x90,0x80,0x80], [0xf0,0x9f]]) {
    const bytes = new Uint8Array(100); bytes.set(part);
    assert.throws(() => decodeTexts(bytes), /invalid UTF-8/);
  }
});

test('older ASCII firmware is rejected before writing', async () => {
  const device = new Device(); device.unicode = false;
  const keyboard = new Keyboard(device);
  await assert.rejects(keyboard.inspect(), /Older ASCII firmware/);
  await assert.rejects(keyboard.save(Array(15).fill('ą')), /Verify the UTF-8 firmware/);
  assert.equal(device.writes.length, 0); keyboard.dispose();
});
test('failed verification leaves playback disabled and a retry recovers it', async () => {
  const device = new Device(); const keyboard = new Keyboard(device);
  await keyboard.inspect(); device.corrupt = true;
  await assert.rejects(keyboard.save(Array(15).fill('test')), /verification failed/);
  assert.equal(device.memory.at(-1), 1);
  device.corrupt = false;
  await keyboard.save(Array(15).fill('recovered'));
  assert.equal(device.memory.at(-1), 0);
  keyboard.dispose();
});
test('timeouts release request state; disconnect rejects outstanding requests', async () => {
  const device = new Device(); device.silent = true;
  const keyboard = new Keyboard(device, 20);
  await assert.rejects(keyboard.inspect(), /did not respond/);
  assert.equal(keyboard.pending, null);
  const pending = keyboard.inspect(); keyboard.dispose();
  await assert.rejects(pending, /disconnected/);
});


test('mixed macros and both layer assignment maps round trip and save', async () => {
  const library = normalizeLibrary(Array(15).fill('ą😀'));
  library.layers.fn3.macros[1] = { type:'normal', text:'', actions:[{type:'down',key:224},{type:'tap',key:6},{type:'up',key:224},{type:'delay',ms:65535},{type:'tap',key:40}] };
  library.layers.fn3.steps[0] = 1; library.layers.fn4.steps[0] = 1; library.layers.fn4.macros[1] = {type:"text",text:"Only FN4",actions:[]};
  assert.deepEqual(decodeLibrary(encodeLibrary(library,31827)), library);
  const device = new Device(), keyboard = new Keyboard(device); await keyboard.inspect(); await keyboard.save(library);
  assert.deepEqual(decodeLibrary(await keyboard.readBuffer()), library); keyboard.dispose();
});
test('normal action encoding rejects invalid actions, keys, delay values and overflow', () => {
  for (const action of [{type:'unknown',key:4},{type:'tap',key:0},{type:'down',key:256},{type:'delay',ms:-1},{type:'delay',ms:65536},{type:'delay',ms:1.5}]) assert.throws(() => macroBytes({type:'normal',actions:[action]}));
  assert.throws(() => macroBytes({type:'normal',actions:Array(341).fill({type:'tap',key:4})}));
  const library = normalizeLibrary(Array(15).fill('')); library.layers.fn4.steps[0] = 15;
  assert.throws(() => encodeLibrary(library,31827));
});
test('corrupted normal actions and layer assignments are rejected on read', () => {
  for (const part of [[1,70], [1,78,5,1,1], [1,78,1,2], [1,78,2,17,1], [1,78,1,1,4]]) {
    const bytes = new Uint8Array(31827); bytes.set(part);
    assert.throws(() => decodeLibrary(bytes));
  }
  const bytes = encodeLibrary(normalizeLibrary(Array(15).fill('')),31827);
  bytes[bytes.length-34+3+15] = 42;
  assert.throws(() => decodeLibrary(bytes), /FN4|fn4/);
});
test('legacy text backups migrate to independent banks and old UTF-8 firmware permits read/export only', async () => {
  const library = importLibrary({format:'kafka-texts-v1',texts:['old text']});
  assert.equal(library.layers.fn3.macros[0].text, 'old text'); assert.equal(library.layers.fn3.steps[0], 0); assert.equal(library.layers.fn4.steps[0], 0);
  assert.deepEqual(importLibrary({format:'kafka-macros-v4',library}), library);
  const device = new Device(31827); device.format = 1;
  const old = new TextEncoder().encode('old text'); device.memory.set(old);
  const keyboard = new Keyboard(device); const info = await keyboard.inspect(); assert.equal(info.legacy,true);
  assert.equal(decodeLibrary(await keyboard.readBuffer(), {format:1}).layers.fn3.macros[0].text,'old text');
  await assert.rejects(keyboard.save(library), /independent-layer firmware/); assert.equal(device.writes.length,0); keyboard.dispose();
});

test('legacy shared drafts deep-copy texts, actions, types and assignments into independent layers', () => {
  const old = {macros:Array.from({length:15},()=>({type:'normal',text:'old',actions:[{type:'tap',key:4}]})),layers:{fn2:Array(15).fill(0),fn3:Array(15).fill(1),fn4:Array(15).fill(2)}};
  const library = importLibrary({format:'kafka-macros-v3',library:old});
  library.layers.fn4.macros[0].text = 'edited';
  library.layers.fn4.macros[0].type = 'text';
  library.layers.fn4.macros[0].actions[0].key = 5;
  library.layers.fn4.steps[0] = 3;
  assert.equal(library.layers.fn3.macros[0].text,'old');
  assert.equal(library.layers.fn3.macros[0].type,'normal');
  assert.equal(library.layers.fn3.macros[0].actions[0].key,4);
  assert.equal(library.layers.fn3.steps[0],1);
  assert.equal(old.macros[0].actions[0].key,4);
  assert.deepEqual(Object.keys(library.layers),['fn3','fn4']);
});
test('all thirty maximum-length definitions fit and remain separate on the wire', () => {
  const library = normalizeLibrary(Array(15).fill('a'.repeat(1023)));
  library.layers.fn4.macros.forEach(macro => macro.text = 'b'.repeat(1023));
  assert.deepEqual(decodeLibrary(encodeLibrary(library,31827)),library);
});

test('format 2 keyboard backup preserves assignments and separates the old shared pool', async () => {
  const device = new Device(15443); device.format = 2;
  device.memory.set(new TextEncoder().encode('shared\0'));
  const tail = device.memory.length - 49;
  device.memory.set([75,77,50],tail);
  device.memory.fill(255,tail+3,device.memory.length-1);
  device.memory[tail+3+15] = 0; device.memory[tail+3+30] = 0;
  const keyboard = new Keyboard(device); await keyboard.inspect();
  const library = decodeLibrary(await keyboard.readBuffer(), {format:keyboard.format});
  library.layers.fn4.macros[0].text = 'only four';
  assert.equal(library.layers.fn3.macros[0].text,'shared');
  assert.equal(library.layers.fn3.steps[0],0); assert.equal(library.layers.fn4.steps[0],0);
  await assert.rejects(keyboard.save(library),/independent-layer/);
  assert.equal(device.writes.length,0); keyboard.dispose();
});
