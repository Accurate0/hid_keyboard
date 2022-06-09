#include QMK_KEYBOARD_H
#include "print.h"
#include "raw_hid.h"
#pragma once

enum {
    KC_EMO = SAFE_RANGE,
    KC_WPM,
    KC_CAL,
    KC_FSH,
    KC_KAL,
    KC_LIG,
    KC_SC1,
    KC_SC2,
    KC_SC3,
    KC_SC4,
    KC_SC5,

    KC_RSL,
    KC_NXT,
    KC_THV,
    KC_EFF,

    NEW_SAFE_RANGE
};

enum Layers {
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

    struct {
        bool enabled;
        uint32_t last_keepalive;
    } keepalive;

} globals_t;

typedef union {
    uint32_t raw;
    struct {
        struct {
            bool disabled : 1;
        } hid;

        struct {
            bool disabled : 1;
        } effect;
    };
} eeprom_config_t;

// persistent
extern eeprom_config_t _eeprom_config;
// runtime
extern globals_t _globals;

bool process_record_user(uint16_t keycode, keyrecord_t *record);
void qk_ucis_start_user(void);
bool encoder_update_user(uint8_t index, bool clockwise);
