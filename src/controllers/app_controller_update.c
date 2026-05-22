#include "app_controller_i.h"

static void app_reset_update_mode_state(SpoolmanSyncApp* app) {
    furi_assert(app);

    app->update.untagged_spools_count = 0;
    app->update.total_spools_count = 0;
    app->update.current_untagged_index = 0;
    app->update.detail_visible = false;
    spoolman_spool_list_clear(&app->update.spools);
    app_reset_update_scan_state(app);
}

void app_start_update_scan(SpoolmanSyncApp* app) {
    app_stop_create_mode(app);
    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    app_reset_update_scan_state(app);
    app->update.scan_active = true;
    app_clear_http_status(app);
    furi_mutex_release(app->runtime.mutex);
    view_port_update(app->runtime.view_port);
    app_start_nfc_reader(app);
}

void app_cancel_update_scan(SpoolmanSyncApp* app) {
    app_stop_create_mode(app);
    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    app_reset_update_scan_state(app);
    app_clear_http_status(app);
    furi_mutex_release(app->runtime.mutex);
    view_port_update(app->runtime.view_port);
}

void app_update_skip_current_spool(SpoolmanSyncApp* app, bool forward) {
    app_stop_create_mode(app);
    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    app_reset_update_scan_state(app);
    app_clear_http_status(app);

    if(forward) {
        if(!app->update.detail_visible) {
            if(app->update.untagged_spools_count > 0) {
                app->update.detail_visible = true;
                app->update.current_untagged_index = 0;
            }
        } else if(app->update.current_untagged_index + 1 < app->update.untagged_spools_count) {
            app->update.current_untagged_index++;
        } else {
            app->update.detail_visible = false;
            app->update.current_untagged_index = 0;
        }
    } else if(app->update.untagged_spools_count > 0) {
        if(!app->update.detail_visible) {
            app->update.detail_visible = true;
            app->update.current_untagged_index = app->update.untagged_spools_count - 1;
        } else if(app->update.current_untagged_index > 0) {
            app->update.current_untagged_index--;
        } else {
            app->update.detail_visible = false;
            app->update.current_untagged_index = 0;
        }
    }
    furi_mutex_release(app->runtime.mutex);
    view_port_update(app->runtime.view_port);
}

bool app_update_confirm_current_spool(SpoolmanSyncApp* app) {
    if(!app || !app->services.spoolman_api) {
        return false;
    }

    int spool_id = 0;
    char tag_hex[(NFC_BLOCK_SIZE * 2) + 1];
    SpoolmanApiResult result = SpoolmanApiResultInvalidArguments;
    bool tag_already_defined = false;
    int existing_spool_id = 0;

    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    const SpoolmanSpool* spool =
        app_get_untagged_spool_by_index(app, app->update.current_untagged_index);
    if(!spool || !app->update.scan_has_result) {
        furi_mutex_release(app->runtime.mutex);
        return false;
    }

    spool_id = spool->id;
    app_tag_format_block9_hex(&app->update.scan_result, tag_hex, sizeof(tag_hex));
    const SpoolmanSpool* existing_spool =
        app_spool_find_by_tag(&app->update.spools, tag_hex, spool_id);
    if(existing_spool) {
        tag_already_defined = true;
        existing_spool_id = existing_spool->id;
    }
    app->update.patch_in_progress = true;
    app->update.patch_error = false;
    app_clear_http_status(app);
    furi_mutex_release(app->runtime.mutex);
    view_port_update(app->runtime.view_port);

    if(tag_already_defined) {
        char error_message[48];
        snprintf(error_message, sizeof(error_message), "Tag already defined on #%d", existing_spool_id);
        furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
        app->update.patch_in_progress = false;
        app->update.patch_error = true;
        app->server.status_code = 0;
        furi_string_set_str(app->server.error, error_message);
        furi_mutex_release(app->runtime.mutex);
        app_notify(app, &sequence_error);
        view_port_update(app->runtime.view_port);
        return false;
    }

    app_stop_create_mode(app);
    result = spoolman_api_update_spool_tag(app->services.spoolman_api, spool_id, tag_hex);

    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    app->update.patch_in_progress = false;
    if(result == SpoolmanApiResultOk) {
        size_t current_index = 0;
        for(size_t i = 0; i < app->update.spools.count; i++) {
            if(!app_spool_is_untagged(&app->update.spools.items[i])) {
                continue;
            }
            if(current_index == app->update.current_untagged_index) {
                furi_string_set_str(app->update.spools.items[i].tag, tag_hex);
                break;
            }
            current_index++;
        }

        if(app->update.untagged_spools_count > 0) {
            app->update.untagged_spools_count--;
        }
        if(app->update.untagged_spools_count == 0) {
            app->update.detail_visible = false;
            app->update.current_untagged_index = 0;
        } else if(app->update.current_untagged_index >= app->update.untagged_spools_count) {
            app->update.current_untagged_index = app->update.untagged_spools_count - 1;
        }
        app_reset_update_scan_state(app);
    } else {
        app->update.patch_error = true;
        app->server.status_code = spoolman_api_get_status_code(app->services.spoolman_api);
        furi_string_set(
            app->server.error, spoolman_api_get_last_error(app->services.spoolman_api));
    }
    furi_mutex_release(app->runtime.mutex);

    app_notify(app, result == SpoolmanApiResultOk ? &sequence_success : &sequence_error);
    view_port_update(app->runtime.view_port);
    return result == SpoolmanApiResultOk;
}

void app_start_update_mode(SpoolmanSyncApp* app) {
    app_stop_create_mode(app);
    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    app->status = AppStatusLoadingUpdate;
    app_reset_find_state(app);
    app_reset_update_mode_state(app);
    app->server.status_code = 0;
    furi_string_reset(app->server.error);
    furi_mutex_release(app->runtime.mutex);
    view_port_update(app->runtime.view_port);

    app_ensure_spoolman_api(app);

    SpoolmanApiResult result = SpoolmanApiResultHttpUnavailable;
    SpoolmanSpoolList spools;
    spoolman_spool_list_init(&spools);
    if(app->services.spoolman_api) {
        result = spoolman_api_get_spools(app->services.spoolman_api, &spools, false);
    }

    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    if(result == SpoolmanApiResultOk) {
        size_t untagged_spool_count = 0;
        for(size_t i = 0; i < spools.count; i++) {
            if(app_spool_is_untagged(&spools.items[i])) {
                untagged_spool_count++;
            }
        }
        app->update.spools = spools;
        app->update.untagged_spools_count = untagged_spool_count;
        app->update.total_spools_count = spools.count;
        app->status = AppStatusUpdateMode;
    } else {
        spoolman_spool_list_clear(&spools);
        app->status = AppStatusUpdateError;
        if(app->services.spoolman_api) {
            app->server.status_code = spoolman_api_get_status_code(app->services.spoolman_api);
            furi_string_set(
                app->server.error, spoolman_api_get_last_error(app->services.spoolman_api));
        } else {
            furi_string_set(app->server.error, "HTTP unavailable");
        }
    }
    furi_mutex_release(app->runtime.mutex);
    view_port_update(app->runtime.view_port);
}
