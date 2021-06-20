#include "accurate0.h"
#include "calc.h"
#include "libc.h"

__attribute__ ((weak))
bool process_record_secrets(uint16_t keycode, keyrecord_t *record) {
    return true;
}

const qk_ucis_symbol_t ucis_symbol_table[] = UCIS_TABLE(
    UCIS_SYM("eyes", 0x1F441, 0x1F445, 0x1F441) //👁👅👁
);

RGB caps_colour = { RGB_GREEN };

void rgb_matrix_indicators_user(void) {
    if (host_keyboard_led_state().caps_lock) {
        // 82-89 left side light
        for(int i = 82; i <= 89; i++) {
            rgb_matrix_set_color(i, caps_colour.r, caps_colour.g, caps_colour.b);
        }
    }
}

bool raw_hid_available = false;
uint8_t volume = 80;
#define VOLUME 0x1
// TODO: handle mute events :) switch colour to red ?
void raw_hid_receive(uint8_t *data, uint8_t length) {
    uint8_t command = data[0];

    switch(command) {
        case VOLUME:
        {
            raw_hid_available = true;
            volume = data[1];
            dprintf("%d\n", volume);
        }
        break;
    }
}

bool encoder_update_user(uint8_t index, bool clockwise) {
    switch(get_highest_layer(layer_state)) {
        case LY_BASE:
            if (clockwise) {
                tap_code(KC_VOLU);
                if(!raw_hid_available && volume < 100)
                    volume += 2;
            } else {
                tap_code(KC_VOLD);
                if(!raw_hid_available && volume > 0)
                    volume -= 2;
            }
        break;

        case LY_FUNC:
            if (clockwise)
                tap_code(KC_MNXT);
            else
                tap_code(KC_MPRV);
        break;
    }

    return false;
}

void flash_and_reset(bool secrets) {
    if(secrets)
        SEND_STRING("qmk flash -j 4 -e SECRETS=yes" SS_TAP(X_ENTER));
    else
        SEND_STRING("qmk flash -j 4" SS_TAP(X_ENTER));

    eeconfig_init();
    reset_keyboard();
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if(calc_is_in_progress())
        return calc_process_input(keycode, record);

    switch(keycode) {
        case KC_CAPS:
            if(record->event.pressed) {
                caps_colour.r = rand() % 255;
                caps_colour.g = rand() % 255;
                caps_colour.b = rand() % 255;
                return true;
            }
            break;

        case KC_FSH:
            if(record->event.pressed) {
                flash_and_reset(false);
                return false;
            }
            break;

        case KC_FSHS:
            if(record->event.pressed) {
                flash_and_reset(true);
                return false;
            }
            break;

        case KC_EMO:
            if(record->event.pressed) {
                qk_ucis_start();
                return false;
            }
            break;

        case KC_WPM:
            if(record->event.pressed) {
                uint8_t wpm = get_current_wpm();
                static const char integer_map[] = { "0123456789" };

                // int max '2147483647'
                char buf[11] = { 0 };

                for(int i = 0; wpm > 0; wpm /= 10, i++) {
                    buf[i] = integer_map[wpm % 10];
                }

                reverse(buf);
                send_string(buf);
            }
            break;

        case KC_CAL:
            if(record->event.pressed) {
                calc_start();
                return false;
            }
            break;
    }

    return process_record_secrets(keycode, record);
}

void qk_ucis_start_user(void) {}
