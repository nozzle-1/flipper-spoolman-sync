#include "app_page.h"

#include <stdio.h>

static void app_page_update_format_scan_hex(const NfcScanResult* result, char* output) {
    for(size_t i = 0; i < NFC_BLOCK_SIZE; i++) {
        snprintf(&output[i * 2], 3, "%02X", result->blocks[9][i]);
    }
}

static void app_page_update_draw_summary(Canvas* canvas, const SpoolmanSyncApp* app) {
    char untagged_count[24];
    snprintf(
        untagged_count,
        sizeof(untagged_count),
        "Untagged spools: %zu/%zu",
        app->spools_to_update_count,
        app->total_spools_count);
    canvas_draw_str(canvas, 5, 28, untagged_count);
    canvas_draw_str(canvas, 5, 52, "UP/DOWN: browse");
    canvas_draw_str(canvas, 5, 63, "OK: refresh");
}

static void app_page_update_draw_detail_status(Canvas* canvas, const SpoolmanSyncApp* app) {
    if(app->update_patch_in_progress) {
        canvas_draw_str(canvas, 5, 50, "Updating Spoolman...");
        canvas_draw_str(canvas, 5, 63, "Wait...");
        return;
    }

    if(app->update_scan_active) {
        canvas_draw_str(canvas, 5, 50, "Scanning tag...");
        canvas_draw_str(canvas, 5, 63, "UP/DOWN: skip");
        return;
    }

    if(app->update_patch_error) {
        if(app->spoolman_status_code > 0) {
            char status[24];
            snprintf(status, sizeof(status), "HTTP %d", app->spoolman_status_code);
            canvas_draw_str(canvas, 5, 50, status);
        } else if(!furi_string_empty(app->spoolman_error)) {
            canvas_draw_str(canvas, 5, 50, furi_string_get_cstr(app->spoolman_error));
        } else {
            canvas_draw_str(canvas, 5, 50, "Patch failed");
        }
        canvas_draw_str(canvas, 5, 63, "OK retry / UPDOWN skip");
        return;
    }

    if(app->update_scan_has_result) {
        char scan_hex[(NFC_BLOCK_SIZE * 2) + 1];
        app_page_update_format_scan_hex(&app->update_scan_result, scan_hex);
        canvas_draw_str(canvas, 5, 50, scan_hex);
        canvas_draw_str(canvas, 5, 63, "OK save / UPDOWN skip");
        return;
    }

    if(app->update_scan_error) {
        canvas_draw_str(canvas, 5, 50, "Scan failed");
        canvas_draw_str(canvas, 5, 63, "OK rescan / UPDOWN skip");
        return;
    }

    canvas_draw_str(canvas, 5, 50, "Press OK to scan");
    canvas_draw_str(canvas, 5, 63, "UP/DOWN: skip");
}

static void app_page_update_draw_detail(Canvas* canvas, const SpoolmanSyncApp* app) {
    const SpoolmanSpool* spool =
        app_get_untagged_spool_by_index(app, app->current_untagged_spool_index);

    if(!spool) {
        canvas_draw_str(canvas, 5, 35, "No untagged spool");
        canvas_draw_str(canvas, 5, 50, "DOWN: recap");
        canvas_draw_str(canvas, 5, 63, "BACK: menu");
        return;
    }

    char header[24];
    char spool_label[64];

    const char* material = furi_string_empty(spool->filament.material) ?
                               "-" :
                               furi_string_get_cstr(spool->filament.material);
    const char* filament_name =
        furi_string_empty(spool->filament.name) ? "-" : furi_string_get_cstr(spool->filament.name);

    snprintf(
        header,
        sizeof(header),
        "%zu/%zu",
        app->current_untagged_spool_index + 1,
        app->spools_to_update_count);
    snprintf(spool_label, sizeof(spool_label), "#%d - %.12s", spool->id, material);

    canvas_draw_str(canvas, 96, 15, header);
    canvas_draw_str(canvas, 5, 27, spool_label);
    canvas_draw_str(canvas, 5, 38, filament_name);
    app_page_update_draw_detail_status(canvas, app);
}

void app_page_update_draw(Canvas* canvas, const SpoolmanSyncApp* app) {
    if(app->status == AppStatusLoadingUpdate) {
        canvas_draw_str(canvas, 10, 15, "Update mode");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 5, 35, "Loading spools...");
        return;
    }

    if(app->status == AppStatusUpdateError) {
        canvas_draw_str(canvas, 10, 15, "Update error");
        canvas_set_font(canvas, FontSecondary);
        if(app->spoolman_status_code > 0) {
            char status[24];
            snprintf(status, sizeof(status), "HTTP status: %d", app->spoolman_status_code);
            canvas_draw_str(canvas, 5, 31, status);
        } else {
            canvas_draw_str(canvas, 5, 31, "No HTTP response");
        }
        canvas_draw_str(canvas, 5, 45, furi_string_get_cstr(app->spoolman_error));
        canvas_draw_str(canvas, 5, 60, "OK retry / BACK menu");
        return;
    }

    canvas_draw_str(canvas, 10, 15, "Update mode");
    canvas_set_font(canvas, FontSecondary);
    if(app->update_detail_visible) {
        app_page_update_draw_detail(canvas, app);
    } else {
        app_page_update_draw_summary(canvas, app);
    }
}

bool app_page_update_handle_input(SpoolmanSyncApp* app, const InputEvent* event) {
    if(event->key == InputKeyOk) {
        if(app->status == AppStatusUpdateError) {
            app_start_update_mode(app);
        } else if(app->status == AppStatusUpdateMode && app->update_detail_visible) {
            if(app->update_scan_has_result) {
                app_update_confirm_current_spool(app);
            } else if(!app->update_patch_in_progress) {
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
