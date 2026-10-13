/* Copyright 2021 @ Keychron (https://www.keychron.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include QMK_KEYBOARD_H
#include "dynamic_keymap.h"
#include "kafka_text.h"
#include "raw_hid.h"

// clang-format off

enum layers {
    MAC_BASE,
    WIN_BASE,
    _FN1,
    _FN2,
    _FN3,
    _FN4
};

#define KC_TASK LGUI(KC_TAB)
#define KC_FLXP LGUI(KC_E)

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [MAC_BASE] = LAYOUT_iso_70(
        KC_ESC,  KC_1,     KC_2,     KC_3,    KC_4,    KC_5,    KC_6,     KC_7,     KC_8,    KC_9,    KC_0,     KC_MINS,  KC_EQL,   KC_BSPC,          KC_MUTE,
        KC_TAB,  KC_Q,     KC_W,     KC_E,    KC_R,    KC_T,    KC_Y,     KC_U,     KC_I,    KC_O,    KC_P,     KC_LBRC,  KC_RBRC,                    KC_DEL,
        KC_CAPS, KC_A,     KC_S,     KC_D,    KC_F,    KC_G,    KC_H,     KC_J,     KC_K,    KC_L,    KC_SCLN,  KC_QUOT,  KC_NUHS,  KC_ENT,           KC_HOME,
        KC_LSFT, KC_NUBS,  KC_Z,     KC_X,    KC_C,    KC_V,    KC_B,     KC_B,     KC_N,    KC_M,    KC_COMM,  KC_DOT,   KC_SLSH,  KC_RSFT, KC_UP,
        KC_LCTL, KC_LOPT,  KC_LCMD,           KC_SPC,           MO(_FN1), MO(_FN3),          KC_SPC,            KC_RCMD,            KC_LEFT, KC_DOWN, KC_RGHT),

    [WIN_BASE] = LAYOUT_iso_70(
        KC_ESC,  KC_1,     KC_2,     KC_3,    KC_4,    KC_5,    KC_6,     KC_7,     KC_8,    KC_9,    KC_0,     KC_MINS,  KC_EQL,   KC_BSPC,          KC_MUTE,
        KC_TAB,  KC_Q,     KC_W,     KC_E,    KC_R,    KC_T,    KC_Y,     KC_U,     KC_I,    KC_O,    KC_P,     KC_LBRC,  KC_RBRC,                    KC_DEL,
        KC_CAPS, KC_A,     KC_S,     KC_D,    KC_F,    KC_G,    KC_H,     KC_J,     KC_K,    KC_L,    KC_SCLN,  KC_QUOT,  KC_NUHS,  KC_ENT,           KC_HOME,
        KC_LSFT, KC_NUBS,  KC_Z,     KC_X,    KC_C,    KC_V,    KC_B,     KC_B,     KC_N,    KC_M,    KC_COMM,  KC_DOT,   KC_SLSH,  KC_RSFT, KC_UP,
        KC_LCTL, KC_LWIN,  KC_LALT,           KC_SPC,           MO(_FN2), MO(_FN3),          KC_SPC,            KC_RALT,            KC_LEFT, KC_DOWN, KC_RGHT),

    [_FN1] = LAYOUT_iso_70(
        KC_GRV,  KC_BRID,  KC_BRIU,  KC_NO,   KC_NO,   RM_VALD, RM_VALU,  KC_MPRV,  KC_MPLY, KC_MNXT, KC_MUTE,  KC_VOLD,  KC_VOLU,  _______,          RM_TOGG,
        RM_TOGG, RM_NEXT,  RM_VALU,  RM_HUEU, RM_SATU, RM_SPDU, _______,  _______,  _______, _______, _______,  _______,  _______,                    _______,
        _______, RM_PREV,  RM_VALD,  RM_HUED, RM_SATD, RM_SPDD, _______,  _______,  _______, _______, _______,  _______,  _______,  _______,          _______,
        _______, _______,  _______,  _______, _______, _______, _______,  _______,  NK_TOGG, _______, _______,  _______,  _______,  _______, _______,
        _______, _______,  _______,           _______,          _______,  _______,           _______,           _______,            _______, _______, _______),

    [_FN2] = LAYOUT_iso_70(
        KC_GRV,  KC_BRID,  KC_BRIU,  KC_TASK, KC_FLXP, RM_VALD, RM_VALU,  KC_MPRV,  KC_MPLY, KC_MNXT, KC_MUTE,  KC_VOLD,  KC_VOLU,  _______,          RM_TOGG,
        RM_TOGG, RM_NEXT,  RM_VALU,  RM_HUEU, RM_SATU, RM_SPDU, _______,  _______,  _______, _______, _______,  _______,  _______,                    _______,
        _______, RM_PREV,  RM_VALD,  RM_HUED, RM_SATD, RM_SPDD, _______,  _______,  _______, _______, _______,  _______,  _______,  _______,          _______,
        _______, _______,  _______,  _______, _______, _______, _______,  _______,  NK_TOGG, _______, _______,  _______,  _______,  _______, _______,
        _______, _______,  _______,           _______,          _______,  _______,           _______,           _______,            _______, _______, _______),

    [_FN3] = LAYOUT_iso_70(
        KC_TILD, KC_F1,    KC_F2,    KC_F3,   KC_F4,   KC_F5,   KC_F6,    KC_F7,    KC_F8,   KC_F9,   KC_F10,   KC_F11,   KC_F12,   _______,          _______,
        RM_TOGG, RM_NEXT,  RM_VALU,  RM_HUEU, RM_SATU, RM_SPDU, _______,  _______,  _______, _______, _______,  _______,  _______,                    _______,
        _______, RM_PREV,  RM_VALD,  RM_HUED, RM_SATD, RM_SPDD, _______,  _______,  _______, _______, _______,  _______,  _______,  _______,          _______,
        _______, _______,  _______,  _______, _______, _______, _______,  _______,  _______, _______, _______,  _______,  _______,  _______, _______,
        _______, _______,  _______,           _______,          _______,  _______,           _______,           _______,            _______, _______, _______),

    [_FN4] = LAYOUT_iso_70(
        _______, _______,    _______,    _______,   _______,   _______,   _______,    _______,    _______,   _______,   _______,   _______,   _______,   _______,          _______,
        _______, _______,  _______,  _______, _______, _______, _______,  _______,  _______, _______, _______,  _______,  _______,                    _______,
        _______, _______,  _______,  _______, _______, _______, _______,  _______,  _______, _______, _______,  _______,  _______,  _______,          _______,
        _______, _______,  _______,  _______, _______, _______, _______,  _______,  _______, _______, _______,  _______,  _______,  _______, _______,
        _______, _______,  _______,           _______,          _______,  _______,           _______,           _______,            _______, _______, _______)
};

#define TEXT_N 15
static uint8_t encoder_state[2] = {TEXT_N - 1, TEXT_N - 1};
static bool encoder_selected[2];
static bool knob_consumed;

// Identify UTF-8 storage explicitly: an older ASCII firmware must not accept
// Unicode uploads just because it also exposes fifteen VIA macros.
bool via_command_kb(uint8_t *data, uint8_t length) {
    if (data[0] == 0x0F) {
        kafka_text_storage_write();
        return false; // Let VIA perform the standard macro-buffer write.
    }
    if (data[0] != 0xB0 || length < 15) return false;
    data[1] = 'K'; data[2] = 'F'; data[3] = 'U'; data[4] = '8';
    data[5] = 3; // typed macros and per-layer assignments
    data[6] = DYNAMIC_KEYMAP_MACRO_COUNT;
    data[7] = 1023 >> 8; data[8] = 1023 & 0xFF;
    data[9] = 4; // QMK's WinCompose input-mode identifier
    data[10] = 2; data[11] = 15; // encoder banks and steps per bank
    data[12] = 3; data[13] = 4; data[14] = 0;
    raw_hid_send(data, length);
    return true;
}

bool pre_process_record_user(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    // The Q8 ISO encoder push switch is matrix row 0, column 14.
    // Consume both edges even if Fn is released while the knob is held.
    if (record->event.key.row == 0 && record->event.key.col == 14) {
        if (!record->event.pressed && knob_consumed) {
            knob_consumed = false;
            return false;
        }
        uint8_t bank = kafka_bank_for_layer(get_highest_layer(layer_state | default_layer_state));
        if (record->event.pressed && bank != 255) {
            knob_consumed = true;
            uint8_t slot = encoder_selected[bank] ? kafka_macro_for_step(bank, encoder_state[bank]) : 255;
            kafka_macro_knob_press(slot);
            return false;
        }
    }
    return true;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    // VIA's standard SEND_STRING player cannot decode UTF-8. Assigned macro
    // keys insert the same texts through our deferred player, without Ctrl+A.
    if (keycode >= QK_MACRO && keycode <= QK_MACRO_MAX) {
        if (record->event.pressed) kafka_text_insert(keycode - QK_MACRO);
        return false;
    }
    return true;
}

bool encoder_update_user(uint8_t index, bool clockwise) {
    if (index != 0) {
        return true;
    }

    uint8_t layer = get_highest_layer(layer_state | default_layer_state);

    // Any turn cancels the previous text, including a turn on another layer.
    kafka_text_cancel();

    switch (layer) {
        case MAC_BASE:
        case WIN_BASE:
            tap_code(clockwise ? KC_VOLU : KC_VOLD);
            break;
        case _FN1:
            tap_code16(clockwise ? RM_VALU : RM_VALD);
            break;
        case 3:
        case 4: {
            uint8_t bank = kafka_bank_for_layer(layer);
            encoder_state[bank] = clockwise ? (encoder_state[bank] + 1) % TEXT_N
                                            : (encoder_state[bank] + TEXT_N - 1) % TEXT_N;
            encoder_selected[bank] = true;
            uint8_t slot = kafka_macro_for_step(bank, encoder_state[bank]);
            if (slot != 255) kafka_text_start(slot);
            break;
        }
    }

    return false;
}
