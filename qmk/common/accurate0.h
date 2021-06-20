#include QMK_KEYBOARD_H
#pragma once

enum {
    KC_EMO = SAFE_RANGE,
    KC_WPM,
    KC_CAL,
    KC_FSH,
    KC_FSHS,
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

#define LED_COUNT_PER_SIDE 8
static const int left_side_leds[] = { 67, 70, 73, 76, 80, 83, 87, 91 };
static const int right_side_leds[] = { 68, 71, 74, 77, 81, 84, 88, 92 };
