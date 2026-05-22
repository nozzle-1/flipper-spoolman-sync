#include "app_controller_i.h"

#define APP_CONTROLLER_TAG "SpoolmanSync"

static void app_set_lookup_error(
    SpoolmanSyncApp* app,
    AppStatus error_status,
    FuriString* error,
    SpoolmanApiResult result) {
    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    app->status = error_status;
    app->server.status_code = app->services.spoolman_api ?
                                  spoolman_api_get_status_code(app->services.spoolman_api) :
                                  0;
    if(result == SpoolmanApiResultInvalidArguments) {
        app->server.status_code = 0;
        furi_string_set_str(error, "Could not read spool data");
    } else if(
        app->services.spoolman_api &&
        strcmp(spoolman_api_get_last_error(app->services.spoolman_api), "Filament not found") == 0) {
        furi_string_set_str(error, "Filament missing in Spoolman");
    } else if(app->services.spoolman_api) {
        furi_string_set(error, spoolman_api_get_last_error(app->services.spoolman_api));
    } else {
        furi_string_set_str(error, "HTTP unavailable");
    }
    furi_mutex_release(app->runtime.mutex);
}

void app_process_create_scan(SpoolmanSyncApp* app) {
    if(!app) {
        return;
    }

    app_stop_create_mode(app);
    app_ensure_spoolman_api(app);

    char tag_hex[(NFC_BLOCK_SIZE * 2) + 1] = "";
    SpoolmanFilament filament;
    SpoolmanSpoolList spools;
    spoolman_filament_init(&filament);
    spoolman_spool_list_init(&spools);

    SpoolmanApiResult result = app_filament_resolve_from_scan(
        app->services.spoolman_api, &app->create.scan_result, tag_hex, sizeof(tag_hex), &filament);

    if(result != SpoolmanApiResultOk) {
        app_set_lookup_error(app, AppStatusCreateError, app->create.error, result);
        app_notify(app, &sequence_error);
        view_port_update(app->runtime.view_port);
        goto cleanup_create;
    }

    result = spoolman_api_get_spools_by_filament(
        app->services.spoolman_api, filament.id, &spools, true);
    if(result != SpoolmanApiResultOk) {
        furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
        app->status = AppStatusCreateError;
        app->server.status_code = app->services.spoolman_api ?
                                      spoolman_api_get_status_code(app->services.spoolman_api) :
                                      0;
        if(app->services.spoolman_api) {
            furi_string_set(
                app->create.error, spoolman_api_get_last_error(app->services.spoolman_api));
        } else {
            furi_string_set_str(app->create.error, "HTTP unavailable");
        }
        furi_mutex_release(app->runtime.mutex);
        app_notify(app, &sequence_error);
        view_port_update(app->runtime.view_port);
        goto cleanup_create;
    }

    spoolman_api_fill_missing_spool_details(app->services.spoolman_api, &spools);
    const SpoolmanSpool* existing_spool = app_spool_find_by_tag(&spools, tag_hex, 0);
    if(existing_spool) {
        furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
        app->status = AppStatusCreateError;
        app->server.status_code = 0;
        app_set_existing_create_spool_details(app, existing_spool, &filament);
        furi_string_printf(app->create.error, "Tag already defined on #%d", existing_spool->id);
        furi_mutex_release(app->runtime.mutex);
        app_notify(app, &sequence_error);
        view_port_update(app->runtime.view_port);
        goto cleanup_create;
    }

    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    app->status = AppStatusCreateConfirm;
    app->create.filament_id = filament.id;
    app->create.filament_weight = filament.weight;
    furi_string_set(app->create.filament_name, filament.name);
    furi_string_set(app->create.filament_material, filament.material);
    furi_mutex_release(app->runtime.mutex);
    view_port_update(app->runtime.view_port);

cleanup_create:
    spoolman_spool_list_clear(&spools);
    spoolman_filament_clear(&filament);
}

void app_process_find_scan(SpoolmanSyncApp* app) {
    if(!app) {
        return;
    }

    app_stop_create_mode(app);
    app_ensure_spoolman_api(app);

    char tag_hex[(NFC_BLOCK_SIZE * 2) + 1] = "";
    SpoolmanFilament filament;
    SpoolmanSpoolList spools;
    spoolman_filament_init(&filament);
    spoolman_spool_list_init(&spools);

    SpoolmanApiResult result = app_filament_resolve_from_scan(
        app->services.spoolman_api, &app->find.scan_result, tag_hex, sizeof(tag_hex), &filament);

    if(result != SpoolmanApiResultOk) {
        app_set_lookup_error(app, AppStatusFindError, app->find.error, result);
        app_notify(app, &sequence_error);
        view_port_update(app->runtime.view_port);
        goto cleanup_find;
    }

    result = spoolman_api_get_spools_by_filament(
        app->services.spoolman_api, filament.id, &spools, true);
    if(result != SpoolmanApiResultOk) {
        furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
        app->status = AppStatusFindError;
        app->server.status_code = app->services.spoolman_api ?
                                      spoolman_api_get_status_code(app->services.spoolman_api) :
                                      0;
        if(app->services.spoolman_api) {
            furi_string_set(app->find.error, spoolman_api_get_last_error(app->services.spoolman_api));
        } else {
            furi_string_set_str(app->find.error, "HTTP unavailable");
        }
        furi_mutex_release(app->runtime.mutex);
        app_notify(app, &sequence_error);
        view_port_update(app->runtime.view_port);
        goto cleanup_find;
    }

    spoolman_api_fill_missing_spool_details(app->services.spoolman_api, &spools);
    const SpoolmanSpool* found_spool = app_spool_find_by_tag(&spools, tag_hex, 0);
    if(!found_spool) {
        furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
        app->status = AppStatusFindError;
        app->server.status_code = 0;
        app->find.filament_id = filament.id;
        app->find.spool_weight = 0;
        app->find.spool_has_remaining_weight = false;
        furi_string_set(app->find.filament_name, filament.name);
        furi_string_set(app->find.filament_material, filament.material);
        furi_string_set_str(app->find.error, "No spool matches this tag");
        furi_mutex_release(app->runtime.mutex);
        app_notify(app, &sequence_error);
        view_port_update(app->runtime.view_port);
        goto cleanup_find;
    }

    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    app->status = AppStatusFindSuccess;
    app->server.status_code = 0;
    app->find.filament_id = filament.id;
    app->find.spool_id = found_spool->id;
    app->find.spool_has_remaining_weight =
        app_spool_get_remaining_weight(found_spool, &app->find.spool_weight);
    furi_string_reset(app->find.error);
    furi_string_set(
        app->find.filament_name,
        furi_string_empty(found_spool->filament.name) ? filament.name : found_spool->filament.name);
    furi_string_set(
        app->find.filament_material,
        furi_string_empty(found_spool->filament.material) ? filament.material :
                                                            found_spool->filament.material);
    furi_mutex_release(app->runtime.mutex);
    app_notify(app, &sequence_success);
    view_port_update(app->runtime.view_port);

cleanup_find:
    spoolman_spool_list_clear(&spools);
    spoolman_filament_clear(&filament);
}

void app_confirm_create_spool(SpoolmanSyncApp* app) {
    if(!app || !app->services.spoolman_api) {
        return;
    }

    int filament_id = 0;
    int spool_id = 0;
    int rolled_back_spool_id = 0;
    char tag_hex[(NFC_BLOCK_SIZE * 2) + 1] = "";
    char duplicate_tag_error[32] = "";
    SpoolmanSpoolList spools;

    spoolman_spool_list_init(&spools);

    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    if(app->status != AppStatusCreateConfirm || app->create.filament_id <= 0) {
        spoolman_spool_list_clear(&spools);
        furi_mutex_release(app->runtime.mutex);
        return;
    }

    filament_id = app->create.filament_id;
    app_tag_format_block9_hex(&app->create.scan_result, tag_hex, sizeof(tag_hex));
    app->status = AppStatusCreateSaving;
    app->create.spool_id = 0;
    app->create.existing_spool_id = 0;
    app->create.existing_spool_weight = 0;
    app->create.existing_spool_has_remaining_weight = false;
    furi_string_reset(app->create.error);
    furi_string_reset(app->create.existing_spool_name);
    furi_string_reset(app->create.existing_spool_material);
    furi_mutex_release(app->runtime.mutex);
    view_port_update(app->runtime.view_port);

    SpoolmanApiResult result =
        spoolman_api_get_spools_by_filament(app->services.spoolman_api, filament_id, &spools, true);
    if(result == SpoolmanApiResultOk) {
        spoolman_api_fill_missing_spool_details(app->services.spoolman_api, &spools);
    }

    const SpoolmanSpool* existing_spool =
        result == SpoolmanApiResultOk ? app_spool_find_by_tag(&spools, tag_hex, 0) : NULL;

    if(existing_spool) {
        furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
        app_set_existing_create_spool_details(app, existing_spool, NULL);
        furi_mutex_release(app->runtime.mutex);
        snprintf(
            duplicate_tag_error,
            sizeof(duplicate_tag_error),
            "Tag already defined on #%d",
            existing_spool->id);
        result = SpoolmanApiResultRequestFailed;
    } else if(result == SpoolmanApiResultOk) {
        result = spoolman_api_create_spool(app->services.spoolman_api, filament_id, &spool_id);
        if(result == SpoolmanApiResultOk) {
            result = spoolman_api_update_spool_tag(app->services.spoolman_api, spool_id, tag_hex);
            if(result != SpoolmanApiResultOk) {
                if(spoolman_api_delete_spool(app->services.spoolman_api, spool_id) ==
                   SpoolmanApiResultOk) {
                    rolled_back_spool_id = spool_id;
                    spool_id = 0;
                }
            }
        }
    }

    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    app->server.status_code =
        duplicate_tag_error[0] != '\0' ?
            0 :
            (app->services.spoolman_api ?
                 spoolman_api_get_status_code(app->services.spoolman_api) :
                 0);
    if(result == SpoolmanApiResultOk) {
        app->status = AppStatusCreateSuccess;
        app->create.spool_id = spool_id;
        furi_string_reset(app->create.error);
    } else {
        app->status = AppStatusCreateError;
        app->create.spool_id = spool_id;
        if(duplicate_tag_error[0] != '\0') {
            furi_string_set_str(app->create.error, duplicate_tag_error);
        } else if(app->services.spoolman_api) {
            furi_string_set(
                app->create.error, spoolman_api_get_last_error(app->services.spoolman_api));
        } else {
            furi_string_set_str(app->create.error, "HTTP unavailable");
        }
    }
    furi_mutex_release(app->runtime.mutex);

    if(rolled_back_spool_id > 0) {
        FURI_LOG_I(
            APP_CONTROLLER_TAG,
            "Rolled back spool #%d after create/tag failure",
            rolled_back_spool_id);
    }

    app_notify(app, result == SpoolmanApiResultOk ? &sequence_success : &sequence_error);
    view_port_update(app->runtime.view_port);
    spoolman_spool_list_clear(&spools);
}
