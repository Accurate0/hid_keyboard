#include QMK_KEYBOARD_H
#include "raw_hid.h"
#include "print.h"
#pragma once

enum {
    KC_EMO = SAFE_RANGE,
    KC_WPM,
    KC_CAL,
    KC_FSH,
    KC_SC1,
    KC_SC2,
    KC_SC3,
    KC_SC4,
    KC_SC5,

    NEW_SAFE_RANGE
};

enum Layers {
    LY_BASE = 0,
    LY_FUNC
};

bool process_record_user(uint16_t keycode, keyrecord_t *record);
void qk_ucis_start_user(void);
bool encoder_update_user(uint8_t index, bool clockwise);
