#pragma once

#include "app.h"

#include <stdio.h>
#include <string.h>

#define APP_UI_TITLE_Y 11
#define APP_UI_TEXT_LEFT 4
#define APP_UI_BODY_LEFT 5
#define APP_UI_ROW_1_Y 24
#define APP_UI_ROW_2_Y 34
#define APP_UI_ROW_3_Y 44
#define APP_UI_ROW_4_Y 54
#define APP_UI_FOOTER_Y 63

static inline void app_ui_copy_truncated(
    char* output,
    size_t output_size,
    const char* input,
    size_t visible_chars) {
    if(!output || output_size == 0) {
        return;
    }

    output[0] = '\0';
    if(!input) {
        return;
    }

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
    if(weight <= 0) {
        return 0;
    }

    return (unsigned int)weight;
}

void app_page_spoolman_draw(Canvas* canvas, const SpoolmanSyncApp* app);

bool app_page_spoolman_handle_input(SpoolmanSyncApp* app, const InputEvent* event);

void app_page_mode_select_draw(Canvas* canvas, const SpoolmanSyncApp* app);

bool app_page_mode_select_handle_input(SpoolmanSyncApp* app, const InputEvent* event);

void app_page_update_draw(Canvas* canvas, const SpoolmanSyncApp* app);

bool app_page_update_handle_input(SpoolmanSyncApp* app, const InputEvent* event);

void app_page_find_draw(Canvas* canvas, const SpoolmanSyncApp* app);

bool app_page_find_handle_input(SpoolmanSyncApp* app, const InputEvent* event);

void app_page_scan_draw(Canvas* canvas, const SpoolmanSyncApp* app);

bool app_page_scan_handle_input(SpoolmanSyncApp* app, const InputEvent* event);

void app_page_create_draw(Canvas* canvas, const SpoolmanSyncApp* app);

bool app_page_create_handle_input(SpoolmanSyncApp* app, const InputEvent* event);
