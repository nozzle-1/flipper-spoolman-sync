#pragma once

#include "../app_i.h"

#include <stdio.h>
#include <string.h>

#define APP_UI_TITLE_Y 11
#define APP_UI_TEXT_LEFT 4
#define APP_UI_BODY_LEFT 5
#define APP_UI_FOOTER_Y 63

static inline void app_ui_copy_truncated(
    char* output,
    size_t output_size,
    const char* input,
    size_t visible_chars) {
    if(!output || output_size == 0) return;
    output[0] = '\0';
    if(!input) return;

    size_t input_len = strlen(input);
    if(input_len <= visible_chars) {
        snprintf(output, output_size, "%s", input);
    } else if(visible_chars > 3) {
        snprintf(output, output_size, "%.*s...", (int)(visible_chars - 3), input);
    } else {
        snprintf(output, output_size, "%.*s", (int)visible_chars, input);
    }
}

static inline void app_ui_draw_title(Canvas* canvas, const char* title) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, APP_UI_TEXT_LEFT, APP_UI_TITLE_Y, title);
    canvas_set_font(canvas, FontSecondary);
}

static inline unsigned int app_ui_weight_to_grams(double weight) {
    return weight <= 0 ? 0U : (unsigned int)weight;
}
