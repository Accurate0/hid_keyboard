#include "accurate0.h"
#include "calc.h"
#include "hid_commands.h"

// define the via protocol version supported by the raw_hid_driver
// ensure we expect the right via id, else raw_hid_handler won't be called
#define SUPPORTED_VIA_PROTOCOL_VERSION 0x0009

#if VIA_PROTOCOL_VERSION != SUPPORTED_VIA_PROTOCOL_VERSION
#warning VIA COMMAND ID FOR LIGHTING MAY HAVE CHANGED
#error check: https://github.com/qmk/qmk_firmware/blob/master/quantum/via.h#L68
#endif

__attribute__((weak)) bool process_record_secrets(uint16_t keycode, keyrecord_t *record) {
    return true;
}

const qk_ucis_symbol_t ucis_symbol_table[] =
    UCIS_TABLE(UCIS_SYM("eyes", 0x1F441, 0x1F445, 0x1F441) //👁👅👁
    );

void rgbtimeout_check(bool pressed);

globals_t _globals = {.color =
                          {
                              .capslock = {RGB_GREEN},
                          },
                      .hid =
                          {
                              .available = false,
                              .mute = false,
                              .volume = 0,
                          },
                      .keepalive = {
                          .enabled = false,
                      }};

eeprom_config_t _eeprom_config;

#define KEEPALIVE_TIME_BETWEEN (1 * MINUTES)

void eeconfig_init_user(void) {
    _eeprom_config.raw = 0;
    _eeprom_config.hid.disabled = true;
    _eeprom_config.effect.disabled = true;
    eeconfig_update_user(_eeprom_config.raw);
}

void keyboard_post_init_user(void) {
    // setup initial values
    _globals.keepalive.last_keepalive = timer_read32();
    _eeprom_config.raw = eeconfig_read_user();
}

void matrix_scan_user(void) {
    if (_globals.keepalive.enabled &&
        timer_elapsed32(_globals.keepalive.last_keepalive) > KEEPALIVE_TIME_BETWEEN)
    {
        // send a f13 every minute when this is enabled
        _globals.keepalive.last_keepalive = timer_read32();
        // gnome has a keybind for F13...
        tap_code(KC_F24);
    }
}

void raw_hid_receive_kb(uint8_t *data, uint8_t length) {
    if (data[0] != VIA_LIGHTING_SET_VALUE) {
        // via just called us with the wrong command id
        return;
    }

    uint8_t command = data[1];
    uint8_t command_data = data[2];

    // if we receive any data
    _globals.hid.available = true;

    switch (command) {
        case VOLUME_COMMAND: {
            _globals.hid.volume = command_data;
            dprintf("vol: %d\n", _globals.hid.volume);
        } break;

        case MUTE_COMMAND: {
            _globals.hid.mute = (bool)command_data;
            dprintf("mute: %d\n", _globals.hid.mute);
        } break;

        case CALC_REPLY: {
            const char *answer = (char *)(data + 2);
            dprintf("calc: %s\n", answer);
            send_string(answer);
        } break;
    }
}

bool encoder_update_user(uint8_t index, bool clockwise) {
    switch (get_highest_layer(layer_state)) {
        case LY_BASE: {
            if (clockwise) {
                tap_code(KC_VOLU);
            } else {
                tap_code(KC_VOLD);
            }

        } break;

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

void keepalive_toggle(void) {
    _globals.keepalive.enabled = !_globals.keepalive.enabled;
}

void rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    if (host_keyboard_led_state().caps_lock) {
        rgb_matrix_set_color(86, _globals.color.capslock.r, _globals.color.capslock.b,
                             _globals.color.capslock.g);
    }

    uint8_t layer = get_highest_layer(layer_state);
    if (layer > 0) {
        for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
            for (uint8_t col = 0; col < MATRIX_COLS; ++col) {
                uint8_t index = g_led_config.matrix_co[row][col];

                if (index >= led_min && index <= led_max && index != NO_LED &&
                    keymap_key_to_keycode(layer, (keypos_t){col, row}) > KC_TRNS)
                {
                    rgb_matrix_set_color(index, RGB_PURPLE);
                }
            }
        }
    }

    if (_globals.keepalive.enabled) {
        rgb_matrix_set_color(89, RGB_RED);
    }
}
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (calc_is_in_progress())
        return calc_process_input(keycode, record);

    switch (keycode) {
        case KC_MUTE:
            if (record->event.pressed) {
                return true;
            }
            break;

        case KC_CAPS:
            if (record->event.pressed && !host_keyboard_led_state().caps_lock) {
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
                // calc_start();
                return false;
            }
            break;

        case KC_KAL:
            if (record->event.pressed) {
                keepalive_toggle();
                return false;
            }
            break;

        case KC_RSL:
            // reset light to default
            if (record->event.pressed) {
                rgb_matrix_set_color_all(0, 0, 0);
                rgb_matrix_mode_noeeprom(RGB_MATRIX_CUSTOM_REACTIVE);
                return false;
            }
            break;

        case KC_NXT:
            if (record->event.pressed) {
                rgb_matrix_step_noeeprom();
                return false;
            }
            break;

        case KC_THV:
            if (record->event.pressed) {
                _eeprom_config.hid.disabled = !_eeprom_config.hid.disabled;
                eeconfig_update_user(_eeprom_config.raw);
                return false;
            }
            break;

        case KC_EFF:
            if (record->event.pressed) {
                _eeprom_config.effect.disabled = !_eeprom_config.effect.disabled;
                eeconfig_update_user(_eeprom_config.raw);
                return false;
            }
            break;
    }

    return process_record_secrets(keycode, record);
}
