// !!! DO NOT ADD #pragma once !!! //

// Step 1.
// Declare custom effects using the RGB_MATRIX_EFFECT macro
// (note the lack of semicolon after the macro!)
RGB_MATRIX_EFFECT(REACTIVE)
// Step 2.
// Define effects inside the `RGB_MATRIX_CUSTOM_EFFECT_IMPLS` ifdef block
#ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS
#include "accurate0.h"
extern uint8_t volume;

static HSV REACTIVE_math(HSV hsv, int16_t dx, int16_t dy, uint8_t dist, uint16_t tick) {
    uint16_t effect = tick + dist;
    dx              = dx < 0 ? dx * -1 : dx;
    dy              = dy < 0 ? dy * -1 : dy;
    dx              = dx * 16 > 255 ? 255 : dx * 16;
    dy              = dy * 16 > 255 ? 255 : dy * 16;
    effect += dx > dy ? dy : dx;
    if (effect > 255) effect = 255;
    hsv.v = qadd8(hsv.v, 255 - effect);
    return hsv;
}

typedef HSV (*reactive_splash_f)(HSV hsv, int16_t dx, int16_t dy, uint8_t dist, uint16_t tick);

bool REACTIVE_effect_runner(uint8_t start, effect_params_t* params, reactive_splash_f effect_func) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    uint8_t count = g_last_hit_tracker.count;
    // left side light
    for(int i = 0; i < LED_COUNT_PER_SIDE; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        HSV hsv = { 9, 150, 0 };
         for (uint8_t j = start; j < count; j++) {
            int16_t  dx   = g_led_config.point[left_side_leds[i]].x - g_last_hit_tracker.x[left_side_leds[i]];
            int16_t  dy   = g_led_config.point[left_side_leds[i]].y - g_last_hit_tracker.y[left_side_leds[i]];
            // uint8_t  dist = sqrt16(dx * dx + dy * dy);
            uint16_t tick = scale16by8(g_last_hit_tracker.tick[j], rgb_matrix_config.speed);
            hsv           = effect_func(hsv, dx, dy, 0, tick);
        }
        hsv.v   = scale8(hsv.v, 255);
        RGB rgb = rgb_matrix_hsv_to_rgb(hsv);
        rgb_matrix_set_color(left_side_leds[i], rgb.r, rgb.g, rgb.b);
    }

    uint32_t total_count = volume * 20;
    // right side
    for(int i = LED_COUNT_PER_SIDE - 1; i >= 0; i--) {
        if(total_count < 0) {
            rgb_matrix_set_color(right_side_leds[i], RGB_BLACK);
            continue;
        }

        uint8_t value = 0;
        if(total_count > 255) {
            value = 255;
        } else {
            value = total_count;
        }

        HSV hsv = { 9, 150, value };
        RGB rgb = rgb_matrix_hsv_to_rgb(hsv);
        rgb_matrix_set_color(right_side_leds[i], rgb.r, rgb.g, rgb.b);

        total_count -= value;
    }

    return led_max < DRIVER_LED_TOTAL;
}

// e.g: A simple effect, self-contained within a single method
bool REACTIVE(effect_params_t* params) {
    return REACTIVE_effect_runner(qsub8(g_last_hit_tracker.count, 1), params, &REACTIVE_math);
}

#endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS

#define RGB_MATRIX_STARTUP_MODE RGB_MATRIX_CUSTOM_REACTIVE
