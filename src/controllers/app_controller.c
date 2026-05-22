#include "app_controller_i.h"

const NotificationSequence app_sequence_tag_found = {
    &message_note_c6,
    &message_delay_25,
    &message_sound_off,
    NULL,
};

void app_notify(SpoolmanSyncApp* app, const NotificationSequence* sequence) {
    if(app && app->runtime.notifications && sequence) {
        notification_message(app->runtime.notifications, sequence);
    }
}

bool app_has_spoolman_base_url(const SpoolmanSyncApp* app) {
    return app && app->server.base_url && !furi_string_empty(app->server.base_url);
}

bool app_ensure_spoolman_api(SpoolmanSyncApp* app) {
    if(!app) {
        return false;
    }

    if(!app->services.spoolman_api && app->server.base_url) {
        app->services.spoolman_api =
            spoolman_api_alloc(furi_string_get_cstr(app->server.base_url));
    }

    return app->services.spoolman_api != NULL;
}

void app_reset_spoolman_api(SpoolmanSyncApp* app, const char* base_url) {
    if(!app) {
        return;
    }

    if(app->services.spoolman_api) {
        spoolman_api_free(app->services.spoolman_api);
        app->services.spoolman_api = NULL;
    }

    if(base_url && base_url[0] != '\0') {
        app->services.spoolman_api = spoolman_api_alloc(base_url);
    }
}

void app_clear_http_status(SpoolmanSyncApp* app) {
    if(!app) {
        return;
    }

    app->server.status_code = 0;
    furi_string_reset(app->server.error);
}

void app_close_base_url_editor(SpoolmanSyncApp* app) {
    if(!app || !app->services.base_url_view_holder) {
        return;
    }

    view_holder_set_view(app->services.base_url_view_holder, NULL);
    view_port_enabled_set(app->runtime.view_port, true);
    view_port_update(app->runtime.view_port);
}

void app_reset_create_state(SpoolmanSyncApp* app) {
    furi_assert(app);

    memset(&app->create.scan_result, 0, sizeof(app->create.scan_result));
    app->create.filament_id = 0;
    app->create.spool_id = 0;
    app->create.existing_spool_id = 0;
    app->create.filament_weight = 0;
    app->create.existing_spool_weight = 0;
    app->create.existing_spool_has_remaining_weight = false;
    furi_string_reset(app->create.error);
    furi_string_reset(app->create.filament_name);
    furi_string_reset(app->create.filament_material);
    furi_string_reset(app->create.existing_spool_name);
    furi_string_reset(app->create.existing_spool_material);
}

void app_reset_find_state(SpoolmanSyncApp* app) {
    furi_assert(app);

    memset(&app->find.scan_result, 0, sizeof(app->find.scan_result));
    app->find.filament_id = 0;
    app->find.spool_id = 0;
    app->find.spool_weight = 0;
    app->find.spool_has_remaining_weight = false;
    furi_string_reset(app->find.error);
    furi_string_reset(app->find.filament_name);
    furi_string_reset(app->find.filament_material);
}

void app_reset_update_scan_state(SpoolmanSyncApp* app) {
    furi_assert(app);

    memset(&app->update.scan_result, 0, sizeof(app->update.scan_result));
    app->update.scan_active = false;
    app->update.scan_has_result = false;
    app->update.scan_error = false;
    app->update.patch_in_progress = false;
    app->update.patch_error = false;
}

void app_set_existing_create_spool_details(
    SpoolmanSyncApp* app,
    const SpoolmanSpool* spool,
    const SpoolmanFilament* fallback_filament) {
    if(!app) {
        return;
    }

    app->create.existing_spool_id = spool ? spool->id : 0;
    app->create.existing_spool_weight = 0;
    app->create.existing_spool_has_remaining_weight = false;
    furi_string_reset(app->create.existing_spool_name);
    furi_string_reset(app->create.existing_spool_material);

    if(!spool) {
        return;
    }

    const char* spool_name = app_spool_get_display_name(spool, fallback_filament);
    const char* spool_material = app_spool_get_display_material(spool, fallback_filament);

    if(spool_name) {
        furi_string_set_str(app->create.existing_spool_name, spool_name);
    }
    if(spool_material) {
        furi_string_set_str(app->create.existing_spool_material, spool_material);
    }

    app->create.existing_spool_has_remaining_weight =
        app_spool_get_remaining_weight(spool, &app->create.existing_spool_weight);
}

const SpoolmanSpool*
    app_get_untagged_spool_by_index(const SpoolmanSyncApp* app, size_t untagged_index) {
    if(!app) {
        return NULL;
    }

    size_t current_index = 0;
    for(size_t i = 0; i < app->update.spools.count; i++) {
        if(!app_spool_is_untagged(&app->update.spools.items[i])) {
            continue;
        }
        if(current_index == untagged_index) {
            return &app->update.spools.items[i];
        }
        current_index++;
    }

    return NULL;
}

void app_queue_create_process(SpoolmanSyncApp* app) {
    AppEvent event = {.type = AppEventTypeCreateProcess};
    furi_message_queue_put(app->runtime.input_queue, &event, 0);
}

void app_queue_find_process(SpoolmanSyncApp* app) {
    AppEvent event = {.type = AppEventTypeFindProcess};
    furi_message_queue_put(app->runtime.input_queue, &event, 0);
}

void app_stop_nfc_reader(SpoolmanSyncApp* app) {
    if(app && app->services.nfc_reader) {
        nfc_reader_stop(app->services.nfc_reader);
        app->services.nfc_reader = NULL;
    }
}

void app_stop_create_mode(SpoolmanSyncApp* app) {
    app_stop_nfc_reader(app);
}

void app_return_to_menu(SpoolmanSyncApp* app) {
    app_stop_create_mode(app);
    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    app->status = AppStatusModeSelect;
    app_reset_find_state(app);
    app_reset_update_scan_state(app);
    furi_mutex_release(app->runtime.mutex);
    view_port_update(app->runtime.view_port);
}
