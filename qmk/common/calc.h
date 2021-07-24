#include QMK_KEYBOARD_H
#pragma once

#define CALC_MAX 128

typedef struct {
    uint8_t count;
    uint8_t chars[CALC_MAX];
    bool in_progress : 1;
} calc_t;

void calc_start(void);
void calc_end(void);
void calc_evaluate(void);
bool calc_is_in_progress(void);
void calc_add(uint16_t keycode, bool shift);
bool calc_process_input(uint16_t keycode, keyrecord_t *record);
