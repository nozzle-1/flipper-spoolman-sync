#include "app_page.h"

#include <stdio.h>

static void app_page_find_draw_error(Canvas* canvas, const SpoolmanSyncApp* app) {
    char line[25];
    const char* error_text =
        furi_string_empty(app->find_error) ? "Lookup failed" : furi_string_get_cstr(app->find_error);
    app_ui_copy_truncated(line, sizeof(line), error_text, sizeof(line) - 1);
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 32, line);
    if(app->spoolman_status_code > 0) {
        snprintf(line, sizeof(line), "HTTP %d", app->spoolman_status_code);
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 46, line);
    }
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "OK Try again");
}

static void app_page_find_draw_success(Canvas* canvas, const SpoolmanSyncApp* app) {
    char header_line[25];
    char name_line[25];
    char weight_line[25];

    snprintf(
        header_line,
        sizeof(header_line),
        "#%d %.15s",
        app->find_spool_id,
        furi_string_get_cstr(app->find_filament_material));
    app_ui_copy_truncated(
        name_line, sizeof(name_line), furi_string_get_cstr(app->find_filament_name), 24);

    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 24, header_line);
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 34, name_line);
    if(app->find_spool_has_remaining_weight) {
        snprintf(
            weight_line,
            sizeof(weight_line),
            "Remain: %ug",
            app_ui_weight_to_grams(app->find_spool_weight));
    } else {
        snprintf(weight_line, sizeof(weight_line), "Remain: N/A");
    }
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 46, weight_line);
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "OK Scan again");
}

void app_page_find_draw(Canvas* canvas, const SpoolmanSyncApp* app) {
    app_ui_draw_title(canvas, "Find a spool");

    if(app->status == AppStatusFindReading) {
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 32, "Hold spool near reader");
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 46, "Reading RFID tag...");
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "BACK Cancel");
        return;
    }

    if(app->status == AppStatusFindSearching) {
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 32, "Searching Spoolman");
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 46, "for this spool...");
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "Please wait");
        return;
    }

    if(app->status == AppStatusFindSuccess) {
        app_page_find_draw_success(canvas, app);
        return;
    }

    if(app->status == AppStatusFindError) {
        app_page_find_draw_error(canvas, app);
        return;
    }

    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 32, "Scan a Bambu spool");
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 46, "to find it online");
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "OK Start scan");
}

bool app_page_find_handle_input(SpoolmanSyncApp* app, const InputEvent* event) {
    if(event->key == InputKeyOk &&
       (app->status == AppStatusFindReady || app->status == AppStatusFindSuccess ||
        app->status == AppStatusFindError)) {
        app_start_find_scan(app);
        return true;
    }

    return false;
}
