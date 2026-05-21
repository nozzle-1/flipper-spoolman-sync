#include "app.h"
#include "app_pages.h"
#include "uart_text_input.h"

#include <furi.h>
#include <furi_hal.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#define TAG "SpoolmanSync"

#define SPOOLMAN_URL_CONFIG_PATH APP_DATA_PATH("spoolman_url.txt")

static const NotificationSequence sequence_tag_found = {
    &message_note_c6,
    &message_delay_25,
    &message_sound_off,
    NULL,
};

static void app_notify(SpoolmanSyncApp* app, const NotificationSequence* sequence) {
    if(app && app->notifications && sequence) {
        notification_message(app->notifications, sequence);
    }
}

static bool app_is_ascii_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static void app_trim_ascii_whitespace(char* value) {
    if(!value) {
        return;
    }

    size_t start = 0;
    size_t len = strlen(value);
    while(start < len && app_is_ascii_whitespace(value[start])) {
        start++;
    }

    size_t end = len;
    while(end > start && app_is_ascii_whitespace(value[end - 1])) {
        end--;
    }

    size_t trimmed_len = end - start;
    if(start > 0 && trimmed_len > 0) {
        memmove(value, value + start, trimmed_len);
    }
    value[trimmed_len] = '\0';
}

static bool app_write_spoolman_url_config(const char* base_url) {
    if(!base_url || base_url[0] == '\0') {
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool saved = false;

    if(storage_file_open(file, SPOOLMAN_URL_CONFIG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        size_t expected_size = strlen(base_url);
        size_t written_size = storage_file_write(file, base_url, expected_size);
        if(written_size != expected_size) {
            FURI_LOG_W(TAG, "Failed to write Spoolman URL config");
        } else {
            saved = true;
        }
        storage_file_close(file);
    } else {
        FURI_LOG_W(TAG, "Failed to create config file %s", SPOOLMAN_URL_CONFIG_PATH);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return saved;
}

static FuriString* app_load_spoolman_base_url(void) {
    FuriString* base_url = furi_string_alloc();
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(!storage_file_open(file, SPOOLMAN_URL_CONFIG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_I(TAG, "Config not found at %s", SPOOLMAN_URL_CONFIG_PATH);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return base_url;
    }

    size_t file_size = storage_file_size(file);
    if(file_size > SPOOLMAN_URL_MAX_LEN) {
        file_size = SPOOLMAN_URL_MAX_LEN;
    }

    char url_buffer[SPOOLMAN_URL_MAX_LEN + 1];
    memset(url_buffer, 0, sizeof(url_buffer));
    size_t read_size = storage_file_read(file, url_buffer, file_size);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    url_buffer[read_size] = '\0';
    app_trim_ascii_whitespace(url_buffer);

    if(read_size == 0 || url_buffer[0] == '\0') {
        FURI_LOG_W(TAG, "Config file is empty");
        return base_url;
    }

    furi_string_set_str(base_url, url_buffer);
    FURI_LOG_I(TAG, "Loaded Spoolman URL: %s", furi_string_get_cstr(base_url));
    return base_url;
}

static bool app_has_spoolman_base_url(const SpoolmanSyncApp* app) {
    return app && app->spoolman_base_url && !furi_string_empty(app->spoolman_base_url);
}

static bool app_ensure_spoolman_api(SpoolmanSyncApp* app) {
    if(!app) {
        return false;
    }

    if(!app->spoolman_api && app->spoolman_base_url) {
        app->spoolman_api = spoolman_api_alloc(furi_string_get_cstr(app->spoolman_base_url));
    }

    return app->spoolman_api != NULL;
}

static void app_reset_spoolman_api(SpoolmanSyncApp* app, const char* base_url) {
    if(!app) {
        return;
    }

    if(app->spoolman_api) {
        spoolman_api_free(app->spoolman_api);
        app->spoolman_api = NULL;
    }

    if(base_url && base_url[0] != '\0') {
        app->spoolman_api = spoolman_api_alloc(base_url);
    }
}

static void app_clear_http_status(SpoolmanSyncApp* app) {
    if(!app) {
        return;
    }

    app->spoolman_status_code = 0;
    furi_string_reset(app->spoolman_error);
}

static void app_close_base_url_editor(SpoolmanSyncApp* app) {
    if(!app || !app->base_url_view_holder) {
        return;
    }

    view_holder_set_view(app->base_url_view_holder, NULL);
    view_port_enabled_set(app->view_port, true);
    view_port_update(app->view_port);
}

static void app_free_base_url_test_thread(SpoolmanSyncApp* app) {
    if(!app || !app->base_url_test_thread) {
        return;
    }

    furi_thread_join(app->base_url_test_thread);
    furi_thread_free(app->base_url_test_thread);
    app->base_url_test_thread = NULL;
}

static int32_t app_base_url_test_worker(void* context) {
    SpoolmanSyncApp* app = context;
    if(!app) {
        return -1;
    }

    char candidate[SPOOLMAN_URL_MAX_LEN + 1];
    char previous_base_url[SPOOLMAN_URL_MAX_LEN + 1];
    bool reused_existing_api = false;
    SpoolmanApi* test_api = NULL;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    snprintf(candidate, sizeof(candidate), "%s", app->spoolman_base_url_input);
    snprintf(
        previous_base_url,
        sizeof(previous_base_url),
        "%s",
        furi_string_get_cstr(app->spoolman_base_url));
    furi_mutex_release(app->mutex);

    app_trim_ascii_whitespace(candidate);
    if(candidate[0] == '\0') {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        app->status = AppStatusSpoolmanError;
        app_clear_http_status(app);
        furi_string_set_str(app->spoolman_error, "URL is empty");
        furi_mutex_release(app->mutex);
        view_port_update(app->view_port);
        return 0;
    }

    if(app->spoolman_api) {
        reused_existing_api = spoolman_api_set_base_url(app->spoolman_api, candidate);
        if(reused_existing_api) {
            test_api = app->spoolman_api;
        }
    }

    if(!test_api) {
        test_api = spoolman_api_alloc(candidate);
    }

    SpoolmanApiResult result = SpoolmanApiResultHttpUnavailable;
    if(test_api) {
        result = spoolman_api_health_check(test_api);
    }

    if(result == SpoolmanApiResultOk && test_api) {
        bool saved = app_write_spoolman_url_config(candidate);

        furi_mutex_acquire(app->mutex, FuriWaitForever);
        furi_string_set_str(app->spoolman_base_url, candidate);
        snprintf(
            app->spoolman_base_url_input, sizeof(app->spoolman_base_url_input), "%s", candidate);
        app->status = AppStatusModeSelect;
        furi_string_set_str(
            app->info_message, saved ? "Spoolman URL saved" : "Spoolman OK, save failed");
        app_clear_http_status(app);
        furi_mutex_release(app->mutex);

        if(reused_existing_api) {
            spoolman_api_set_base_url(app->spoolman_api, candidate);
        } else {
            app_reset_spoolman_api(app, candidate);
            spoolman_api_free(test_api);
        }
        view_port_update(app->view_port);
        return 0;
    }

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->status = AppStatusSpoolmanError;
    if(test_api) {
        app->spoolman_status_code = spoolman_api_get_status_code(test_api);
        furi_string_set(app->spoolman_error, spoolman_api_get_last_error(test_api));
    } else {
        furi_string_set_str(app->spoolman_error, "HTTP unavailable");
    }
    furi_mutex_release(app->mutex);

    if(test_api) {
        const char* last_error = spoolman_api_get_last_error(test_api);
        if(spoolman_api_get_status_code(test_api) > 0) {
            FURI_LOG_W(
                TAG,
                "Base URL validation failed: HTTP %d",
                spoolman_api_get_status_code(test_api));
        } else if(last_error && last_error[0] != '\0') {
            FURI_LOG_W(TAG, "Base URL validation failed: %s", last_error);
        } else {
            FURI_LOG_W(TAG, "Base URL validation failed: Spoolman unreachable");
        }
        if(!reused_existing_api) {
            spoolman_api_free(test_api);
        }
    } else {
        FURI_LOG_W(TAG, "Base URL validation failed: HTTP unavailable");
    }

    if(reused_existing_api) {
        spoolman_api_set_base_url(app->spoolman_api, previous_base_url);
    } else {
        app_reset_spoolman_api(app, previous_base_url);
    }
    view_port_update(app->view_port);
    return 0;
}

static void app_base_url_handle_save(SpoolmanSyncApp* app) {
    if(!app) {
        return;
    }

    app_free_base_url_test_thread(app);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->status = AppStatusTestingBaseUrl;
    app_clear_http_status(app);
    furi_string_reset(app->info_message);
    furi_mutex_release(app->mutex);

    app_close_base_url_editor(app);
    view_port_update(app->view_port);

    app->base_url_test_thread = furi_thread_alloc();
    furi_check(app->base_url_test_thread);
    furi_thread_set_name(app->base_url_test_thread, "SpoolmanUrlTest");
    furi_thread_set_stack_size(app->base_url_test_thread, 2048);
    furi_thread_set_context(app->base_url_test_thread, app);
    furi_thread_set_callback(app->base_url_test_thread, app_base_url_test_worker);
    furi_thread_start(app->base_url_test_thread);
}

static void app_base_url_result_callback(void* context) {
    SpoolmanSyncApp* app = context;
    if(!app) {
        return;
    }

    AppEvent event = {.type = AppEventTypeBaseUrlSave};
    furi_message_queue_put(app->input_queue, &event, FuriWaitForever);
}

static void app_base_url_handle_back(SpoolmanSyncApp* app) {
    if(!app) {
        return;
    }

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->status = AppStatusModeSelect;
    app_clear_http_status(app);
    furi_mutex_release(app->mutex);

    app_close_base_url_editor(app);
}

static void app_base_url_back_callback(void* context) {
    SpoolmanSyncApp* app = context;
    if(!app) {
        return;
    }

    AppEvent event = {.type = AppEventTypeBaseUrlBack};
    furi_message_queue_put(app->input_queue, &event, FuriWaitForever);
}

void app_open_base_url_editor(SpoolmanSyncApp* app) {
    if(!app) {
        return;
    }

    app_stop_create_mode(app);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    const char* current_base_url = furi_string_get_cstr(app->spoolman_base_url);
    snprintf(
        app->spoolman_base_url_input,
        sizeof(app->spoolman_base_url_input),
        "%s",
        current_base_url[0] != '\0' ? current_base_url : "https://");
    app_clear_http_status(app);
    furi_string_reset(app->info_message);
    app->status = AppStatusEditingBaseUrl;
    furi_mutex_release(app->mutex);

    uart_text_input_reset(app->base_url_text_input);
    uart_text_input_set_header_text(app->base_url_text_input, "Spoolman URL");
    uart_text_input_set_validator(app->base_url_text_input, NULL, NULL);
    uart_text_input_set_result_callback(
        app->base_url_text_input,
        app_base_url_result_callback,
        app,
        app->spoolman_base_url_input,
        sizeof(app->spoolman_base_url_input),
        false);

    view_port_enabled_set(app->view_port, false);
    view_holder_set_view(
        app->base_url_view_holder, uart_text_input_get_view(app->base_url_text_input));
    view_port_update(app->view_port);
}

void app_test_and_save_base_url(SpoolmanSyncApp* app) {
    UNUSED(app);
}

bool app_spool_is_untagged(const SpoolmanSpool* spool) {
    return spool && furi_string_empty(spool->tag);
}

static void app_reset_update_scan_state(SpoolmanSyncApp* app) {
    memset(&app->update_scan_result, 0, sizeof(app->update_scan_result));
    app->update_scan_active = false;
    app->update_scan_has_result = false;
    app->update_scan_error = false;
    app->update_patch_in_progress = false;
    app->update_patch_error = false;
}

static void
    app_format_scan_result_hex(const NfcScanResult* result, char* output, size_t output_size) {
    furi_assert(result);
    furi_assert(output);
    furi_assert(output_size >= (NFC_BLOCK_SIZE * 2) + 1);

    for(size_t i = 0; i < NFC_BLOCK_SIZE; i++) {
        snprintf(&output[i * 2], 3, "%02X", result->block9[i]);
    }
}

static const SpoolmanSpool*
    app_find_spool_by_tag(const SpoolmanSpoolList* spools, const char* tag, int exclude_spool_id) {
    if(!spools || !tag) {
        return NULL;
    }

    for(size_t i = 0; i < spools->count; i++) {
        const SpoolmanSpool* spool = &spools->items[i];
        if(spool->id == exclude_spool_id || furi_string_empty(spool->tag)) {
            continue;
        }

        if(strcmp(furi_string_get_cstr(spool->tag), tag) == 0) {
            return spool;
        }
    }

    return NULL;
}

const SpoolmanSpool*
    app_get_untagged_spool_by_index(const SpoolmanSyncApp* app, size_t untagged_index) {
    if(!app) {
        return NULL;
    }

    size_t current_index = 0;
    for(size_t i = 0; i < app->spools.count; i++) {
        if(!app_spool_is_untagged(&app->spools.items[i])) {
            continue;
        }

        if(current_index == untagged_index) {
            return &app->spools.items[i];
        }
        current_index++;
    }

    return NULL;
}

static void draw_callback(Canvas* canvas, void* context) {
    SpoolmanSyncApp* app = context;
    app_pages_draw(canvas, app);
}

static void input_callback(InputEvent* input_event, void* context) {
    SpoolmanSyncApp* app = context;
    AppEvent event = {
        .type = AppEventTypeInput,
        .input = *input_event,
    };
    furi_message_queue_put(app->input_queue, &event, 0);
}

static void nfc_service_callback(
    SpoolmanNfcServiceEvent event,
    const NfcScanResult* result,
    void* context) {
    SpoolmanSyncApp* app = context;
    bool play_tag_found_sound = false;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if(app->status == AppStatusUpdateMode) {
        if(event == SpoolmanNfcServiceEventReading) {
            app->update_scan_active = true;
            app->update_scan_error = false;
            app->update_patch_error = false;
        } else if(event == SpoolmanNfcServiceEventSuccess) {
            app->update_scan_active = false;
            app->update_scan_has_result = true;
            app->update_scan_error = false;
            app->update_scan_result = *result;
            play_tag_found_sound = true;
        } else if(event == SpoolmanNfcServiceEventError) {
            app->update_scan_active = false;
            app->update_scan_error = true;
        }
    } else if(event == SpoolmanNfcServiceEventReading) {
        app->status = AppStatusReading;
    } else if(event == SpoolmanNfcServiceEventSuccess) {
        app->status = AppStatusSuccess;
        app->scan_result = *result;
        play_tag_found_sound = true;
    } else if(event == SpoolmanNfcServiceEventError) {
        app->status = AppStatusError;
    }
    furi_mutex_release(app->mutex);

    if(play_tag_found_sound) {
        app_notify(app, &sequence_tag_found);
    }

    view_port_update(app->view_port);
}

void app_stop_create_mode(SpoolmanSyncApp* app) {
    if(app->nfc_service) {
        nfc_reader_stop(app->nfc_service);
        app->nfc_service = NULL;
    }
}

void app_check_spoolman_health(SpoolmanSyncApp* app) {
    app_stop_create_mode(app);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->status = AppStatusCheckingSpoolman;
    app_clear_http_status(app);
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);

    app_ensure_spoolman_api(app);

    SpoolmanApiResult result = SpoolmanApiResultHttpUnavailable;
    if(app->spoolman_api) {
        result = spoolman_api_health_check(app->spoolman_api);
    }

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if(result == SpoolmanApiResultOk) {
        FURI_LOG_I(TAG, "Spoolman health check OK");
        app->status = AppStatusModeSelect;
    } else {
        FURI_LOG_E(TAG, "Spoolman health check failed: result=%d", result);
        app->status = AppStatusSpoolmanError;
        if(app->spoolman_api) {
            app->spoolman_status_code = spoolman_api_get_status_code(app->spoolman_api);
            furi_string_set(app->spoolman_error, spoolman_api_get_last_error(app->spoolman_api));
        } else {
            furi_string_set(app->spoolman_error, "HTTP unavailable");
        }
    }
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);
}

void app_start_update_scan(SpoolmanSyncApp* app) {
    app_stop_create_mode(app);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app_reset_update_scan_state(app);
    app->update_scan_active = true;
    app_clear_http_status(app);
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);

    app->nfc_service = nfc_reader_alloc(nfc_service_callback, app);
    nfc_reader_start(app->nfc_service);
}

void app_cancel_update_scan(SpoolmanSyncApp* app) {
    app_stop_create_mode(app);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app_reset_update_scan_state(app);
    app_clear_http_status(app);
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);
}

void app_update_skip_current_spool(SpoolmanSyncApp* app, bool forward) {
    app_stop_create_mode(app);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app_reset_update_scan_state(app);
    app_clear_http_status(app);

    if(forward) {
        if(!app->update_detail_visible) {
            if(app->spools_to_update_count > 0) {
                app->update_detail_visible = true;
                app->current_untagged_spool_index = 0;
            }
        } else if(app->current_untagged_spool_index + 1 < app->spools_to_update_count) {
            app->current_untagged_spool_index++;
        } else {
            app->update_detail_visible = false;
            app->current_untagged_spool_index = 0;
        }
    } else if(app->spools_to_update_count > 0) {
        if(!app->update_detail_visible) {
            app->update_detail_visible = true;
            app->current_untagged_spool_index = app->spools_to_update_count - 1;
        } else if(app->current_untagged_spool_index > 0) {
            app->current_untagged_spool_index--;
        } else {
            app->update_detail_visible = false;
            app->current_untagged_spool_index = 0;
        }
    }
    furi_mutex_release(app->mutex);

    view_port_update(app->view_port);
}

bool app_update_confirm_current_spool(SpoolmanSyncApp* app) {
    if(!app || !app->spoolman_api) {
        return false;
    }

    int spool_id = 0;
    char tag_hex[(NFC_BLOCK_SIZE * 2) + 1];
    SpoolmanApiResult result = SpoolmanApiResultInvalidArguments;
    SpoolmanSpoolList remote_spools;
    spoolman_spool_list_init(&remote_spools);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    const SpoolmanSpool* spool =
        app_get_untagged_spool_by_index(app, app->current_untagged_spool_index);
    if(!spool || !app->update_scan_has_result) {
        furi_mutex_release(app->mutex);
        spoolman_spool_list_clear(&remote_spools);
        return false;
    }

    spool_id = spool->id;
    app_format_scan_result_hex(&app->update_scan_result, tag_hex, sizeof(tag_hex));
    app->update_patch_in_progress = true;
    app->update_patch_error = false;
    app_clear_http_status(app);
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);

    app_stop_create_mode(app);

    result = spoolman_api_get_spools(app->spoolman_api, &remote_spools);
    if(result == SpoolmanApiResultOk) {
        const SpoolmanSpool* existing_spool =
            app_find_spool_by_tag(&remote_spools, tag_hex, spool_id);

        if(existing_spool) {
            char error_message[48];
            snprintf(
                error_message,
                sizeof(error_message),
                "Tag already defined on #%d",
                existing_spool->id);
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            app->update_patch_in_progress = false;
            app->update_patch_error = true;
            app->spoolman_status_code = 0;
            furi_string_set_str(app->spoolman_error, error_message);
            furi_mutex_release(app->mutex);
            spoolman_spool_list_clear(&remote_spools);
            app_notify(app, &sequence_error);
            view_port_update(app->view_port);
            return false;
        }

        result = spoolman_api_update_spool_tag(app->spoolman_api, spool_id, tag_hex);
    }

    spoolman_spool_list_clear(&remote_spools);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->update_patch_in_progress = false;
    if(result == SpoolmanApiResultOk) {
        SpoolmanSpool* mutable_spool = NULL;
        size_t current_index = 0;
        for(size_t i = 0; i < app->spools.count; i++) {
            if(!app_spool_is_untagged(&app->spools.items[i])) {
                continue;
            }
            if(current_index == app->current_untagged_spool_index) {
                mutable_spool = &app->spools.items[i];
                break;
            }
            current_index++;
        }

        if(mutable_spool) {
            furi_string_set_str(mutable_spool->tag, tag_hex);
        }

        if(app->spools_to_update_count > 0) {
            app->spools_to_update_count--;
        }
        if(app->spools_to_update_count == 0) {
            app->update_detail_visible = false;
            app->current_untagged_spool_index = 0;
        } else if(app->current_untagged_spool_index >= app->spools_to_update_count) {
            app->current_untagged_spool_index = app->spools_to_update_count - 1;
        }

        app_reset_update_scan_state(app);
    } else {
        app->update_patch_error = true;
        app->spoolman_status_code = spoolman_api_get_status_code(app->spoolman_api);
        furi_string_set(app->spoolman_error, spoolman_api_get_last_error(app->spoolman_api));
    }
    furi_mutex_release(app->mutex);

    app_notify(app, result == SpoolmanApiResultOk ? &sequence_success : &sequence_error);
    view_port_update(app->view_port);

    return result == SpoolmanApiResultOk;
}

void app_start_update_mode(SpoolmanSyncApp* app) {
    app_stop_create_mode(app);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->status = AppStatusLoadingUpdate;
    app->spools_to_update_count = 0;
    app->total_spools_count = 0;
    app->current_untagged_spool_index = 0;
    app->update_detail_visible = false;
    app->spoolman_status_code = 0;
    spoolman_spool_list_clear(&app->spools);
    app_reset_update_scan_state(app);
    furi_string_reset(app->spoolman_error);
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);

    app_ensure_spoolman_api(app);

    SpoolmanApiResult result = SpoolmanApiResultHttpUnavailable;
    SpoolmanSpoolList spools;
    spoolman_spool_list_init(&spools);
    if(app->spoolman_api) {
        result = spoolman_api_get_spools(app->spoolman_api, &spools);
        if(result == SpoolmanApiResultOk) {
            spoolman_api_fill_missing_spool_details(app->spoolman_api, &spools);
        }
    }

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if(result == SpoolmanApiResultOk) {
        size_t untagged_spool_count = 0;
        for(size_t i = 0; i < spools.count; i++) {
            if(app_spool_is_untagged(&spools.items[i])) {
                untagged_spool_count++;
            }
        }

        FURI_LOG_I(
            TAG,
            "Loaded %zu untagged spools out of %zu from Spoolman",
            untagged_spool_count,
            spools.count);
        app->spools = spools;
        app->spools_to_update_count = untagged_spool_count;
        app->total_spools_count = spools.count;
        app->status = AppStatusUpdateMode;
    } else {
        spoolman_spool_list_clear(&spools);
        FURI_LOG_E(TAG, "Failed to load spools: result=%d", result);
        app->status = AppStatusUpdateError;
        if(app->spoolman_api) {
            app->spoolman_status_code = spoolman_api_get_status_code(app->spoolman_api);
            furi_string_set(app->spoolman_error, spoolman_api_get_last_error(app->spoolman_api));
        } else {
            furi_string_set(app->spoolman_error, "HTTP unavailable");
        }
    }
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);
}

void app_return_to_menu(SpoolmanSyncApp* app) {
    app_stop_create_mode(app);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->status = AppStatusModeSelect;
    app_reset_update_scan_state(app);
    furi_mutex_release(app->mutex);

    view_port_update(app->view_port);
}

int32_t entrypoint(void* p) {
    UNUSED(p);
    SpoolmanSyncApp* app = malloc(sizeof(SpoolmanSyncApp));
    memset(app, 0, sizeof(SpoolmanSyncApp));
    app->status = AppStatusCheckingSpoolman;
    app->selected_mode = AppModeUpdate;
    spoolman_spool_list_init(&app->spools);
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->input_queue = furi_message_queue_alloc(8, sizeof(AppEvent));
    app->spoolman_base_url = app_load_spoolman_base_url();
    app->info_message = furi_string_alloc();
    app->spoolman_error = furi_string_alloc();

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->base_url_text_input = uart_text_input_alloc();
    app->base_url_view_holder = view_holder_alloc();
    view_holder_attach_to_gui(app->base_url_view_holder, app->gui);
    view_holder_set_back_callback(app->base_url_view_holder, app_base_url_back_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    if(app_has_spoolman_base_url(app)) {
        app_check_spoolman_health(app);
    } else {
        app->status = AppStatusModeSelect;
        app_open_base_url_editor(app);
    }

    AppEvent event;
    while(furi_message_queue_get(app->input_queue, &event, FuriWaitForever) == FuriStatusOk) {
        if(event.type == AppEventTypeInput) {
            if(app_pages_handle_input(app, &event.input)) {
                break;
            }
        } else if(event.type == AppEventTypeBaseUrlSave) {
            app_base_url_handle_save(app);
        } else if(event.type == AppEventTypeBaseUrlBack) {
            app_base_url_handle_back(app);
        }
    }

    app_stop_create_mode(app);

    app_free_base_url_test_thread(app);
    view_holder_set_view(app->base_url_view_holder, NULL);
    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    view_holder_free(app->base_url_view_holder);
    uart_text_input_free(app->base_url_text_input);
    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);
    if(app->spoolman_api) {
        spoolman_api_free(app->spoolman_api);
    }
    spoolman_spool_list_clear(&app->spools);
    furi_string_free(app->spoolman_base_url);
    furi_string_free(app->info_message);
    furi_string_free(app->spoolman_error);
    furi_mutex_free(app->mutex);
    free(app);

    return 0;
}
