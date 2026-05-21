#include "app_page.h"

#include <stdio.h>

static void app_page_create_draw_error(Canvas* canvas, const SpoolmanSyncApp* app) {
    const char* error_text =
        furi_string_empty(app->create_error) ? "Create failed" : furi_string_get_cstr(app->create_error);
    canvas_draw_str(canvas, 5, 32, error_text);
    if(app->spoolman_status_code > 0) {
        char status[24];
        snprintf(status, sizeof(status), "HTTP %d", app->spoolman_status_code);
        canvas_draw_str(canvas, 5, 47, status);
    }
    canvas_draw_str(canvas, 5, 63, "OK retry / BACK menu");
}

static void app_page_create_draw_confirm(Canvas* canvas, const SpoolmanSyncApp* app) {
    char id_line[24];
    char material_line[40];
    char name_line[64];
    snprintf(id_line, sizeof(id_line), "ID: %d", app->create_filament_id);
    snprintf(
        material_line,
        sizeof(material_line),
        "Mat: %s",
        furi_string_get_cstr(app->create_filament_material));
    snprintf(name_line, sizeof(name_line), "Nom: %s", furi_string_get_cstr(app->create_filament_name));

    canvas_draw_str(canvas, 5, 26, id_line);
    canvas_draw_str(canvas, 5, 39, material_line);
    canvas_draw_str(canvas, 5, 52, name_line);
    canvas_draw_str(canvas, 5, 63, "OK confirm / BACK menu");
}

void app_page_create_draw(Canvas* canvas, const SpoolmanSyncApp* app) {
    canvas_draw_str(canvas, 10, 15, "Create spool");
    canvas_set_font(canvas, FontSecondary);

    if(app->status == AppStatusCreateReading) {
        canvas_draw_str(canvas, 5, 35, "Scanning tag...");
        canvas_draw_str(canvas, 5, 50, "Reading spool RFID");
        canvas_draw_str(canvas, 5, 63, "BACK: cancel");
        return;
    }

    if(app->status == AppStatusCreateSearching) {
        canvas_draw_str(canvas, 5, 35, "Searching filament");
        canvas_draw_str(canvas, 5, 50, "in Spoolman...");
        canvas_draw_str(canvas, 5, 63, "Wait...");
        return;
    }

    if(app->status == AppStatusCreateConfirm) {
        app_page_create_draw_confirm(canvas, app);
        return;
    }

    if(app->status == AppStatusCreateSaving) {
        canvas_draw_str(canvas, 5, 35, "Creating spool");
        canvas_draw_str(canvas, 5, 50, "in Spoolman...");
        canvas_draw_str(canvas, 5, 63, "Wait...");
        return;
    }

    if(app->status == AppStatusCreateSuccess) {
        char spool_line[32];
        canvas_draw_str(canvas, 5, 35, "Spool created");
        if(app->create_spool_id > 0) {
            snprintf(spool_line, sizeof(spool_line), "Spool #%d", app->create_spool_id);
            canvas_draw_str(canvas, 5, 50, spool_line);
        }
        canvas_draw_str(canvas, 5, 63, "OK scan another");
        return;
    }

    if(app->status == AppStatusCreateError) {
        app_page_create_draw_error(canvas, app);
        return;
    }

    canvas_draw_str(canvas, 5, 35, "Scan a Bambu spool");
    canvas_draw_str(canvas, 5, 50, "to create it");
    canvas_draw_str(canvas, 5, 63, "OK scan / BACK menu");
}

bool app_page_create_handle_input(SpoolmanSyncApp* app, const InputEvent* event) {
    if(event->key == InputKeyOk &&
       (app->status == AppStatusCreateReady || app->status == AppStatusCreateSuccess ||
        app->status == AppStatusCreateError)) {
        app_start_create_scan(app);
        return true;
    }

    if(event->key == InputKeyOk && app->status == AppStatusCreateConfirm) {
        app_confirm_create_spool(app);
        return true;
    }

    return false;
}
