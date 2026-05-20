#include "app_page.h"

#include <stdio.h>

void app_page_spoolman_draw(Canvas* canvas, const SpoolmanSyncApp* app) {
    if(app->status == AppStatusCheckingSpoolman) {
        canvas_draw_str(canvas, 10, 15, "Spoolman Sync");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 10, 35, "Checking Spoolman...");
        return;
    }

    canvas_draw_str(canvas, 10, 15, "Spoolman error");
    canvas_set_font(canvas, FontSecondary);
    if(app->spoolman_status_code > 0) {
        char status[24];
        snprintf(status, sizeof(status), "HTTP status: %d", app->spoolman_status_code);
        canvas_draw_str(canvas, 5, 31, status);
    } else {
        canvas_draw_str(canvas, 5, 31, "No HTTP response");
    }
    canvas_draw_str(canvas, 5, 45, furi_string_get_cstr(app->spoolman_error));
    canvas_draw_str(canvas, 5, 60, "OK retry / UP edit URL");
}

bool app_page_spoolman_handle_input(SpoolmanSyncApp* app, const InputEvent* event) {
    if(app->status == AppStatusSpoolmanError && event->key == InputKeyOk) {
        app_check_spoolman_health(app);
        return true;
    }

    if(app->status == AppStatusSpoolmanError && event->key == InputKeyUp) {
        app_open_base_url_editor(app);
        return true;
    }

    return false;
}
