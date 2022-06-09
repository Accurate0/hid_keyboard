RGB_MATRIX_EFFECT(REACTIVE)
#ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS
#include "accurate0.h"

static HSV REACTIVE_math(HSV hsv, int16_t dx, int16_t dy, uint8_t dist, uint16_t tick) {
    uint16_t effect = tick + dist;
    dx = dx < 0 ? dx * -1 : dx;
    dy = dy < 0 ? dy * -1 : dy;
    dx = dx * 16 > 255 ? 255 : dx * 16;
    dy = dy * 16 > 255 ? 255 : dy * 16;
    effect += dx > dy ? dy : dx;
    if (effect > 255)
        effect = 255;
    hsv.v = qadd8(hsv.v, 255 - effect);
    return hsv;
}

typedef HSV (*reactive_splash_f)(HSV hsv, int16_t dx, int16_t dy, uint8_t dist, uint16_t tick);

bool REACTIVE_effect_runner(uint8_t start, effect_params_t *params, reactive_splash_f effect_func) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint8_t count = g_last_hit_tracker.count;

    uint8_t layer = get_highest_layer(layer_state);
    if (layer == 0) {
        rgb_matrix_set_color_all(0, 0, 0);
    }

    HSV base = {106, 255, 0};

    // left side light or maybe right if no hid
    if (!_eeprom_config.effect.disabled) {

        uint8_t finish =
            _globals.hid.available && _globals.hid.available && !_eeprom_config.hid.disabled ? 90
                                                                                             : 97;
        for (uint8_t i = 82; i <= finish; i++) {
            RGB_MATRIX_TEST_LED_FLAGS();
            HSV hsv = base;
            for (uint8_t j = start; j < count; j++) {
                int16_t dx = g_led_config.point[i].x - g_last_hit_tracker.x[j];
                int16_t dy = g_led_config.point[i].y - g_last_hit_tracker.y[j];
                uint8_t dist = _globals.hid.available ? 0 : sqrt16(dx * dx + dy * dy);
                uint16_t tick = scale16by8(g_last_hit_tracker.tick[j], rgb_matrix_config.speed);
                hsv = effect_func(hsv, dx, dy, dist, tick);
            }
            hsv.v = scale8(hsv.v, 255);
            RGB rgb = rgb_matrix_hsv_to_rgb(hsv);
            rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
        }
    }

    if (_globals.hid.available && !_eeprom_config.hid.disabled && !_eeprom_config.effect.disabled) {
        uint32_t total_count = _globals.hid.volume * 20;
        // right side
        for (uint8_t i = 97; i >= 90; i--) {
            if (total_count < 0) {
                rgb_matrix_set_color(i, RGB_BLACK);
                continue;
            }
            uint8_t value = 0;
            if (total_count > 255) {
                value = 255;
            } else {
                value = total_count;
            }

            HSV hsv = base;
            hsv.v = value;

            RGB rgb = rgb_matrix_hsv_to_rgb(_globals.hid.mute ? (HSV){0, 255, value} : hsv);
            rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
            total_count -= value;
        }
    }

    return led_max < DRIVER_LED_TOTAL;
}

bool REACTIVE(effect_params_t *params) {
    return REACTIVE_effect_runner(qsub8(g_last_hit_tracker.count, 1), params, &REACTIVE_math);
}

#endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS

#define RGB_MATRIX_STARTUP_MODE RGB_MATRIX_CUSTOM_REACTIVE
