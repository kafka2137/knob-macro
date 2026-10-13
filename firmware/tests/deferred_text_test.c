#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "qmk_mock.h"
#include "deferred_exec.h"
#include "kafka_text.h"

static uint8_t memory[31827], mods, physical_mods, oneshot_mods, held;
static bool keys_down[256];
static uint32_t now, due;
static deferred_token token, next_token;
static deferred_exec_callback callback;
static void *callback_arg;
static bool no_executor;
static unsigned reads, max_read;
static struct { uint8_t key, mods; bool down; uint32_t time; } events[5000];
static unsigned event_count;

const uint8_t ascii_to_keycode_lut[128] = { ['a'] = KC_A, ['b'] = KC_B, ['c'] = KC_C, ['X'] = KC_X, ['Y'] = KC_Y };
const uint8_t ascii_to_shift_lut[16] = { [11] = 3 }; // X and Y
const uint8_t ascii_to_altgr_lut[16] = {0};
const uint8_t ascii_to_dead_lut[16] = {0};

void register_code(uint8_t code) {
    assert(!keys_down[code]); keys_down[code] = true; held = code;
    if (code >= KC_LCTL) physical_mods |= MOD_BIT(code);
    assert(event_count < 5000);
    events[event_count++] = (typeof(events[0])){code, mods, true, now};
}
void unregister_code(uint8_t code) {
    assert(keys_down[code]); keys_down[code] = false; held = 0;
    for (unsigned key = 4; key < 256; ++key) if (keys_down[key]) { held = key; break; }
    if (code >= KC_LCTL) physical_mods &= ~MOD_BIT(code);
    events[event_count++] = (typeof(events[0])){code, mods, false, now};
}
uint8_t get_weak_mods(void) { return mods; }
bool is_key_pressed(uint8_t key) { return keys_down[key]; }
uint8_t get_mods(void) { return physical_mods; }
uint8_t get_oneshot_mods(void) { return oneshot_mods; }
void add_weak_mods(uint8_t bits) { mods |= bits; }
void del_weak_mods(uint8_t bits) { mods &= ~bits; }
void send_keyboard_report(void) {}
uint32_t timer_elapsed32(uint32_t since) { return now - since; }
uint32_t timer_read32(void) { return now; }
uint16_t dynamic_keymap_macro_get_buffer_size(void) { return sizeof(memory); }
void dynamic_keymap_macro_get_buffer(uint16_t offset, uint16_t count, uint8_t *out) {
    assert((unsigned)offset + count <= sizeof(memory));
    ++reads; if (count > max_read) max_read = count;
    memcpy(out, memory + offset, count);
}
deferred_token defer_exec(uint32_t delay, deferred_exec_callback cb, void *arg) {
    assert(!token);
    if (no_executor) return 0;
    token = ++next_token; if (!token) token = ++next_token;
    callback = cb; callback_arg = arg; due = now + delay;
    return token;
}
bool cancel_deferred_exec(deferred_token value) {
    if (value != token || !token) return false;
    token = 0; callback = NULL; return true;
}
static void tick(uint32_t lateness) {
    assert(token); now = due + lateness;
    uint32_t delay = callback(due, callback_arg);
    if (delay) due += delay;
    else { token = 0; callback = NULL; }
}
static void drain(void) {
    unsigned limit = 10000;
    while (token && limit--) tick(0);
    assert(!token); assert(!held);
}
static void reset(void) {
    kafka_text_cancel();
    if (token) drain();
    memset(memory, 0, sizeof(memory));
    memset(keys_down, 0, sizeof(keys_down));
    memcpy(memory, "abc\0XY\0", 7);
    mods = physical_mods = oneshot_mods = held = 0; now = 0; event_count = reads = max_read = 0; no_executor = false;
}

static void load_text(const char *text) {
    reset(); memset(memory, 0, sizeof(memory));
    memcpy(memory, text, strlen(text));
}

static void expect_unicode(const char *text, const uint32_t *expected, unsigned count) {
    load_text(text); kafka_text_insert(0); drain();
    unsigned position = 0;
    for (unsigned i = 0; i < count; ++i) {
        assert(events[position].key == KC_RALT && events[position].down); position += 2;
        assert(events[position].key == KC_U && events[position].down); position += 2;
        uint32_t value = 0; unsigned digits = 0;
        while (position < event_count && events[position].key != KC_ENT) {
            uint8_t key = events[position].key;
            int digit = key == KC_0 ? 0 : key >= KC_1 && key <= KC_9 ? key - KC_1 + 1 : key >= KC_A && key <= KC_F ? key - KC_A + 10 : -1;
            assert(digit >= 0 && events[position].down && !events[position + 1].down);
            assert(events[position + 1].time - events[position].time >= DYNAMIC_KEYMAP_MACRO_DELAY);
            value = (value << 4) | digit; ++digits; position += 2;
        }
        assert(value == expected[i] && digits >= 4 && digits <= 6);
        assert(position < event_count && events[position].key == KC_ENT); position += 2;
    }
    assert(position == event_count);
    assert(!mods && !physical_mods && max_read <= 4);
}

static unsigned action_offset;
static void normal_begin(void) {
    load_text(""); memory[0] = 1; memory[1] = 'N'; action_offset = 2;
}
static void normal_action(uint8_t op, uint16_t value) {
    memory[action_offset++] = op;
    for (int shift = op == 4 ? 12 : 4; shift >= 0; shift -= 4) memory[action_offset++] = ((value >> shift) & 15) + 1;
}

int main(void) {
    assert(kafka_bank_for_layer(3) == 0);
    assert(kafka_bank_for_layer(4) == 1);
    assert(kafka_bank_for_layer(2) == 255);
    assert(kafka_bank_for_layer(5) == 255);
    reset(); kafka_text_start(0); drain();
    assert(event_count == 10); // Ctrl+A, Delete, a, b, c: down and up
    assert(events[0].key == KC_A && events[0].mods == MOD_BIT(KC_LCTL));
    assert(events[2].key == KC_DEL && events[2].mods == 0);
    for (unsigned i = 0; i < event_count; i += 2) {
        assert(events[i].down && !events[i + 1].down);
        assert(events[i + 1].time - events[i].time >= DYNAMIC_KEYMAP_MACRO_DELAY);
    }
    assert(!mods);

    // Interrupt while b is physically down. No old c may follow the switch.
    reset(); kafka_text_start(0);
    while (held != KC_B) tick(0);
    uint32_t switched = now;
    kafka_text_start(1); assert(!held && !mods); drain();
    for (unsigned i = 0; i < event_count; ++i) {
        assert(!(events[i].time >= switched && events[i].down && events[i].key == KC_C));
    }
    assert(events[event_count - 4].key == KC_X && events[event_count - 4].mods == MOD_BIT(KC_LSFT));
    assert(events[event_count - 2].key == KC_Y);
    assert(!mods);

    // Cancel at every stage, including selection, shift setup, held keys, gaps.
    for (unsigned steps = 0; steps < 30; ++steps) {
        reset(); mods = MOD_BIT(KC_RALT); kafka_text_start(1);
        for (unsigned i = 0; i < steps && token; ++i) tick(0);
        kafka_text_cancel();
        assert(!token && !held && mods == MOD_BIT(KC_RALT));
    }

    // An editor upload cancels playback and releases the held key and modifiers.
    reset(); kafka_text_start(1);
    while (held != KC_X) tick(0);
    memory[sizeof(memory) - 1] = 1; tick(0);
    assert(!token && !held && !mods);
    kafka_text_start(0); assert(!token);

    // Search work is bounded for twenty-nine 1023-character predecessors.
    reset(); memset(memory, 'a', sizeof(memory));
    for (unsigned i = 0; i < 30; ++i) memory[(i + 1) * 1024 - 1] = 0;
    memory[sizeof(memory) - 1] = 0;
    kafka_text_start(29); tick(0); assert(!event_count && max_read <= 32);
    kafka_text_cancel(); assert(!token);

    // Late callbacks must still leave a full delay before the next event.
    reset(); kafka_text_start(0); tick(100);
    assert(due - now >= DYNAMIC_KEYMAP_MACRO_DELAY); drain();

    // Unavailable scheduler and invalid slot are harmless.
    reset(); no_executor = true; kafka_text_start(0); assert(!token && !held && !mods);
    no_executor = false; kafka_text_start(30); assert(!token);

    // Timer wraparound and invalid stored macro commands are bounded.
    reset(); now = UINT32_MAX - 15; kafka_text_start(0); drain();
    reset(); memory[0] = 1; kafka_text_start(0); drain(); assert(event_count == 0);
    // All Polish accented letters, uppercase and lowercase.
    const uint32_t polish[] = {0x105,0x107,0x119,0x142,0x144,0xF3,0x15B,0x17A,0x17C,0x104,0x106,0x118,0x141,0x143,0xD3,0x15A,0x179,0x17B};
    expect_unicode("ąćęłńóśźżĄĆĘŁŃÓŚŹŻ", polish, sizeof(polish)/sizeof(*polish));
    const uint32_t scripts[] = {0x4E2D,0x6587,0x416,0x645,0x915,0xD55C,0x1F600,0x1F469,0x1F3FD,0x200D,0x1F4BB,0xFE0F,0x301,0xA000,0x10FFFF};
    expect_unicode("中文Жمक한😀👩🏽‍💻️́ꀀ\xF4\x8F\xBF\xBF", scripts, sizeof(scripts)/sizeof(*scripts));

    load_text("😀"); kafka_text_insert(0);
    while (token) tick(100);
    for (unsigned i = 0; i < event_count; i += 2) {
        assert(events[i + 1].time - events[i].time >= DYNAMIC_KEYMAP_MACRO_DELAY);
    }

    // Cancel at every Unicode phase; Escape only if Compose has not committed.
    for (unsigned steps = 0; steps < 25; ++steps) {
        load_text("😀abc"); kafka_text_insert(0);
        for (unsigned i = 0; i < steps && token; ++i) tick(0);
        bool composed = false, committed = false;
        for (unsigned i = 0; i < event_count; ++i) {
            if (events[i].down && events[i].key == KC_RALT) composed = true;
            if (events[i].down && events[i].key == KC_ENT) committed = true;
        }
        unsigned before = event_count;
        kafka_text_cancel(); if (token) drain();
        assert(!held && !mods && !physical_mods);
        unsigned escapes = 0;
        for (unsigned i = before; i < event_count; ++i) if (events[i].down) {
            assert(events[i].key == KC_ESC); ++escapes;
        }
        assert(escapes == (composed && !committed ? 1u : 0u));
    }

    // Replacement must clear partial compose input before its Ctrl+A.
    load_text("😀\0"); memcpy(memory + strlen("😀") + 1, "XY", 2);
    kafka_text_insert(0); while (held != KC_1) tick(0);
    unsigned before = event_count;
    kafka_text_start(1); drain();
    assert(events[before].key == KC_1 && !events[before].down);
    assert(events[before + 1].key == KC_ESC && events[before + 1].down);
    assert(events[before + 3].key == KC_A && events[before + 3].mods == MOD_BIT(KC_LCTL));

    // Rapid rotations during Escape's hold must not send a second Escape.
    load_text("😀"); memcpy(memory + strlen("😀") + 1, "XY", 2);
    kafka_text_insert(0); while (held != KC_1) tick(0);
    kafka_text_start(1); tick(0); assert(held == KC_ESC);
    before = event_count;
    kafka_text_start(1); drain();
    for (unsigned i = before; i < event_count; ++i) assert(!(events[i].key == KC_ESC && events[i].down));

    // Upload marker must still allow Escape cleanup; no old digits follow it.
    load_text("ą"); kafka_text_insert(0); while (held != KC_5) tick(0);
    before = event_count; memory[sizeof(memory)-1] = 1; tick(100);
    assert(due - now >= 1); drain();
    assert(events[before + 1].key == KC_ESC);

    // Malformed, surrogate, overlong, truncated, C1 and out-of-range sequences.
    const char *invalid[] = {"\x80", "\xC0\xAF", "\xE0\x80\x80", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\xF0\x9F", "\xE2" "a", "\xC2\x85"};
    for (unsigned i = 0; i < sizeof(invalid)/sizeof(*invalid); ++i) {
        load_text(invalid[i]); kafka_text_insert(0); drain(); assert(!event_count);
    }

    // Enforce byte capacity without beginning a truncated final Unicode input.
    load_text(""); memset(memory, 'a', 1021); memcpy(memory + 1021, "😀", 4);
    kafka_text_insert(0); drain(); assert(event_count == 2042);
    for (unsigned i = 0; i < event_count; ++i) assert(events[i].key == KC_A);
    load_text(""); memset(memory, 'a', 1021); memcpy(memory + 1021, "ą", 2);
    kafka_text_insert(0); drain(); assert(events[event_count - 2].key == KC_ENT);

    // Physical and one-shot modifiers are preserved and never used for hex.
    load_text("ą"); physical_mods = MOD_BIT(KC_LSFT); kafka_text_insert(0); drain();
    assert(!event_count && physical_mods == MOD_BIT(KC_LSFT));
    load_text("ą"); oneshot_mods = MOD_BIT(KC_LCTL); kafka_text_insert(0); drain(); assert(!event_count);
    load_text("ą"); kafka_text_insert(0); while (held != KC_5) tick(0);
    physical_mods |= MOD_BIT(KC_LSFT); tick(0); drain();
    assert(physical_mods == MOD_BIT(KC_LSFT) && events[event_count - 2].key == KC_ESC);

    // Normal encoder macros wait from the last selection before any key event.
    normal_begin(); normal_action(1, KC_C);
    kafka_text_start(0); tick(0); assert(!event_count && !held && !mods);
    drain();
    assert(events[0].key == KC_C);
#if KAFKA_NORMAL_DEBOUNCE_MS > 0
    assert(events[0].time >= KAFKA_NORMAL_DEBOUNCE_MS);
#else
    assert(events[0].time < 10);
#endif

#if KAFKA_NORMAL_DEBOUNCE_MS > 10
    // A second turn discards the first pending macro and restarts the timer.
    normal_begin(); normal_action(1, KC_A);
    const uint8_t second_normal[] = {1, 'N', 1, 1, KC_B + 1, 0};
    memcpy(memory + action_offset + 1, second_normal, sizeof(second_normal));
    kafka_text_start(0); tick(0);
    now = KAFKA_NORMAL_DEBOUNCE_MS / 2;
    uint32_t reselected = now;
    kafka_text_start(1); tick(0); tick(0); assert(!event_count && !held && !mods);
    assert(due - reselected == KAFKA_NORMAL_DEBOUNCE_MS);
    drain();
    assert(event_count == 2 && events[0].key == KC_B && events[0].time - reselected >= KAFKA_NORMAL_DEBOUNCE_MS);

    // Switching to text cancels the pending normal macro without delaying text.
    normal_begin(); normal_action(1, KC_X); memcpy(memory + action_offset + 1, "abc", 4);
    kafka_text_start(0); tick(0); now = KAFKA_NORMAL_DEBOUNCE_MS / 2; reselected = now;
    kafka_text_start(1); drain();
    assert(events[0].key == KC_A && events[0].time - reselected < KAFKA_NORMAL_DEBOUNCE_MS);
    for (unsigned i = 0; i < event_count; ++i) assert(events[i].key != KC_X);

    // Assigned macro keys insert normal macros immediately, without encoder debounce.
    normal_begin(); normal_action(1, KC_C); kafka_text_insert(0); drain();
    assert(events[0].time < KAFKA_NORMAL_DEBOUNCE_MS);
#endif

    // An upload cancels pending debounce without ever executing the normal macro.
    normal_begin(); normal_action(1, KC_C); kafka_text_start(0); tick(0);
    kafka_text_storage_write(); assert(!token && !event_count && !held && !mods);

    // Lookup callbacks that run late do not add a second full debounce interval.
    normal_begin(); normal_action(1, KC_C); kafka_text_start(0); tick(KAFKA_NORMAL_DEBOUNCE_MS + 100);
    assert(due - now == 1); drain();

    // The same elapsed-time check remains valid across the 32-bit timer wrap.
    normal_begin(); normal_action(1, KC_C); now = UINT32_MAX - KAFKA_NORMAL_DEBOUNCE_MS / 2;
    uint32_t selected_at = now; kafka_text_start(0); drain();
#if KAFKA_NORMAL_DEBOUNCE_MS > 0
    assert(events[0].time - selected_at >= KAFKA_NORMAL_DEBOUNCE_MS);
#else
    assert(events[0].time - selected_at < 10);
#endif

    // Knob press cancels text without restarting it or sending selection keys.
    reset(); kafka_text_start(0); while (held != KC_A) tick(0);
    kafka_macro_knob_press(0); unsigned cancelled_events = event_count; drain();
    assert(event_count == cancelled_events && !held && !mods);
    reset(); kafka_macro_knob_press(0); drain(); assert(event_count == 0);
    // A pending normal macro runs immediately on press, then replays after end.
    normal_begin(); normal_action(1, KC_C); kafka_text_start(0);
    uint32_t pressed_at = now; kafka_macro_knob_press(0); drain();
    assert(event_count == 2 && events[0].time - pressed_at < 10);
    pressed_at = now; kafka_macro_knob_press(0); drain();
    assert(event_count == 4 && events[2].time - pressed_at < 10);
    // Press during a long delay releases held keys and restarts at action one.
    normal_begin(); normal_action(2, KC_A); normal_action(4, 60000);
    kafka_macro_knob_press(0); while (!keys_down[KC_A]) tick(0);
    kafka_macro_knob_press(0); assert(!keys_down[KC_A]);
    while (!keys_down[KC_A]) tick(0);
    kafka_text_cancel(); assert(!keys_down[KC_A]);

    // Normal macros replay chords without Ctrl+A or Delete.
    normal_begin();
    normal_action(2, KC_LCTL); normal_action(1, KC_C); normal_action(3, KC_LCTL);
    normal_action(2, KC_A); normal_action(2, KC_B); normal_action(4, 500); normal_action(3, KC_A); normal_action(3, KC_B);
    kafka_text_start(0); drain();
    assert(event_count == 6 && events[0].key == KC_C && events[0].mods == MOD_BIT(KC_LCTL));
    assert(events[4].time - events[3].time >= 500);
    for (unsigned i = 0; i < event_count; ++i) assert(events[i].key != KC_DEL);
    assert(!mods && !physical_mods);

    // End/cancel releases every macro-owned held key and weak modifier.
    for (unsigned steps = 0; steps < 12; ++steps) {
        normal_begin(); normal_action(2, KC_LCTL); normal_action(2, KC_A); normal_action(2, KC_B); normal_action(4, 65535);
        kafka_text_start(0);
        for (unsigned i = 0; i < steps && token; ++i) tick(0);
        kafka_text_cancel(); assert(!held && !mods && !token);
    }
    normal_begin(); normal_action(2, KC_LSFT); normal_action(2, KC_A); normal_action(4, 65535);
    kafka_text_start(0); while (held != KC_A) tick(0);
    memory[sizeof(memory)-1] = 1; tick(0); assert(!held && !mods && !token);

    // A VIA write cancels a long delay immediately, without ticking its timer.
    normal_begin(); normal_action(2, KC_LCTL); normal_action(2, KC_A); normal_action(4, 65535);
    kafka_text_start(0); while (held != KC_A) tick(0); tick(0);
    assert(due - now >= 65535);
    kafka_text_storage_write(); assert(!held && !mods && !token);

    // Repeated upload chunks preserve the already-scheduled Escape cleanup.
    load_text("😀"); kafka_text_start(0); while (held != KC_1) tick(0);
    kafka_text_storage_write(); deferred_token cleanup = token;
    kafka_text_storage_write(); assert(token == cleanup);
    drain(); assert(events[event_count - 2].key == KC_ESC);

    // Switching from unfinished Unicode to a normal macro sends Escape first,
    // then the chord; it must never select or delete the input.
    normal_begin(); normal_action(2, KC_LCTL); normal_action(1, KC_C); normal_action(3, KC_LCTL);
    uint8_t normal_copy[16]; memcpy(normal_copy, memory, action_offset + 1);
    load_text("😀"); memcpy(memory + strlen("😀") + 1, normal_copy, 12);
    kafka_text_start(0); while (held != KC_1) tick(0); before = event_count;
    kafka_text_start(1); drain();
    assert(events[before + 1].key == KC_ESC && events[before + 3].key == KC_C);
    for (unsigned i = before; i < event_count; ++i) if (events[i].down) assert(events[i].key != KC_A && events[i].key != KC_DEL);

    // Malformed action operands stop safely and clean held keys.
    normal_begin(); normal_action(2, KC_A); memory[action_offset++] = 2; memory[action_offset++] = 17; memory[action_offset++] = 1;
    kafka_text_start(0); drain(); assert(event_count == 2 && !held);

    // Tapping an already-owned key does not prematurely release the hold.
    normal_begin(); normal_action(2, KC_A); normal_action(1, KC_A); normal_action(3, KC_A);
    kafka_text_start(0); drain(); assert(event_count == 2);

    // A physically held key/modifier is not claimed or released by a macro.
    normal_begin(); normal_action(1, KC_A); physical_mods = MOD_BIT(KC_LCTL); register_code(KC_A);
    kafka_text_start(0); while (token) tick(0);
    assert(keys_down[KC_A] && physical_mods == MOD_BIT(KC_LCTL) && event_count == 1);
    unregister_code(KC_A); physical_mods = 0;
    normal_begin(); normal_action(2, KC_LCTL); normal_action(1, KC_C); normal_action(3, KC_LCTL);
    mods = MOD_BIT(KC_LCTL); kafka_text_start(0); drain(); assert(mods == MOD_BIT(KC_LCTL));

    // All two encoder banks have independent assignments; unknown data is inert.
    reset(); assert(kafka_macro_for_step(0, 14) == 14 && kafka_macro_for_step(1, 0) == 15);
    unsigned metadata = sizeof(memory) - 34;
    memcpy(memory + metadata, "KM3", 3); memset(memory + metadata + 3, 255, 30);
    memory[metadata + 3] = 1; memory[metadata + 3 + 15] = 0;
    assert(kafka_macro_for_step(0, 0) == 1 && kafka_macro_for_step(1, 0) == 15);
    assert(kafka_macro_for_step(2, 0) == 255 && kafka_macro_for_step(0, 15) == 255);
    memory[metadata + 3] = 20; assert(kafka_macro_for_step(0, 0) == 255);
    memory[sizeof(memory)-1] = 1; assert(kafka_macro_for_step(1, 0) == 255);

    puts("Mixed macro playback: Unicode, key chords, interruptible delays, cancellation, ownership, upload guard and two independent assignment banks passed.");
}
