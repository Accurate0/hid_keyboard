#include "accurate0.h"
#include "calc.h"
#include "hid_commands.h"

__attribute__((weak)) bool process_record_secrets(uint16_t keycode, keyrecord_t *record) {
    return true;
}

const qk_ucis_symbol_t ucis_symbol_table[] =
    UCIS_TABLE(UCIS_SYM("eyes", 0x1F441, 0x1F445, 0x1F441) //👁👅👁
    );

globals_t _globals = {.color = {.capslock = {RGB_GREEN}},
                      .hid = {
                          .available = false,
                          .mute = false,
                          .volume = 80,
                      }};

void raw_hid_receive(uint8_t *data, uint8_t length) {
    uint8_t command = data[0];

    switch (command) {
        case VOLUME_COMMAND: {
            _globals.hid.available = true;
            _globals.hid.volume = data[1];
            dprintf("vol: %d\n", _globals.hid.volume);
        } break;

        case MUTE_COMMAND: {
            _globals.hid.mute = (bool)data[1];
            dprintf("mute: %d\n", _globals.hid.mute);
        } break;
    }
}

bool encoder_update_user(uint8_t index, bool clockwise) {
    switch (get_highest_layer(layer_state)) {
        case LY_BASE:
            if (clockwise) {
                tap_code(KC_VOLU);
            } else {
                tap_code(KC_VOLD);
            }
            break;

        case LY_FUNC:
            if (clockwise) {
                tap_code(KC_MNXT);
            } else {
                tap_code(KC_MPRV);
            }
            break;
    }

    return false;
}

void flash_and_reset(void) {
    SEND_STRING("qmk flash -j 24" SS_TAP(X_ENTER));
    eeconfig_init();
    reset_keyboard();
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (calc_is_in_progress())
        return calc_process_input(keycode, record);

    switch (keycode) {
        case KC_CAPS:
            if (record->event.pressed) {
                _globals.color.capslock.r = rand() % 255;
                _globals.color.capslock.g = rand() % 255;
                _globals.color.capslock.b = rand() % 255;
                return true;
            }
            break;

        case KC_FSH:
            if (record->event.pressed) {
                flash_and_reset();
                return false;
            }
            break;

        case KC_EMO:
            if (record->event.pressed) {
                qk_ucis_start();
                return false;
            }
            break;

        case KC_WPM:
            if (record->event.pressed) {
                uint8_t wpm = get_current_wpm();
                char buf[11] = {0};
                sprintf(buf, "%d", wpm);
                send_string(buf);
            }
            break;

        case KC_CAL:
            if (record->event.pressed) {
                calc_start();
                return false;
            }
            break;
    }

    return process_record_secrets(keycode, record);
}

void qk_ucis_start_user(void) {
}
