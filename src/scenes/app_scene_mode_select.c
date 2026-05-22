#include "app_scene_i.h"

#include "app_scene_ui.h"

static bool app_scene_mode_select_has_server_url(const SpoolmanSyncApp* app) {
    return app && app->server.base_url && !furi_string_empty(app->server.base_url);
}

static void app_scene_mode_select_draw_status(Canvas* canvas, const SpoolmanSyncApp* app) {
    if(!furi_string_empty(app->server.info_message)) {
        char line[25];
        app_ui_copy_truncated(
            line,
            sizeof(line),
            furi_string_get_cstr(app->server.info_message),
            sizeof(line) - 1);
        canvas_draw_str(canvas, APP_UI_TEXT_LEFT, 20, line);
        return;
    }

    if(app->status == AppStatusTestingBaseUrl) {
        canvas_draw_str(canvas, APP_UI_TEXT_LEFT, 20, "Checking server...");
        return;
    }

    if(!app_scene_mode_select_has_server_url(app)) {
        canvas_draw_str(canvas, APP_UI_TEXT_LEFT, 20, "Setup needed: add server");
    }
}

static void app_scene_mode_select_draw_item(
    Canvas* canvas,
    uint8_t row_y,
    bool selected,
    bool enabled,
    const char* label) {
    char line[25];
    snprintf(line, sizeof(line), "%c %s%s", selected ? '>' : ' ', label, enabled ? "" : " *");
    canvas_draw_str(canvas, APP_UI_TEXT_LEFT, row_y, line);
}

void app_scene_mode_select_draw(Canvas* canvas, const SpoolmanSyncApp* app) {
    bool has_server_url = app_scene_mode_select_has_server_url(app);

    app_ui_draw_title(canvas, "Spoolman Sync");
    app_scene_mode_select_draw_status(canvas, app);
    app_scene_mode_select_draw_item(
        canvas, 26, app->selected_mode == AppModeCreate, has_server_url, "Create a spool");
    app_scene_mode_select_draw_item(
        canvas, 35, app->selected_mode == AppModeFind, has_server_url, "Find a spool");
    app_scene_mode_select_draw_item(
        canvas, 44, app->selected_mode == AppModeUpdate, has_server_url, "Tag existing spools");
    app_scene_mode_select_draw_item(
        canvas, 53, app->selected_mode == AppModeScan, true, "Read raw spool tag");
    app_scene_mode_select_draw_item(
        canvas, 62, app->selected_mode == AppModeConfig, true, "Server settings");
}

bool app_scene_mode_select_handle_input(SpoolmanSyncApp* app, const InputEvent* event) {
    bool has_server_url = app_scene_mode_select_has_server_url(app);

    if(event->key == InputKeyUp || event->key == InputKeyDown) {
        furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
        if(event->key == InputKeyDown) {
            app->selected_mode = (app->selected_mode + 1) % 5;
        } else {
            app->selected_mode =
                app->selected_mode == AppModeCreate ? AppModeConfig : app->selected_mode - 1;
        }
        furi_mutex_release(app->runtime.mutex);
        view_port_update(app->runtime.view_port);
        return true;
    }

    if(event->key == InputKeyOk) {
        if(app->selected_mode == AppModeCreate && has_server_url) {
            app_start_create_mode(app);
        } else if(app->selected_mode == AppModeFind && has_server_url) {
            app_start_find_mode(app);
        } else if(app->selected_mode == AppModeUpdate && has_server_url) {
            app_start_update_mode(app);
        } else if(app->selected_mode == AppModeScan) {
            app_start_scan_mode(app);
        } else if(app->selected_mode == AppModeConfig) {
            app_open_base_url_editor(app);
        } else if(!has_server_url) {
            furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
            furi_string_set_str(app->server.info_message, "Add server URL first");
            furi_mutex_release(app->runtime.mutex);
            app_open_base_url_editor(app);
        }
        return true;
    }

    return false;
}
