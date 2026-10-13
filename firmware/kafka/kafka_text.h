#pragma once

#include <stdint.h>

void kafka_text_start(uint8_t slot);
void kafka_text_insert(uint8_t slot);
void kafka_text_cancel(void);
void kafka_macro_knob_press(uint8_t slot);
void kafka_text_storage_write(void);
// Bank 0 = FN3, bank 1 = FN4; returned slots are global 0–29; 255 means unassigned.
uint8_t kafka_macro_for_step(uint8_t bank, uint8_t step);

// Editor names use VIA/Launcher numeric layers, not the keymap enum names.
static inline uint8_t kafka_bank_for_layer(uint8_t layer) {
    return layer == 3 ? 0 : layer == 4 ? 1 : 255;
}
