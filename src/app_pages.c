#include "app_pages.h"
#include "app_page.h"

void app_pages_draw(Canvas* canvas, SpoolmanSyncApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    canvas_set_font(canvas, FontPrimary);
    switch(app->status) {
    case AppStatusCheckingSpoolman:
    case AppStatusSpoolmanError:
        app_page_spoolman_draw(canvas, app);
        break;
    case AppStatusModeSelect:
    case AppStatusEditingBaseUrl:
    case AppStatusTestingBaseUrl:
        app_page_mode_select_draw(canvas, app);
        break;
    case AppStatusLoadingUpdate:
    case AppStatusUpdateMode:
    case AppStatusUpdateError:
        app_page_update_draw(canvas, app);
        break;
    case AppStatusScanReady:
    case AppStatusScanReading:
    case AppStatusScanSuccess:
    case AppStatusScanError:
        app_page_scan_draw(canvas, app);
        break;
    case AppStatusCreateReady:
    case AppStatusCreateReading:
    case AppStatusCreateSearching:
    case AppStatusCreateConfirm:
    case AppStatusCreateSaving:
    case AppStatusCreateSuccess:
    case AppStatusCreateError:
        app_page_create_draw(canvas, app);
        break;
    }

    furi_mutex_release(app->mutex);
}

static bool app_handle_back_input(SpoolmanSyncApp* app) {
    if(app->status == AppStatusModeSelect || app->status == AppStatusSpoolmanError) {
        return true;
    }

    app_return_to_menu(app);
    view_port_update(app->view_port);
    return false;
}

bool app_pages_handle_input(SpoolmanSyncApp* app, const InputEvent* event) {
    if(event->type != InputTypeShort) {
        return false;
    }

    if(event->key == InputKeyBack) {
        return app_handle_back_input(app);
    }

    if(app->status == AppStatusCheckingSpoolman || app->status == AppStatusSpoolmanError) {
        app_page_spoolman_handle_input(app, event);
        return false;
    }

    if(app->status == AppStatusLoadingUpdate || app->status == AppStatusUpdateMode ||
       app->status == AppStatusUpdateError) {
        app_page_update_handle_input(app, event);
        return false;
    }

    if(app->status == AppStatusModeSelect) {
        app_page_mode_select_handle_input(app, event);
        return false;
    }

    if(app->status == AppStatusScanReady || app->status == AppStatusScanReading ||
       app->status == AppStatusScanSuccess || app->status == AppStatusScanError) {
        app_page_scan_handle_input(app, event);
        return false;
    }

    if(app->status == AppStatusCreateReady || app->status == AppStatusCreateReading ||
       app->status == AppStatusCreateSearching || app->status == AppStatusCreateConfirm ||
       app->status == AppStatusCreateSaving || app->status == AppStatusCreateSuccess ||
       app->status == AppStatusCreateError) {
        app_page_create_handle_input(app, event);
        return false;
    }

    return false;
}
