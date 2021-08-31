#include QMK_KEYBOARD_H
#include "print.h"
#include "raw_hid.h"
#pragma once

enum
{
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

enum Layers
{
    LY_BASE = 0,
    LY_FUNC
};

typedef struct {
    struct {
        RGB capslock;
    } color;

    struct {
        bool available;
        bool mute;
        uint8_t volume;
    } hid;

    uint32_t rgb_timeout;
    bool rgb_enabled;
} globals_t;

bool process_record_user(uint16_t keycode, keyrecord_t *record);
void qk_ucis_start_user(void);
bool encoder_update_user(uint8_t index, bool clockwise);
