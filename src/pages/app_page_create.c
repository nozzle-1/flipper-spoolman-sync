#include "app_page.h"

#include <stdio.h>

static bool app_page_create_parse_existing_tag_error(const char* error_text, int* spool_id) {
    if(!error_text || !spool_id) {
        return false;
    }

    return sscanf(error_text, "Tag already defined on #%d", spool_id) == 1;
}

static void app_page_create_draw_error(Canvas* canvas, const SpoolmanSyncApp* app) {
    char line[25];
    const char* error_text =
        furi_string_empty(app->create_error) ? "Create failed" : furi_string_get_cstr(app->create_error);
    int existing_spool_id = 0;
    bool tag_exists_error = app_page_create_parse_existing_tag_error(error_text, &existing_spool_id);

    if(tag_exists_error) {
        char header_line[25];
        char name_line[25];
        char weight_line[25];

        if(app->create_existing_spool_id > 0) {
            existing_spool_id = app->create_existing_spool_id;
        }

        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 20, "Tag already exists");
        snprintf(
            header_line,
            sizeof(header_line),
            "#%d %.15s",
            existing_spool_id,
            furi_string_get_cstr(app->create_existing_spool_material));
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 32, header_line);
        app_ui_copy_truncated(
            name_line, sizeof(name_line), furi_string_get_cstr(app->create_existing_spool_name), 24);
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 42, name_line);
        if(app->create_existing_spool_has_remaining_weight) {
            snprintf(
                weight_line,
                sizeof(weight_line),
                "Remain: %ug",
                app_ui_weight_to_grams(app->create_existing_spool_weight));
        } else {
            snprintf(weight_line, sizeof(weight_line), "Remain: N/A");
        }
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 52, weight_line);
    } else {
        app_ui_copy_truncated(line, sizeof(line), error_text, sizeof(line) - 1);
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 28, line);
    }

    if(app->spoolman_status_code > 0 && !tag_exists_error) {
        snprintf(line, sizeof(line), "HTTP %d", app->spoolman_status_code);
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 42, line);
    }
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "OK Try again");
}

static void app_page_create_draw_confirm(Canvas* canvas, const SpoolmanSyncApp* app) {
    char material_line[25];
    char name_line[25];
    char weight_line[25];
    snprintf(
        material_line,
        sizeof(material_line),
        "Material: %.14s",
        furi_string_get_cstr(app->create_filament_material));
    app_ui_copy_truncated(
        name_line, sizeof(name_line), furi_string_get_cstr(app->create_filament_name), 20);

    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 24, "Create this spool?");
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 34, material_line);
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 44, name_line);
    if(app->create_filament_weight > 0) {
        snprintf(
            weight_line,
            sizeof(weight_line),
            "Nominal: %ug",
            app_ui_weight_to_grams(app->create_filament_weight));
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 54, weight_line);
    }
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "OK Create spool");
}

void app_page_create_draw(Canvas* canvas, const SpoolmanSyncApp* app) {
    app_ui_draw_title(canvas, "Create a spool");

    if(app->status == AppStatusCreateReading) {
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 32, "Hold spool near reader");
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 46, "Reading RFID tag...");
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "BACK Cancel");
        return;
    }

    if(app->status == AppStatusCreateSearching) {
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 32, "Matching filament");
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 46, "in Spoolman...");
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "Please wait");
        return;
    }

    if(app->status == AppStatusCreateConfirm) {
        app_page_create_draw_confirm(canvas, app);
        return;
    }

    if(app->status == AppStatusCreateSaving) {
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 32, "Creating spool");
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 46, "in Spoolman...");
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "Please wait");
        return;
    }

    if(app->status == AppStatusCreateSuccess) {
        char spool_line[32];
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 32, "Spool created");
        if(app->create_spool_id > 0) {
            snprintf(spool_line, sizeof(spool_line), "Spool #%d", app->create_spool_id);
            canvas_draw_str(canvas, APP_UI_BODY_LEFT, 46, spool_line);
        }
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "OK Scan another");
        return;
    }

    if(app->status == AppStatusCreateError) {
        app_page_create_draw_error(canvas, app);
        return;
    }

    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 32, "Scan a Bambu spool");
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 46, "to create it online");
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "OK Start scan");
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
