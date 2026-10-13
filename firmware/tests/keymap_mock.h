#pragma once
#include "qmk_mock.h"
#define PROGMEM
#define MATRIX_ROWS 5
#define MATRIX_COLS 15
// Layout contents are irrelevant to testing the event hooks themselves.
#define LAYOUT_iso_70(...) {{0}}
#define QK_MACRO 0x7700
#define QK_MACRO_MAX 0x771f
#define KC_MUTE 0x7f
#define KC_VOLU 0x80
#define KC_VOLD 0x81
#define RM_VALU 0x7823
#define RM_VALD 0x7824
typedef struct {
    struct { struct { uint8_t row, col; } key; bool pressed; } event;
} keyrecord_t;
extern uint32_t layer_state, default_layer_state;
uint8_t get_highest_layer(uint32_t state);
void tap_code(uint8_t key);
void tap_code16(uint16_t key);
