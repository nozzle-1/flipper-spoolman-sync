#include "app_scene_i.h"

#include "app_scene_ui.h"

static void app_scene_update_format_scan_hex(const NfcScanResult* result, char* output) {
    for(size_t i = 0; i < NFC_BLOCK_SIZE; i++) {
        snprintf(&output[i * 2], 3, "%02X", result->blocks[9][i]);
    }
}

static void app_scene_update_draw_summary(Canvas* canvas, const SpoolmanSyncApp* app) {
    char untagged_count[24];
    snprintf(
        untagged_count,
        sizeof(untagged_count),
        "%zu missing / %zu total",
        app->update.untagged_spools_count,
        app->update.total_spools_count);
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 26, "Spools without RFID tag");
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 40, untagged_count);
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 54, "UPDOWN Review list");
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "OK Refresh");
}

static void app_scene_update_draw_detail_status(Canvas* canvas, const SpoolmanSyncApp* app) {
    char line[25];

    if(app->update.patch_in_progress) {
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 52, "Saving tag to Spoolman");
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "Please wait");
        return;
    }

    if(app->update.scan_active) {
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 52, "Hold spool near reader");
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "UPDOWN Skip");
        return;
    }

    if(app->update.patch_error) {
        if(app->server.status_code > 0) {
            snprintf(line, sizeof(line), "HTTP %d", app->server.status_code);
            canvas_draw_str(canvas, APP_UI_BODY_LEFT, 52, line);
        } else if(!furi_string_empty(app->server.error)) {
            app_ui_copy_truncated(
                line, sizeof(line), furi_string_get_cstr(app->server.error), sizeof(line) - 1);
            canvas_draw_str(canvas, APP_UI_BODY_LEFT, 52, line);
        } else {
            canvas_draw_str(canvas, APP_UI_BODY_LEFT, 52, "Could not save tag");
        }
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "OK Retry  UPDOWN");
        return;
    }

    if(app->update.scan_has_result) {
        char scan_hex[(NFC_BLOCK_SIZE * 2) + 1];
        char short_tag[25];
        app_scene_update_format_scan_hex(&app->update.scan_result, scan_hex);
        app_ui_copy_truncated(short_tag, sizeof(short_tag), scan_hex, sizeof(short_tag) - 1);
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 52, short_tag);
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "OK Save tag  UPDOWN");
        return;
    }

    if(app->update.scan_error) {
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 52, "Scan failed");
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "OK Retry  UPDOWN");
        return;
    }

    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 52, "Press OK to scan tag");
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "UPDOWN Skip");
}

static void app_scene_update_draw_detail(Canvas* canvas, const SpoolmanSyncApp* app) {
    const SpoolmanSpool* spool =
        app_get_untagged_spool_by_index(app, app->update.current_untagged_index);

    if(!spool) {
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 32, "Nothing left to tag");
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 46, "All spools are ready");
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "BACK Menu");
        return;
    }

    char header[24];
    char header_line[25];
    char name_line[25];
    char weight_line[25];
    const char* material =
        furi_string_empty(spool->filament.material) ? "-" :
                                                     furi_string_get_cstr(spool->filament.material);
    const char* filament_name =
        furi_string_empty(spool->filament.name) ? "-" : furi_string_get_cstr(spool->filament.name);

    snprintf(
        header,
        sizeof(header),
        "%zu/%zu",
        app->update.current_untagged_index + 1,
        app->update.untagged_spools_count);
    snprintf(header_line, sizeof(header_line), "#%d %.15s", spool->id, material);
    app_ui_copy_truncated(name_line, sizeof(name_line), filament_name, 24);
    if(spool->has_remaining_weight) {
        snprintf(
            weight_line,
            sizeof(weight_line),
            "Remain: %ug",
            app_ui_weight_to_grams(spool->remaining_weight));
    } else {
        snprintf(weight_line, sizeof(weight_line), "Remain: N/A");
    }

    canvas_draw_str(canvas, 104, APP_UI_TITLE_Y, header);
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 22, header_line);
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 32, name_line);
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 42, weight_line);
    app_scene_update_draw_detail_status(canvas, app);
}

void app_scene_update_draw(Canvas* canvas, const SpoolmanSyncApp* app) {
    if(app->status == AppStatusLoadingUpdate) {
        app_ui_draw_title(canvas, "Tag existing spools");
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 32, "Loading spools...");
        return;
    }

    if(app->status == AppStatusUpdateError) {
        char line[25];
        app_ui_draw_title(canvas, "Spool list unavailable");
        if(app->server.status_code > 0) {
            snprintf(line, sizeof(line), "HTTP status: %d", app->server.status_code);
            canvas_draw_str(canvas, APP_UI_BODY_LEFT, 30, line);
        } else {
            canvas_draw_str(canvas, APP_UI_BODY_LEFT, 30, "No response from server");
        }
        app_ui_copy_truncated(
            line, sizeof(line), furi_string_get_cstr(app->server.error), sizeof(line) - 1);
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 44, line);
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "OK Retry");
        return;
    }

    app_ui_draw_title(canvas, "Tag existing spools");
    if(app->update.detail_visible) {
        app_scene_update_draw_detail(canvas, app);
    } else {
        app_scene_update_draw_summary(canvas, app);
    }
}

bool app_scene_update_handle_input(SpoolmanSyncApp* app, const InputEvent* event) {
    if(event->key == InputKeyOk) {
        if(app->status == AppStatusUpdateError) {
            app_start_update_mode(app);
        } else if(app->status == AppStatusUpdateMode && app->update.detail_visible) {
            if(app->update.scan_has_result) {
                app_update_confirm_current_spool(app);
            } else if(!app->update.patch_in_progress) {
                app_start_update_scan(app);
            }
        } else {
            app_start_update_mode(app);
        }
        return true;
    }

    if(app->status == AppStatusUpdateMode && event->key == InputKeyDown) {
        app_update_skip_current_spool(app, true);
        return true;
    }

    if(app->status == AppStatusUpdateMode && event->key == InputKeyUp) {
        app_update_skip_current_spool(app, false);
        return true;
    }

    return false;
}
