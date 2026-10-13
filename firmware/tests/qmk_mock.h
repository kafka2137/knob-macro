#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define DYNAMIC_KEYMAP_MACRO_COUNT 30
#ifndef DYNAMIC_KEYMAP_MACRO_DELAY
#define DYNAMIC_KEYMAP_MACRO_DELAY 10
#endif
#define KC_A 4
#define KC_B 5
#define KC_C 6
#define KC_D 7
#define KC_E 8
#define KC_F 9
#define KC_U 24
#define KC_X 27
#define KC_Y 28
#define KC_1 30
#define KC_2 31
#define KC_3 32
#define KC_4 33
#define KC_5 34
#define KC_6 35
#define KC_7 36
#define KC_8 37
#define KC_9 38
#define KC_0 39
#define KC_ENT 40
#define KC_ESC 41
#define KC_DEL 76
#define KC_LCTL 224
#define KC_LSFT 225
#define KC_RALT 230
#define KC_RGUI 231
#define MOD_BIT(code) (1u << ((code) & 7))
#define pgm_read_byte(ptr) (*(const uint8_t *)(ptr))
void register_code(uint8_t code);
void unregister_code(uint8_t code);
bool is_key_pressed(uint8_t key);
uint8_t get_weak_mods(void);
uint8_t get_mods(void);
uint8_t get_oneshot_mods(void);
void add_weak_mods(uint8_t mods);
void del_weak_mods(uint8_t mods);
void send_keyboard_report(void);
uint32_t timer_elapsed32(uint32_t since);
uint32_t timer_read32(void);
