#pragma once
#include "qmk_mock.h"
uint16_t dynamic_keymap_macro_get_buffer_size(void);
void dynamic_keymap_macro_get_buffer(uint16_t offset, uint16_t count, uint8_t *out);
