#include "app_controller_i.h"

static void app_nfc_service_callback(
    SpoolmanNfcServiceEvent event,
    const NfcScanResult* result,
    void* context) {
    SpoolmanSyncApp* app = context;
    bool play_tag_found_sound = false;
    bool queue_create_process = false;
    bool queue_find_process = false;

    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    if(app->status == AppStatusUpdateMode) {
        if(event == SpoolmanNfcServiceEventReading) {
            app->update.scan_active = true;
            app->update.scan_error = false;
            app->update.patch_error = false;
        } else if(event == SpoolmanNfcServiceEventSuccess) {
            app->update.scan_active = false;
            app->update.scan_has_result = true;
            app->update.scan_error = false;
            app->update.scan_result = *result;
            play_tag_found_sound = true;
        } else if(event == SpoolmanNfcServiceEventError) {
            app->update.scan_active = false;
            app->update.scan_error = true;
        }
    } else if(app->status == AppStatusFindReading) {
        if(event == SpoolmanNfcServiceEventSuccess) {
            app->status = AppStatusFindSearching;
            app->find.scan_result = *result;
            app->find.filament_id = 0;
            app->find.spool_id = 0;
            furi_string_reset(app->find.error);
            furi_string_reset(app->find.filament_name);
            furi_string_reset(app->find.filament_material);
            play_tag_found_sound = true;
            queue_find_process = true;
        } else if(event == SpoolmanNfcServiceEventError) {
            app->status = AppStatusFindError;
            furi_string_set_str(app->find.error, "Scan failed");
        }
    } else if(app->status == AppStatusCreateReading) {
        if(event == SpoolmanNfcServiceEventSuccess) {
            app->status = AppStatusCreateSearching;
            app->create.scan_result = *result;
            app->create.filament_id = 0;
            app->create.spool_id = 0;
            furi_string_reset(app->create.error);
            furi_string_reset(app->create.filament_name);
            furi_string_reset(app->create.filament_material);
            play_tag_found_sound = true;
            queue_create_process = true;
        } else if(event == SpoolmanNfcServiceEventError) {
            app->status = AppStatusCreateError;
            furi_string_set_str(app->create.error, "Scan failed");
        }
    } else if(app->status == AppStatusScanReading) {
        if(event == SpoolmanNfcServiceEventSuccess) {
            app->status = AppStatusScanSuccess;
            app->scan.scan_result = *result;
            app->scan.read_page = 0;
            furi_string_reset(app->scan.error);
            play_tag_found_sound = true;
        } else if(event == SpoolmanNfcServiceEventError) {
            app->status = AppStatusScanError;
            furi_string_set_str(app->scan.error, "Scan failed");
        }
    }
    furi_mutex_release(app->runtime.mutex);

    if(play_tag_found_sound) {
        app_notify(app, &app_sequence_tag_found);
    }
    if(queue_create_process) {
        app_queue_create_process(app);
    }
    if(queue_find_process) {
        app_queue_find_process(app);
    }

    view_port_update(app->runtime.view_port);
}

void app_start_nfc_reader(SpoolmanSyncApp* app) {
    app->services.nfc_reader = nfc_reader_alloc(app_nfc_service_callback, app);
    nfc_reader_start(app->services.nfc_reader);
}

void app_start_scan_mode(SpoolmanSyncApp* app) {
    app_stop_create_mode(app);
    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    app_reset_create_state(app);
    app_reset_find_state(app);
    app_clear_http_status(app);
    app->status = AppStatusScanReady;
    furi_mutex_release(app->runtime.mutex);
    view_port_update(app->runtime.view_port);
}

void app_start_scan_read(SpoolmanSyncApp* app) {
    app_stop_create_mode(app);
    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    app_reset_create_state(app);
    app_reset_find_state(app);
    app_clear_http_status(app);
    app->status = AppStatusScanReading;
    furi_mutex_release(app->runtime.mutex);
    view_port_update(app->runtime.view_port);
    app_start_nfc_reader(app);
}

void app_start_find_mode(SpoolmanSyncApp* app) {
    app_stop_create_mode(app);
    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    app_reset_find_state(app);
    app_clear_http_status(app);
    app->status = AppStatusFindReady;
    furi_mutex_release(app->runtime.mutex);
    view_port_update(app->runtime.view_port);
}

void app_start_find_scan(SpoolmanSyncApp* app) {
    app_stop_create_mode(app);
    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    app_reset_find_state(app);
    app_clear_http_status(app);
    app->status = AppStatusFindReading;
    furi_mutex_release(app->runtime.mutex);
    view_port_update(app->runtime.view_port);
    app_start_nfc_reader(app);
}

void app_start_create_mode(SpoolmanSyncApp* app) {
    app_stop_create_mode(app);
    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    app_reset_create_state(app);
    app_reset_find_state(app);
    app_clear_http_status(app);
    app->status = AppStatusCreateReady;
    furi_mutex_release(app->runtime.mutex);
    view_port_update(app->runtime.view_port);
}

void app_start_create_scan(SpoolmanSyncApp* app) {
    app_stop_create_mode(app);
    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    app_reset_create_state(app);
    app_reset_find_state(app);
    app_clear_http_status(app);
    app->status = AppStatusCreateReading;
    furi_mutex_release(app->runtime.mutex);
    view_port_update(app->runtime.view_port);
    app_start_nfc_reader(app);
}
