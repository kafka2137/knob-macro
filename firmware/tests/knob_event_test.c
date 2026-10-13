#include <assert.h>
#include <stdio.h>
#include "keymap_mock.h"
#include "kafka_text.h"

uint32_t layer_state, default_layer_state;
static unsigned knob_calls, cancel_calls, start_calls, insert_calls, taps;
static uint8_t knob_slot, start_slot;
static bool unassigned;
bool pre_process_record_user(uint16_t keycode, keyrecord_t *record);
bool process_record_user(uint16_t keycode, keyrecord_t *record);
bool encoder_update_user(uint8_t index, bool clockwise);
uint8_t get_highest_layer(uint32_t state) {
    uint8_t layer = 0;
    while (state >>= 1) ++layer;
    return layer;
}
void tap_code(uint8_t key) { (void)key; ++taps; }
void tap_code16(uint16_t key) { (void)key; ++taps; }
void raw_hid_send(uint8_t *data, uint8_t length) { (void)data; (void)length; }
void kafka_text_storage_write(void) {}
void kafka_text_cancel(void) { ++cancel_calls; }
void kafka_text_start(uint8_t slot) { ++start_calls; start_slot = slot; }
void kafka_text_insert(uint8_t slot) { (void)slot; ++insert_calls; }
void kafka_macro_knob_press(uint8_t slot) { ++knob_calls; knob_slot = slot; }
uint8_t kafka_macro_for_step(uint8_t bank, uint8_t step) {
    return unassigned ? 255 : bank * 15 + step;
}
static bool knob(bool down, uint16_t resolved_keycode) {
    keyrecord_t record = {.event = {.key = {.row = 0, .col = 14}, .pressed = down}};
    return pre_process_record_user(resolved_keycode, &record);
}
int main(void) {
    // Mute resolves through a transparent macro layer, but must never reach
    // ordinary key processing on either press or release.
    layer_state = 1u << 3;
    assert(!knob(true, KC_MUTE)); assert(knob_calls == 1 && knob_slot == 255);
    assert(!knob(false, KC_MUTE));
    encoder_update_user(0, true); assert(start_slot == 0);
    assert(!knob(true, KC_MUTE)); assert(knob_slot == 0);
    // Fn released before the knob: still swallow the knob release.
    layer_state = 1u << 1;
    assert(!knob(false, KC_MUTE));
    assert(knob(true, KC_MUTE)); assert(knob(false, KC_MUTE));
    assert(knob_calls == 2);

    layer_state = 1u << 4;
    encoder_update_user(0, true); assert(start_slot == 15);
    assert(!knob(true, KC_MUTE)); assert(knob_slot == 15);
    assert(!knob(false, KC_MUTE));
    // Handling is physical-position based, including a remapped knob key.
    assert(!knob(true, QK_MACRO)); assert(knob_slot == 15);
    assert(!knob(false, QK_MACRO));
    layer_state = 1u << 3;
    assert(!knob(true, KC_MUTE)); assert(knob_slot == 0);
    assert(!knob(false, KC_MUTE));
    encoder_update_user(0, true); assert(start_slot == 1);
    assert(!knob(true, KC_MUTE)); assert(knob_slot == 1);
    assert(!knob(false, KC_MUTE));
    unassigned = true;
    assert(!knob(true, KC_MUTE)); assert(knob_slot == 255);
    assert(!knob(false, KC_MUTE));
    // Another key with the same resolved mute keycode is not the knob.
    keyrecord_t other = {.event = {.key = {.row = 0, .col = 13}, .pressed = true}};
    assert(pre_process_record_user(KC_MUTE, &other));
    assert(process_record_user(KC_MUTE, &other));
    assert(!process_record_user(QK_MACRO, &other)); assert(insert_calls == 1);
    for (unsigned layer = 0; layer < 6; ++layer) {
        if (layer == 3 || layer == 4) continue;
        layer_state = 1u << layer;
        assert(knob(true, KC_MUTE)); assert(knob(false, KC_MUTE));
    }
    assert(start_calls == 3 && cancel_calls == 3 && taps == 0);
    puts("Knob event hooks: layers 3/4 consume mute, select independent slots, handle remaps and Fn release, and preserve other layers.");
}
