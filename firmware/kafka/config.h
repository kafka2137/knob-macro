#pragma once

// Override the board-generated default of five layers.
#undef DYNAMIC_KEYMAP_LAYER_COUNT
#define DYNAMIC_KEYMAP_LAYER_COUNT 6
#define DYNAMIC_KEYMAP_MACRO_COUNT 30
#define WEAR_LEVELING_BACKING_SIZE 65536
#define WEAR_LEVELING_LOGICAL_SIZE 32768
#define DYNAMIC_KEYMAP_MACRO_DELAY 1

// All Unicode keystrokes are deferred; these are gaps, not blocking waits.
#define KAFKA_UNICODE_STEP_MS DYNAMIC_KEYMAP_MACRO_DELAY
#define KAFKA_COMPOSE_DELAY_MS 1
#define KAFKA_COMPOSE_KEY KC_RALT

// Wait for the last encoder turn before starting a normal macro. 0 disables it.
#ifndef KAFKA_NORMAL_DEBOUNCE_MS
#    define KAFKA_NORMAL_DEBOUNCE_MS 200
#endif
