#include "app_scene_i.h"

#include "app_scene_ui.h"

#define READ_TAG_PAGE_COUNT 4

static void app_scene_scan_format_hex(
    const uint8_t* bytes,
    size_t bytes_len,
    char* output,
    size_t output_size) {
    for(size_t i = 0; i < bytes_len && (i * 2) + 1 < output_size; i++) {
        snprintf(&output[i * 2], output_size - (i * 2), "%02X", bytes[i]);
    }
}

static uint16_t app_scene_scan_read_le_u16(const uint8_t* bytes) {
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

static float app_scene_scan_read_le_f32(const uint8_t* bytes) {
    union {
        uint32_t u32;
        float f32;
    } value;

    value.u32 = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
                ((uint32_t)bytes[3] << 24);
    return value.f32;
}

static void app_scene_scan_copy_ascii(
    char* output,
    size_t output_size,
    const uint8_t* bytes,
    size_t bytes_len) {
    size_t copy_len = bytes_len < (output_size - 1) ? bytes_len : (output_size - 1);
    for(size_t i = 0; i < copy_len; i++) {
        char c = (char)bytes[i];
        output[i] = (c >= 32 && c <= 126) ? c : '\0';
        if(output[i] == '\0') return;
    }
    output[copy_len] = '\0';
}

static void app_scene_scan_draw_header(Canvas* canvas, const SpoolmanSyncApp* app) {
    char page[16];
    snprintf(page, sizeof(page), "%u/%u", (unsigned)(app->scan.read_page + 1), READ_TAG_PAGE_COUNT);
    app_ui_draw_title(canvas, "Read spool tag");
    canvas_draw_str(canvas, 103, APP_UI_TITLE_Y, page);
}

static void app_scene_scan_draw_page_0(Canvas* canvas, const NfcScanResult* result) {
    char tray_uid[(NFC_BLOCK_SIZE * 2) + 1] = "";
    char detailed_type[17] = "";
    char color_hex[9] = "";
    char material_id[9] = "";
    char line[48];

    app_scene_scan_format_hex(result->blocks[9], NFC_BLOCK_SIZE, tray_uid, sizeof(tray_uid));
    app_scene_scan_copy_ascii(detailed_type, sizeof(detailed_type), result->blocks[4], 16);
    app_scene_scan_format_hex(result->blocks[5], 4, color_hex, sizeof(color_hex));
    app_scene_scan_copy_ascii(material_id, sizeof(material_id), &result->blocks[1][8], 8);

    snprintf(line, sizeof(line), "Type:%s", detailed_type[0] ? detailed_type : "<unknown>");
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 26, line);
    snprintf(
        line,
        sizeof(line),
        "Color:%s Mat:%s",
        color_hex,
        material_id[0] ? material_id : "<unknown>");
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 38, line);
    snprintf(line, sizeof(line), "Tray UID:%s", tray_uid);
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 50, line);
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "OK Rescan  UPDOWN");
}

static void app_scene_scan_draw_page_1(Canvas* canvas, const NfcScanResult* result) {
    char line[32];
    char second_color_hex[9] = "";
    char filament_type[17] = "";

    app_scene_scan_format_hex(&result->blocks[16][4], 4, second_color_hex, sizeof(second_color_hex));
    app_scene_scan_copy_ascii(filament_type, sizeof(filament_type), result->blocks[2], 16);

    snprintf(line, sizeof(line), "Filament:%s", filament_type[0] ? filament_type : "<unknown>");
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 26, line);
    snprintf(
        line,
        sizeof(line),
        "Spool:%ug",
        (unsigned)app_scene_scan_read_le_u16(&result->blocks[5][4]));
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 38, line);
    snprintf(
        line,
        sizeof(line),
        "Diameter:%.2fmm",
        (double)app_scene_scan_read_le_f32(&result->blocks[5][8]));
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 50, line);
    snprintf(line, sizeof(line), "Color 2:%s", second_color_hex);
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, line);
}

static void app_scene_scan_draw_page_2(Canvas* canvas, const NfcScanResult* result) {
    char line[32];

    snprintf(
        line,
        sizeof(line),
        "Bed:%uC",
        (unsigned)app_scene_scan_read_le_u16(&result->blocks[6][6]));
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 26, line);
    snprintf(
        line,
        sizeof(line),
        "Dry:%uC %uh",
        (unsigned)app_scene_scan_read_le_u16(&result->blocks[6][0]),
        (unsigned)app_scene_scan_read_le_u16(&result->blocks[6][2]));
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 38, line);
    snprintf(
        line,
        sizeof(line),
        "Hotend:%u-%uC",
        (unsigned)app_scene_scan_read_le_u16(&result->blocks[6][10]),
        (unsigned)app_scene_scan_read_le_u16(&result->blocks[6][8]));
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 50, line);
    snprintf(
        line,
        sizeof(line),
        "Nozzle:%.2f",
        (double)app_scene_scan_read_le_f32(&result->blocks[8][12]));
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, line);
}

static void app_scene_scan_draw_page_3(Canvas* canvas, const NfcScanResult* result) {
    char short_date[17] = "";
    char line[32];

    app_scene_scan_copy_ascii(short_date, sizeof(short_date), result->blocks[13], 16);

    snprintf(line, sizeof(line), "Date:%s", short_date[0] ? short_date : "<unknown>");
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 26, line);
    snprintf(
        line,
        sizeof(line),
        "Length:%um",
        (unsigned)app_scene_scan_read_le_u16(&result->blocks[14][4]));
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 38, line);
    snprintf(
        line,
        sizeof(line),
        "Width:%u",
        (unsigned)app_scene_scan_read_le_u16(&result->blocks[10][4]));
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 50, line);
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "OK Rescan  UPDOWN");
}

void app_scene_scan_draw(Canvas* canvas, const SpoolmanSyncApp* app) {
    app_ui_draw_title(canvas, "Read spool tag");

    if(app->status == AppStatusScanReading) {
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 32, "Hold spool near reader");
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 46, "Reading tag data...");
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "BACK Cancel");
        return;
    }

    if(app->status == AppStatusScanSuccess) {
        app_scene_scan_draw_header(canvas, app);
        switch(app->scan.read_page) {
        case 0:
            app_scene_scan_draw_page_0(canvas, &app->scan.scan_result);
            break;
        case 1:
            app_scene_scan_draw_page_1(canvas, &app->scan.scan_result);
            break;
        case 2:
            app_scene_scan_draw_page_2(canvas, &app->scan.scan_result);
            break;
        default:
            app_scene_scan_draw_page_3(canvas, &app->scan.scan_result);
            break;
        }
        return;
    }

    if(app->status == AppStatusScanError) {
        char line[25];
        app_ui_copy_truncated(
            line,
            sizeof(line),
            furi_string_empty(app->scan.error) ? "Scan failed" :
                                                 furi_string_get_cstr(app->scan.error),
            sizeof(line) - 1);
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 32, line);
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, 46, "Check spool position");
        canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "OK Retry");
        return;
    }

    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 32, "Read and decode");
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, 46, "a Bambu spool tag");
    canvas_draw_str(canvas, APP_UI_BODY_LEFT, APP_UI_FOOTER_Y, "OK Start scan");
}

bool app_scene_scan_handle_input(SpoolmanSyncApp* app, const InputEvent* event) {
    if(event->key == InputKeyOk &&
       (app->status == AppStatusScanReady || app->status == AppStatusScanSuccess ||
        app->status == AppStatusScanError)) {
        app_start_scan_read(app);
        return true;
    }

    if(app->status == AppStatusScanSuccess &&
       (event->key == InputKeyUp || event->key == InputKeyDown)) {
        furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
        if(event->key == InputKeyDown) {
            app->scan.read_page = (app->scan.read_page + 1) % READ_TAG_PAGE_COUNT;
        } else {
            app->scan.read_page =
                app->scan.read_page == 0 ? READ_TAG_PAGE_COUNT - 1 : app->scan.read_page - 1;
        }
        furi_mutex_release(app->runtime.mutex);
        view_port_update(app->runtime.view_port);
        return true;
    }

    return false;
}
