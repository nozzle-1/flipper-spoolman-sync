#include "app_i.h"

#include "controllers/app_controller.h"
#include "helpers/app_storage.h"
#include "scenes/app_scene.h"
#include "uart_text_input.h"

#include <furi.h>

static void app_draw_callback(Canvas* canvas, void* context) {
    app_scene_draw(canvas, context);
}

static void app_input_callback(InputEvent* input_event, void* context) {
    SpoolmanSyncApp* app = context;
    AppEvent event = {
        .type = AppEventTypeInput,
        .input = *input_event,
    };
    furi_message_queue_put(app->runtime.input_queue, &event, 0);
}

static SpoolmanSyncApp* app_alloc(void) {
    SpoolmanSyncApp* app = malloc(sizeof(SpoolmanSyncApp));
    memset(app, 0, sizeof(SpoolmanSyncApp));

    app->status = AppStatusCheckingSpoolman;
    app->selected_mode = AppModeCreate;
    spoolman_spool_list_init(&app->update.spools);
    app->runtime.mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->runtime.input_queue = furi_message_queue_alloc(8, sizeof(AppEvent));
    app->server.base_url = app_storage_load_spoolman_url();
    app->server.info_message = furi_string_alloc();
    app->server.error = furi_string_alloc();
    app->scan.error = furi_string_alloc();
    app->create.error = furi_string_alloc();
    app->create.filament_name = furi_string_alloc();
    app->create.filament_material = furi_string_alloc();
    app->create.existing_spool_name = furi_string_alloc();
    app->create.existing_spool_material = furi_string_alloc();
    app->find.error = furi_string_alloc();
    app->find.filament_name = furi_string_alloc();
    app->find.filament_material = furi_string_alloc();

    app->runtime.view_port = view_port_alloc();
    view_port_draw_callback_set(app->runtime.view_port, app_draw_callback, app);
    view_port_input_callback_set(app->runtime.view_port, app_input_callback, app);

    app->runtime.gui = furi_record_open(RECORD_GUI);
    app->runtime.notifications = furi_record_open(RECORD_NOTIFICATION);
    app->services.base_url_text_input = uart_text_input_alloc();
    app->services.base_url_view_holder = view_holder_alloc();
    view_holder_attach_to_gui(app->services.base_url_view_holder, app->runtime.gui);
    view_holder_set_back_callback(
        app->services.base_url_view_holder, app_handle_base_url_back_callback, app);
    gui_add_view_port(app->runtime.gui, app->runtime.view_port, GuiLayerFullscreen);

    return app;
}

static void app_free(SpoolmanSyncApp* app) {
    if(!app) return;

    app_stop_create_mode(app);
    app_free_base_url_test_thread(app);

    view_holder_set_view(app->services.base_url_view_holder, NULL);
    gui_remove_view_port(app->runtime.gui, app->runtime.view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    view_holder_free(app->services.base_url_view_holder);
    uart_text_input_free(app->services.base_url_text_input);
    view_port_free(app->runtime.view_port);
    furi_message_queue_free(app->runtime.input_queue);

    if(app->services.spoolman_api) {
        spoolman_api_free(app->services.spoolman_api);
    }

    spoolman_spool_list_clear(&app->update.spools);
    furi_string_free(app->server.base_url);
    furi_string_free(app->server.info_message);
    furi_string_free(app->server.error);
    furi_string_free(app->scan.error);
    furi_string_free(app->create.error);
    furi_string_free(app->create.filament_name);
    furi_string_free(app->create.filament_material);
    furi_string_free(app->create.existing_spool_name);
    furi_string_free(app->create.existing_spool_material);
    furi_string_free(app->find.error);
    furi_string_free(app->find.filament_name);
    furi_string_free(app->find.filament_material);
    furi_mutex_free(app->runtime.mutex);
    free(app);
}

static void app_bootstrap(SpoolmanSyncApp* app) {
    if(app_has_spoolman_base_url(app)) {
        app_check_spoolman_health(app);
    } else {
        app->status = AppStatusModeSelect;
        app_open_base_url_editor(app);
    }
}

static void app_handle_event(SpoolmanSyncApp* app, const AppEvent* event, bool* running) {
    if(event->type == AppEventTypeInput) {
        if(app_scene_handle_input(app, &event->input)) {
            *running = false;
        }
    } else if(event->type == AppEventTypeBaseUrlSave) {
        app_handle_base_url_save(app);
    } else if(event->type == AppEventTypeBaseUrlBack) {
        app_handle_base_url_back(app);
    } else if(event->type == AppEventTypeCreateProcess) {
        app_process_create_scan(app);
    } else if(event->type == AppEventTypeFindProcess) {
        app_process_find_scan(app);
    }
}

int32_t entrypoint(void* p) {
    UNUSED(p);

    SpoolmanSyncApp* app = app_alloc();
    app_bootstrap(app);

    bool running = true;
    AppEvent event;
    while(running &&
          furi_message_queue_get(app->runtime.input_queue, &event, FuriWaitForever) ==
              FuriStatusOk) {
        app_handle_event(app, &event, &running);
    }

    app_free(app);
    return 0;
}
