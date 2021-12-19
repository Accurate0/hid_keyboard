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

globals_t _globals = {.key =
                          {
                              .last_press = 0,
                              .last_encoder = 0,

                          },
                      .rgb =
                          {
                              .enabled = true,

                          },
                      .color = {.capslock = {RGB_GREEN}},
                      .hid = {
                          .available = false,
                          .mute = false,
                          .volume = 0,
                      }};

#define RGB_IDLE_TIMEOUT   (5 * 60 * 1000)
#define RGB_VOLUME_TIMEOUT (1 * 60 * 1000)

void keyboard_post_init_user(void) {
    // setup initial values
    _globals.key.last_encoder = timer_read32();
    _globals.key.last_press = timer_read32();
    _globals.rgb.enabled = rgblight_is_enabled();
}

void matrix_scan_user(void) {
    // 1000 == 1 second
    if (_globals.rgb.enabled && timer_elapsed32(_globals.key.last_press) > RGB_IDLE_TIMEOUT) {
        _globals.rgb.enabled = false;
        rgblight_disable_noeeprom();
    }

    if (_globals.hid.available && timer_elapsed32(_globals.key.last_encoder) > RGB_VOLUME_TIMEOUT) {
        _globals.hid.available = false;
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
    }
}

bool encoder_update_user(uint8_t index, bool clockwise) {
    switch (get_highest_layer(layer_state)) {
        case LY_BASE: {
            _globals.key.last_encoder = timer_read32();

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

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    _globals.key.last_press = timer_read32();
    if (!_globals.rgb.enabled && record->event.pressed) {
        _globals.rgb.enabled = true;
        rgblight_enable_noeeprom();
    }

    if (calc_is_in_progress())
        return calc_process_input(keycode, record);

    switch (keycode) {
        case KC_MUTE:
            if (record->event.pressed) {
                _globals.key.last_encoder = timer_read32();
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
                calc_start();
                return false;
            }
            break;
    }

    return process_record_secrets(keycode, record);
}
