#include "app_page.h"

#include <stdio.h>

static bool app_page_mode_select_has_server_url(const SpoolmanSyncApp* app) {
    return app && app->spoolman_base_url && !furi_string_empty(app->spoolman_base_url);
}

static void app_page_mode_select_draw_url(Canvas* canvas, const SpoolmanSyncApp* app) {
    if(!app_page_mode_select_has_server_url(app)) {
        canvas_draw_str(canvas, 10, 22, "No server URL configured");
        return;
    }

    char url_line[23];
    snprintf(url_line, sizeof(url_line), "%.22s", furi_string_get_cstr(app->spoolman_base_url));
    canvas_draw_str(canvas, 10, 22, url_line);
}

void app_page_mode_select_draw(Canvas* canvas, const SpoolmanSyncApp* app) {
    bool has_server_url = app_page_mode_select_has_server_url(app);

    canvas_draw_str(canvas, 10, 15, "Spoolman Sync");

    canvas_set_font(canvas, FontSecondary);
    if(!furi_string_empty(app->info_message)) {
        canvas_draw_str(canvas, 10, 22, furi_string_get_cstr(app->info_message));
    } else if(app->status == AppStatusTestingBaseUrl) {
        canvas_draw_str(canvas, 10, 22, "Testing Spoolman...");
    } else {
        app_page_mode_select_draw_url(canvas, app);
    }
    canvas_draw_str(
        canvas,
        10,
        32,
        app->selected_mode == AppModeUpdate ?
            (has_server_url ? "> 1. Update mode" : "> 1. Update mode [disabled]") :
            (has_server_url ? "  1. Update mode" : "  1. Update mode [disabled]"));
    canvas_draw_str(
        canvas,
        10,
        42,
        app->selected_mode == AppModeScan ? "> 2. Scan spool" : "  2. Scan spool");
    canvas_draw_str(
        canvas,
        10,
        52,
        app->selected_mode == AppModeCreate ?
            (has_server_url ? "> 3. Create spool" : "> 3. Create spool [disabled]") :
            (has_server_url ? "  3. Create spool" : "  3. Create spool [disabled]"));
    canvas_draw_str(
        canvas,
        10,
        62,
        app->selected_mode == AppModeConfig ? "> 4. Server URL" : "  4. Server URL");
}

bool app_page_mode_select_handle_input(SpoolmanSyncApp* app, const InputEvent* event) {
    bool has_server_url = app_page_mode_select_has_server_url(app);

    if(event->key == InputKeyUp || event->key == InputKeyDown) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(event->key == InputKeyDown) {
            app->selected_mode = (app->selected_mode + 1) % 4;
        } else {
            app->selected_mode =
                app->selected_mode == AppModeUpdate ? AppModeConfig : app->selected_mode - 1;
        }
        furi_mutex_release(app->mutex);
        view_port_update(app->view_port);
        return true;
    }

    if(event->key == InputKeyOk) {
        if(app->selected_mode == AppModeUpdate && has_server_url) {
            app_start_update_mode(app);
        }
        if(app->selected_mode == AppModeScan) {
            app_start_scan_mode(app);
        }
        if(app->selected_mode == AppModeCreate && has_server_url) {
            app_start_create_mode(app);
        }
        if(app->selected_mode == AppModeConfig) {
            app_open_base_url_editor(app);
        }
        return true;
    }

    return false;
}
