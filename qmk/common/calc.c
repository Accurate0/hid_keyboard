#include "calc.h"
#include "print.h"

calc_t calc;

static uint8_t calc_keycode_to_char(uint8_t keycode, bool shift) {
    if (shift) {
        switch (keycode) {
            case KC_8:
                return '*';
            case KC_EQL:
                return '+';
        }
    } else {
        switch (keycode) {
            case KC_MINS:
                return '-';
            case KC_SLSH:
                return '/';
            case KC_1:
                return '1';
            case KC_2:
                return '2';
            case KC_3:
                return '3';
            case KC_4:
                return '4';
            case KC_5:
                return '5';
            case KC_6:
                return '6';
            case KC_7:
                return '7';
            case KC_8:
                return '8';
            case KC_9:
                return '9';
            case KC_0:
                return '0';
        }
    }

    dprint("RETURNING 0");
    return 0;
}

bool calc_is_in_progress(void) {
    return calc.in_progress;
}

void calc_start(void) {
    calc.in_progress = true;
    calc.count = 0;
}

void calc_end(void) {
    calc.in_progress = false;
}

void calc_add(uint16_t keycode, bool shift) {
    if (keycode != KC_SPC && keycode != KC_LSHIFT && keycode != KC_RSHIFT)
        calc.chars[calc.count++] = calc_keycode_to_char(keycode, shift);
}

void calc_evaluate(void) {
    calc.chars[calc.count] = 0;
    dprintf("output -> %d:%s \n", calc.count, calc.chars);
    send_string((const char *)calc.chars);
}

bool calc_process_input(uint16_t keycode, keyrecord_t *record) {
    if (!record->event.pressed)
        return true;

    if (keycode == KC_EQL && !(get_mods() & MOD_MASK_SHIFT)) {
        SEND_STRING("=");
        calc_evaluate();
        calc_end();
        return false;
    }

    calc_add(keycode, get_mods() & MOD_MASK_SHIFT);

    return true;
}
