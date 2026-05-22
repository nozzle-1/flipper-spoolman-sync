#include "app_controller_i.h"

static int32_t app_base_url_test_worker(void* context) {
    SpoolmanSyncApp* app = context;
    if(!app) {
        return -1;
    }

    char candidate[SPOOLMAN_URL_MAX_LEN + 1];
    char previous_base_url[SPOOLMAN_URL_MAX_LEN + 1];
    bool reused_existing_api = false;
    SpoolmanApi* test_api = NULL;

    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    snprintf(candidate, sizeof(candidate), "%s", app->server.base_url_input);
    snprintf(
        previous_base_url,
        sizeof(previous_base_url),
        "%s",
        furi_string_get_cstr(app->server.base_url));
    furi_mutex_release(app->runtime.mutex);

    app_storage_trim_ascii_whitespace(candidate);
    if(candidate[0] == '\0') {
        furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
        app->status = AppStatusSpoolmanError;
        app_clear_http_status(app);
        furi_string_set_str(app->server.error, "Server URL is empty");
        furi_mutex_release(app->runtime.mutex);
        view_port_update(app->runtime.view_port);
        return 0;
    }

    if(app->services.spoolman_api) {
        reused_existing_api = spoolman_api_set_base_url(app->services.spoolman_api, candidate);
        if(reused_existing_api) {
            test_api = app->services.spoolman_api;
        }
    }

    if(!test_api) {
        test_api = spoolman_api_alloc(candidate);
    }

    SpoolmanApiResult result =
        test_api ? spoolman_api_health_check(test_api) : SpoolmanApiResultHttpUnavailable;

    if(result == SpoolmanApiResultOk && test_api) {
        bool saved = app_storage_write_spoolman_url(candidate);

        furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
        furi_string_set_str(app->server.base_url, candidate);
        snprintf(
            app->server.base_url_input, sizeof(app->server.base_url_input), "%s", candidate);
        app->status = AppStatusModeSelect;
        furi_string_set_str(
            app->server.info_message,
            saved ? "Server saved and reachable" : "Server OK, save failed");
        app_clear_http_status(app);
        furi_mutex_release(app->runtime.mutex);

        if(reused_existing_api) {
            spoolman_api_set_base_url(app->services.spoolman_api, candidate);
        } else {
            app_reset_spoolman_api(app, candidate);
            spoolman_api_free(test_api);
        }

        view_port_update(app->runtime.view_port);
        return 0;
    }

    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    app->status = AppStatusSpoolmanError;
    if(test_api) {
        app->server.status_code = spoolman_api_get_status_code(test_api);
        furi_string_set(app->server.error, spoolman_api_get_last_error(test_api));
    } else {
        furi_string_set_str(app->server.error, "HTTP unavailable");
    }
    furi_mutex_release(app->runtime.mutex);

    if(test_api && !reused_existing_api) {
        spoolman_api_free(test_api);
    }

    if(reused_existing_api) {
        spoolman_api_set_base_url(app->services.spoolman_api, previous_base_url);
    } else {
        app_reset_spoolman_api(app, previous_base_url);
    }

    view_port_update(app->runtime.view_port);
    return 0;
}

static void app_base_url_result_callback(void* context) {
    SpoolmanSyncApp* app = context;
    if(!app) {
        return;
    }

    AppEvent event = {.type = AppEventTypeBaseUrlSave};
    furi_message_queue_put(app->runtime.input_queue, &event, FuriWaitForever);
}

void app_free_base_url_test_thread(SpoolmanSyncApp* app) {
    if(!app || !app->services.base_url_test_thread) {
        return;
    }

    furi_thread_join(app->services.base_url_test_thread);
    furi_thread_free(app->services.base_url_test_thread);
    app->services.base_url_test_thread = NULL;
}

void app_handle_base_url_save(SpoolmanSyncApp* app) {
    if(!app) {
        return;
    }

    app_free_base_url_test_thread(app);

    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    app->status = AppStatusTestingBaseUrl;
    app_clear_http_status(app);
    furi_string_reset(app->server.info_message);
    furi_mutex_release(app->runtime.mutex);

    app_close_base_url_editor(app);
    view_port_update(app->runtime.view_port);

    app->services.base_url_test_thread = furi_thread_alloc();
    furi_check(app->services.base_url_test_thread);
    furi_thread_set_name(app->services.base_url_test_thread, "SpoolmanUrlTest");
    furi_thread_set_stack_size(app->services.base_url_test_thread, 2048);
    furi_thread_set_context(app->services.base_url_test_thread, app);
    furi_thread_set_callback(app->services.base_url_test_thread, app_base_url_test_worker);
    furi_thread_start(app->services.base_url_test_thread);
}

void app_handle_base_url_back(SpoolmanSyncApp* app) {
    if(!app) {
        return;
    }

    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    app->status = AppStatusModeSelect;
    app_clear_http_status(app);
    furi_mutex_release(app->runtime.mutex);

    app_close_base_url_editor(app);
}

void app_handle_base_url_back_callback(void* context) {
    SpoolmanSyncApp* app = context;
    if(!app) {
        return;
    }

    AppEvent event = {.type = AppEventTypeBaseUrlBack};
    furi_message_queue_put(app->runtime.input_queue, &event, FuriWaitForever);
}

void app_open_base_url_editor(SpoolmanSyncApp* app) {
    if(!app) {
        return;
    }

    app_stop_create_mode(app);

    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    const char* current_base_url = furi_string_get_cstr(app->server.base_url);
    snprintf(
        app->server.base_url_input,
        sizeof(app->server.base_url_input),
        "%s",
        current_base_url[0] != '\0' ? current_base_url : "https://");
    app_clear_http_status(app);
    furi_string_reset(app->server.info_message);
    app->status = AppStatusEditingBaseUrl;
    furi_mutex_release(app->runtime.mutex);

    uart_text_input_reset(app->services.base_url_text_input);
    uart_text_input_set_header_text(app->services.base_url_text_input, "Server URL");
    uart_text_input_set_validator(app->services.base_url_text_input, NULL, NULL);
    uart_text_input_set_result_callback(
        app->services.base_url_text_input,
        app_base_url_result_callback,
        app,
        app->server.base_url_input,
        sizeof(app->server.base_url_input),
        false);

    view_port_enabled_set(app->runtime.view_port, false);
    view_holder_set_view(
        app->services.base_url_view_holder,
        uart_text_input_get_view(app->services.base_url_text_input));
    view_port_update(app->runtime.view_port);
}

void app_check_spoolman_health(SpoolmanSyncApp* app) {
    app_stop_create_mode(app);

    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    app->status = AppStatusCheckingSpoolman;
    app_clear_http_status(app);
    furi_mutex_release(app->runtime.mutex);
    view_port_update(app->runtime.view_port);

    app_ensure_spoolman_api(app);

    SpoolmanApiResult result = app->services.spoolman_api ?
                                   spoolman_api_health_check(app->services.spoolman_api) :
                                   SpoolmanApiResultHttpUnavailable;

    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    if(result == SpoolmanApiResultOk) {
        app->status = AppStatusModeSelect;
    } else {
        app->status = AppStatusSpoolmanError;
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
