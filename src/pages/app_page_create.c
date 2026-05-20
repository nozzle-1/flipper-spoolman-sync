#include "app_page.h"

#include <stdio.h>

static void app_page_create_format_scan_hex(const NfcScanResult* result, char* output) {
    for(size_t i = 0; i < NFC_BLOCK_SIZE; i++) {
        snprintf(&output[i * 2], 3, "%02X", result->block9[i]);
    }
}

void app_page_create_draw(Canvas* canvas, const SpoolmanSyncApp* app) {
    canvas_draw_str(canvas, 10, 15, "Create mode");
    canvas_set_font(canvas, FontSecondary);

    if(app->status == AppStatusReading || app->status == AppStatusScanning) {
        canvas_draw_str(canvas, 5, 35, "Scanning tag...");
        canvas_draw_str(canvas, 5, 50, "Create mode");
        canvas_draw_str(canvas, 5, 63, "is not implemented");
        return;
    }

    if(app->status == AppStatusSuccess) {
        char scan_hex[(NFC_BLOCK_SIZE * 2) + 1];
        app_page_create_format_scan_hex(&app->scan_result, scan_hex);
        canvas_draw_str(canvas, 5, 35, scan_hex);
        canvas_draw_str(canvas, 5, 50, "Create mode");
        canvas_draw_str(canvas, 5, 63, "is not implemented");
        return;
    }

    if(app->status == AppStatusError) {
        canvas_draw_str(canvas, 5, 35, "Scan failed");
        canvas_draw_str(canvas, 5, 50, "Create mode");
        canvas_draw_str(canvas, 5, 63, "is not implemented");
        return;
    }

    canvas_draw_str(canvas, 5, 35, "Create mode");
    canvas_draw_str(canvas, 5, 50, "is not implemented");
    canvas_draw_str(canvas, 5, 63, "BACK: menu");
}

bool app_page_create_handle_input(SpoolmanSyncApp* app, const InputEvent* event) {
    (void)app;
    (void)event;
    return false;
}
