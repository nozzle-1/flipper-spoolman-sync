#include "app_scene_i.h"

#include "app_scene_ui.h"

void app_scene_spoolman_draw(Canvas* canvas, const SpoolmanSyncApp* app) {
    char line[25];

    if(app->status == AppStatusCheckingSpoolman) {
        app_ui_draw_title(canvas, "Checking server");
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 34, "Connecting to Spoolman");
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 46, "Please wait...");
        return;
    }

    app_ui_draw_title(canvas, "Server unavailable");
    if(app->server.status_code > 0) {
        snprintf(line, sizeof(line), "HTTP status: %d", app->server.status_code);
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 30, line);
    } else {
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 30, "No response from server");
    }
    app_ui_copy_truncated(
        line, sizeof(line), furi_string_get_cstr(app->server.error), sizeof(line) - 1);
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 44, line);
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 55, "OK Retry");
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "UP Edit server");
}

bool app_scene_spoolman_handle_input(SpoolmanSyncApp* app, const InputEvent* event) {
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
