// SPDX-License-Identifier: GPL-2.0-or-later
#include QMK_KEYBOARD_H
#include "deferred_exec.h"
#include "dynamic_keymap.h"
#include "send_string.h"
#include "kafka_text.h"

#ifndef DYNAMIC_KEYMAP_MACRO_DELAY
#    define DYNAMIC_KEYMAP_MACRO_DELAY 10
#endif
#ifndef KAFKA_UNICODE_STEP_MS
#    define KAFKA_UNICODE_STEP_MS DYNAMIC_KEYMAP_MACRO_DELAY
#endif
#ifndef KAFKA_COMPOSE_DELAY_MS
#    define KAFKA_COMPOSE_DELAY_MS 10
#endif
#ifndef KAFKA_COMPOSE_KEY
#    define KAFKA_COMPOSE_KEY KC_RALT
#endif
#ifndef KAFKA_NORMAL_DEBOUNCE_MS
#    define KAFKA_NORMAL_DEBOUNCE_MS 200
#endif

// Every wait returns to QMK. No wait_ms(), tap_code(), or SEND_STRING here.
// Keep a minimum 1 ms interval because returning zero ends a deferred callback.
#define TEXT_STEP_MS ((DYNAMIC_KEYMAP_MACRO_DELAY) > 0 ? (DYNAMIC_KEYMAP_MACRO_DELAY) : 1)
#define UNICODE_STEP_MS ((KAFKA_UNICODE_STEP_MS) > 0 ? (KAFKA_UNICODE_STEP_MS) : 1)
#define TEXT_MAX_LENGTH 1023
#define ASSIGNMENT_BYTES 34 // KM3 signature + 2 x 15 assignments + write marker

typedef enum {
    FIND_SLOT,
    SLOT_TYPE,
    SELECT_MODS,
    SELECT_DOWN,
    SELECT_UP,
    DELETE_DOWN,
    DELETE_UP,
    CHAR_MODS,
    CHAR_DOWN,
    CHAR_UP,
    UNICODE_DOWN,
    UNICODE_UP,
    ABORT_DOWN,
    ABORT_UP,
    NORMAL_NEXT,
    NORMAL_TAP_UP,
} text_phase;

static struct {
    deferred_token token;
    text_phase     phase;
    uint16_t       offset;
    uint16_t       size;
    uint16_t       end;
    uint16_t       typed;
    uint32_t       selected_at;
    uint8_t        skip;
    uint8_t        key;
    uint8_t        held_key;
    uint8_t        added_mods;
    uint8_t        sequence[10];
    uint8_t        sequence_length;
    uint8_t        sequence_pos;
    bool           composing;
    bool           resume;
    bool           replace;
    bool           normal_only;
    uint8_t        normal_keys[32];
    uint8_t        normal_mods;
    bool           tap_owned;
} player;

static uint32_t text_step(uint32_t trigger_time, void *arg);

static uint8_t macro_byte(uint16_t offset) {
    uint8_t value = 0;
    dynamic_keymap_macro_get_buffer(offset, 1, &value);
    return value;
}

static bool has_assignments(uint16_t size) {
    return size >= ASSIGNMENT_BYTES && macro_byte(size - ASSIGNMENT_BYTES) == 'K' && macro_byte(size - ASSIGNMENT_BYTES + 1) == 'M' && macro_byte(size - ASSIGNMENT_BYTES + 2) == '3';
}

uint8_t kafka_macro_for_step(uint8_t bank, uint8_t step) {
    if (bank >= 2 || step >= 15) return 255;
    uint16_t size = dynamic_keymap_macro_get_buffer_size();
    if (size < 2 || macro_byte(size - 1)) return 255;
    if (!has_assignments(size)) return bank * 15 + step;
    uint8_t slot = macro_byte(size - ASSIGNMENT_BYTES + 3 + bank * 15 + step);
    return slot < 15 ? bank * 15 + slot : 255;
}

static bool normal_down(uint8_t key) {
    if (key >= KC_LCTL && key <= KC_RGUI) {
        uint8_t bit = MOD_BIT(key);
        if ((get_weak_mods() | get_mods()) & bit) return false;
        player.normal_mods |= bit;
        add_weak_mods(bit); send_keyboard_report();
    } else {
        uint8_t bit = 1u << (key & 7);
        if ((player.normal_keys[key >> 3] & bit) || is_key_pressed(key)) return false;
        player.normal_keys[key >> 3] |= bit;
        register_code(key);
    }
    return true;
}

static void normal_up(uint8_t key) {
    if (key >= KC_LCTL && key <= KC_RGUI) {
        uint8_t bit = MOD_BIT(key);
        if (player.normal_mods & bit) {
            player.normal_mods &= ~bit;
            del_weak_mods(bit); send_keyboard_report();
        }
    } else {
        uint8_t bit = 1u << (key & 7);
        if (player.normal_keys[key >> 3] & bit) {
            player.normal_keys[key >> 3] &= ~bit;
            unregister_code(key);
        }
    }
}

static void release_normal(void) {
    for (uint16_t key = 4; key < 256; ++key) {
        if (player.normal_keys[key >> 3] & (1u << (key & 7))) normal_up(key);
    }
    if (player.normal_mods) {
        del_weak_mods(player.normal_mods);
        player.normal_mods = 0;
        send_keyboard_report();
    }
}

static void release_key(void) {
    if (player.held_key) {
        unregister_code(player.held_key);
        player.held_key = 0;
    }
    // Remove only weak modifiers this player added; preserve physical modifiers.
    if (player.added_mods) {
        del_weak_mods(player.added_mods);
        player.added_mods = 0;
        send_keyboard_report();
    }
}

static void press_key(uint8_t key) {
    player.held_key = key;
    register_code(key);
}

static void add_modifiers(uint8_t mods) {
    player.added_mods = mods & ~get_weak_mods();
    add_weak_mods(player.added_mods);
    send_keyboard_report();
}

static uint32_t finish(void) {
    release_key();
    release_normal();
    player.token = INVALID_DEFERRED_TOKEN;
    return 0;
}

void kafka_text_cancel(void) {
    if (player.token != INVALID_DEFERRED_TOKEN) {
        cancel_deferred_exec(player.token);
    }
    finish();
    player.resume = false;
    if (player.composing) {
        // Escape must reach the host before a replacement starts its Ctrl+A.
        player.phase = ABORT_DOWN;
        player.token = defer_exec(1, text_step, NULL);
        if (player.token == INVALID_DEFERRED_TOKEN) {
            // A cancelled executor normally frees its own slot. Best-effort
            // cleanup if no executor is available; never wait in this path.
            press_key(KC_ESC);
            release_key();
            player.composing = false;
        }
    }
}

static text_phase first_phase(void) {
    return player.skip ? FIND_SLOT : SLOT_TYPE;
}

void kafka_text_storage_write(void) {
    // Cancel before VIA mutates storage, even during a long normal delay.
    // Repeated upload chunks must not continually restart Escape cleanup.
    if (player.token != INVALID_DEFERRED_TOKEN && (player.phase == ABORT_DOWN || player.phase == ABORT_UP)) return;
    kafka_text_cancel();
}

static uint32_t abort_sequence(uint32_t trigger_time) {
    release_key();
    if (!player.composing) return finish();
    player.resume = false;
    player.phase = ABORT_DOWN;
    return timer_elapsed32(trigger_time) + 1;
}

// Read at most four bytes per character. Reject malformed, overlong, surrogate,
// truncated and out-of-range UTF-8 rather than emitting partial key sequences.
static bool read_codepoint(uint32_t *codepoint) {
    if (player.offset >= player.end || player.typed >= TEXT_MAX_LENGTH) return false;
    uint8_t first = macro_byte(player.offset);
    if (!first) return false;
    uint8_t length;
    uint32_t value, minimum;
    if (first < 0x80) { length = 1; value = first; minimum = 0; }
    else if (first >= 0xC2 && first <= 0xDF) { length = 2; value = first & 0x1F; minimum = 0x80; }
    else if (first >= 0xE0 && first <= 0xEF) { length = 3; value = first & 0x0F; minimum = 0x800; }
    else if (first >= 0xF0 && first <= 0xF4) { length = 4; value = first & 7; minimum = 0x10000; }
    else return false;
    if (player.typed + length > TEXT_MAX_LENGTH || player.offset + length > player.end) return false;
    for (uint8_t i = 1; i < length; ++i) {
        uint8_t next = macro_byte(player.offset + i);
        if ((next & 0xC0) != 0x80) return false;
        value = (value << 6) | (next & 0x3F);
    }
    if (value < minimum || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) return false;
    if ((value < 32 && value != '\t' && value != '\n') || (value >= 0x7F && value <= 0x9F)) return false;
    player.offset += length;
    player.typed += length;
    *codepoint = value;
    return true;
}

static bool conflicting_modifiers(void) {
    uint8_t own = player.held_key == KAFKA_COMPOSE_KEY ? MOD_BIT(KAFKA_COMPOSE_KEY) : 0;
    uint8_t mods = (get_mods() & ~own) | get_weak_mods();
#ifndef NO_ACTION_ONESHOT
    mods |= get_oneshot_mods();
#endif
    return mods != 0;
}

static void prepare_unicode(uint32_t codepoint) {
    // Same WinCompose sequence as QMK's register_unicode(), but every press
    // and release is a separate deferred step, including Compose and Enter.
    static const uint8_t hex_keys[] = {KC_0, KC_1, KC_2, KC_3, KC_4, KC_5, KC_6, KC_7, KC_8, KC_9, KC_A, KC_B, KC_C, KC_D, KC_E, KC_F};
    uint8_t digits = codepoint > 0xFFFFF ? 6 : codepoint > 0xFFFF ? 5 : 4;
    player.sequence_length = 0;
    player.sequence_pos = 0;
    player.sequence[player.sequence_length++] = KAFKA_COMPOSE_KEY;
    player.sequence[player.sequence_length++] = KC_U;
    // WinCompose can mistake a leading a-f for a normal compose rule.
    if (((codepoint >> ((digits - 1) * 4)) & 15) > 9) player.sequence[player.sequence_length++] = KC_0;
    for (uint8_t i = digits; i; --i) player.sequence[player.sequence_length++] = hex_keys[(codepoint >> ((i - 1) * 4)) & 15];
    player.sequence[player.sequence_length++] = KC_ENT;
    player.phase = UNICODE_DOWN;
}

static uint32_t text_step(uint32_t trigger_time, void *arg) {
    (void)arg;
    // The editor sets this byte before changing macro storage.
    bool aborting = player.phase == ABORT_DOWN || player.phase == ABORT_UP;
    if (!aborting && macro_byte(player.size - 1) != 0) {
        return abort_sequence(trigger_time);
    }
    if (!aborting && (player.phase == UNICODE_DOWN || player.phase == UNICODE_UP) && conflicting_modifiers()) {
        return abort_sequence(trigger_time);
    }

    uint32_t delay = TEXT_STEP_MS;
    switch (player.phase) {
        case FIND_SLOT: {
            // Bound lookup work too: a late slot can follow fourteen long texts.
            uint8_t bytes[32];
            uint16_t remaining = player.end - player.offset;
            uint8_t count = remaining < sizeof(bytes) ? remaining : sizeof(bytes);
            dynamic_keymap_macro_get_buffer(player.offset, count, bytes);
            for (uint8_t i = 0; i < count && player.skip; ++i) {
                ++player.offset;
                if (bytes[i] == 0) --player.skip;
            }
            if (player.offset >= player.end) return finish();
            if (player.skip == 0) player.phase = first_phase();
            delay = 1;
            break;
        }
        case SLOT_TYPE:
            if (player.offset >= player.end) return finish();
            delay = 1;
            if (macro_byte(player.offset) == 1) {
                if (player.offset + 2 > player.end || macro_byte(player.offset + 1) != 'N') return finish();
                player.offset += 2; player.typed = 2;
                player.phase = NORMAL_NEXT;
                // Only encoder starts debounce. Count from the last selection,
                // so lookup/Compose cleanup time does not add another full wait.
#if KAFKA_NORMAL_DEBOUNCE_MS > 0
                if (player.replace) {
                    uint32_t elapsed = timer_elapsed32(player.selected_at);
                    if (elapsed < KAFKA_NORMAL_DEBOUNCE_MS) delay = KAFKA_NORMAL_DEBOUNCE_MS - elapsed;
                }
#endif
            } else {
                if (player.normal_only) return finish();
                player.phase = player.replace ? SELECT_MODS : CHAR_MODS;
            }
            break;
        case SELECT_MODS:
            add_modifiers(MOD_BIT(KC_LCTL));
            player.phase = SELECT_DOWN;
            break;
        case SELECT_DOWN:
            press_key(KC_A);
            player.phase = SELECT_UP;
            break;
        case SELECT_UP:
            release_key();
            player.phase = DELETE_DOWN;
            break;
        case DELETE_DOWN:
            press_key(KC_DEL);
            player.phase = DELETE_UP;
            break;
        case DELETE_UP:
            release_key();
            player.phase = CHAR_MODS;
            break;
        case CHAR_MODS: {
            uint32_t codepoint;
            if (!read_codepoint(&codepoint)) return finish();
            if (codepoint >= 0x80) {
                // Do not swallow physical modifiers or turn hex digits into
                // shortcuts. Fn is a layer key and does not conflict.
                if (conflicting_modifiers()) return finish();
                prepare_unicode(codepoint);
                delay = UNICODE_STEP_MS;
                break;
            }
            uint8_t ch = codepoint;
            player.key = pgm_read_byte(&ascii_to_keycode_lut[ch]);
            if (!player.key || ((pgm_read_byte(&ascii_to_dead_lut[ch / 8]) >> (ch % 8)) & 1)) return finish();
            uint8_t mods = 0;
            if ((pgm_read_byte(&ascii_to_shift_lut[ch / 8]) >> (ch % 8)) & 1) mods |= MOD_BIT(KC_LSFT);
            if ((pgm_read_byte(&ascii_to_altgr_lut[ch / 8]) >> (ch % 8)) & 1) mods |= MOD_BIT(KC_RALT);
            add_modifiers(mods);
            player.phase = CHAR_DOWN;
            break;
        }
        case CHAR_DOWN:
            press_key(player.key);
            player.phase = CHAR_UP;
            break;
        case CHAR_UP:
            release_key();
            player.phase = CHAR_MODS;
            break;
        case UNICODE_DOWN:
            if (player.sequence_pos == 0) player.composing = true;
            // Enter commits on key-down; cancellation after it must not send
            // Escape to an application that has already received the text.
            if (player.sequence_pos + 1 == player.sequence_length) player.composing = false;
            press_key(player.sequence[player.sequence_pos]);
            player.phase = UNICODE_UP;
            delay = UNICODE_STEP_MS;
            break;
        case UNICODE_UP:
            release_key();
            // Let WinCompose recognize the prefix without blocking QMK.
            delay = player.sequence_pos < 2 && KAFKA_COMPOSE_DELAY_MS > UNICODE_STEP_MS ? KAFKA_COMPOSE_DELAY_MS : UNICODE_STEP_MS;
            if (++player.sequence_pos == player.sequence_length) {
                player.composing = false;
                player.phase = CHAR_MODS;
            } else player.phase = UNICODE_DOWN;
            break;
        case ABORT_DOWN:
            press_key(KC_ESC);
            // Escape cancels on key-down. A second encoder turn during its
            // hold must not emit another Escape into the application.
            player.composing = false;
            player.phase = ABORT_UP;
            delay = UNICODE_STEP_MS;
            break;
        case ABORT_UP:
            release_key();
            player.composing = false;
            if (!player.resume) return finish();
            player.phase = first_phase();
            delay = UNICODE_STEP_MS;
            break;
        case NORMAL_NEXT: {
            if (player.offset >= player.end || player.typed >= TEXT_MAX_LENGTH) return finish();
            uint8_t op = macro_byte(player.offset);
            if (op == 0) return finish();
            uint8_t count = op == 4 ? 5 : 3;
            if (op > 4 || player.offset + count > player.end || player.typed + count > TEXT_MAX_LENGTH) return finish();
            uint16_t value = 0;
            for (uint8_t i = 1; i < count; ++i) {
                uint8_t nibble = macro_byte(player.offset + i);
                if (nibble < 1 || nibble > 16) return finish();
                value = (value << 4) | (nibble - 1);
            }
            player.offset += count; player.typed += count;
            if (op == 4) { delay = value ? value : 1; break; }
            if (value < 4) return finish();
            if (op == 3) normal_up(value);
            else {
                player.key = value;
                player.tap_owned = normal_down(value);
                if (op == 1) player.phase = NORMAL_TAP_UP;
            }
            break;
        }
        case NORMAL_TAP_UP:
            if (player.tap_owned) normal_up(player.key);
            player.phase = NORMAL_NEXT;
            break;
    }
    // QMK reschedules relative to the intended trigger time. Compensate for
    // lateness so a delayed callback cannot compress subsequent key timings.
    return timer_elapsed32(trigger_time) + delay;
}

static void start(uint8_t slot, bool replace, bool normal_only) {
    uint32_t selected_at = timer_read32();
    kafka_text_cancel();
    uint16_t size = dynamic_keymap_macro_get_buffer_size();
    if (slot >= DYNAMIC_KEYMAP_MACRO_COUNT || size < 2 || macro_byte(size - 1) != 0) return;
    if (player.token != INVALID_DEFERRED_TOKEN) cancel_deferred_exec(player.token);
    player.size = size;
    player.end = has_assignments(size) ? size - ASSIGNMENT_BYTES : size - 1;
    player.offset = 0;
    player.skip = slot;
    player.typed = 0;
    player.selected_at = selected_at;
    player.replace = replace;
    player.normal_only = normal_only;
    player.resume = true;
    player.phase = player.composing ? ABORT_DOWN : first_phase();
    player.token = defer_exec(1, text_step, NULL);
}

void kafka_text_start(uint8_t slot) { start(slot, true, false); }
void kafka_text_insert(uint8_t slot) { start(slot, false, false); }

// Always cancel first; restart only normal content, with no encoder debounce.
void kafka_macro_knob_press(uint8_t slot) { start(slot, false, true); }
