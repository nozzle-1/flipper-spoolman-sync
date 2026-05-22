#include "app_scene_i.h"

typedef struct {
    AppSceneDrawCallback draw;
    AppSceneInputCallback input;
} AppSceneHandlers;

static AppScene app_scene_from_status(AppStatus status) {
    static const AppScene scene_by_status[AppStatusCount] = {
        [AppStatusCheckingSpoolman] = AppSceneSpoolman,
        [AppStatusSpoolmanError] = AppSceneSpoolman,
        [AppStatusModeSelect] = AppSceneModeSelect,
        [AppStatusEditingBaseUrl] = AppSceneModeSelect,
        [AppStatusTestingBaseUrl] = AppSceneModeSelect,
        [AppStatusLoadingUpdate] = AppSceneUpdate,
        [AppStatusUpdateMode] = AppSceneUpdate,
        [AppStatusUpdateError] = AppSceneUpdate,
        [AppStatusFindReady] = AppSceneFind,
        [AppStatusFindReading] = AppSceneFind,
        [AppStatusFindSearching] = AppSceneFind,
        [AppStatusFindSuccess] = AppSceneFind,
        [AppStatusFindError] = AppSceneFind,
        [AppStatusScanReady] = AppSceneScan,
        [AppStatusScanReading] = AppSceneScan,
        [AppStatusScanSuccess] = AppSceneScan,
        [AppStatusScanError] = AppSceneScan,
        [AppStatusCreateReady] = AppSceneCreate,
        [AppStatusCreateReading] = AppSceneCreate,
        [AppStatusCreateSearching] = AppSceneCreate,
        [AppStatusCreateConfirm] = AppSceneCreate,
        [AppStatusCreateSaving] = AppSceneCreate,
        [AppStatusCreateSuccess] = AppSceneCreate,
        [AppStatusCreateError] = AppSceneCreate,
    };

    return status < AppStatusCount ? scene_by_status[status] : AppSceneModeSelect;
}

static bool app_scene_handle_back(SpoolmanSyncApp* app) {
    if(app->status == AppStatusModeSelect || app->status == AppStatusSpoolmanError) {
        return true;
    }

    app_return_to_menu(app);
    view_port_update(app->runtime.view_port);
    return false;
}

void app_scene_draw(Canvas* canvas, SpoolmanSyncApp* app) {
    static const AppSceneHandlers handlers[AppSceneCount] = {
        [AppSceneSpoolman] = {app_scene_spoolman_draw, app_scene_spoolman_handle_input},
        [AppSceneModeSelect] = {app_scene_mode_select_draw, app_scene_mode_select_handle_input},
        [AppSceneUpdate] = {app_scene_update_draw, app_scene_update_handle_input},
        [AppSceneFind] = {app_scene_find_draw, app_scene_find_handle_input},
        [AppSceneScan] = {app_scene_scan_draw, app_scene_scan_handle_input},
        [AppSceneCreate] = {app_scene_create_draw, app_scene_create_handle_input},
    };

    furi_mutex_acquire(app->runtime.mutex, FuriWaitForever);
    AppScene scene = app_scene_from_status(app->status);
    handlers[scene].draw(canvas, app);
    furi_mutex_release(app->runtime.mutex);
}

bool app_scene_handle_input(SpoolmanSyncApp* app, const InputEvent* event) {
    static const AppSceneHandlers handlers[AppSceneCount] = {
        [AppSceneSpoolman] = {app_scene_spoolman_draw, app_scene_spoolman_handle_input},
        [AppSceneModeSelect] = {app_scene_mode_select_draw, app_scene_mode_select_handle_input},
        [AppSceneUpdate] = {app_scene_update_draw, app_scene_update_handle_input},
        [AppSceneFind] = {app_scene_find_draw, app_scene_find_handle_input},
        [AppSceneScan] = {app_scene_scan_draw, app_scene_scan_handle_input},
        [AppSceneCreate] = {app_scene_create_draw, app_scene_create_handle_input},
    };

    if(event->type != InputTypeShort) {
        return false;
    }

    if(event->key == InputKeyBack) {
        return app_scene_handle_back(app);
    }

    AppScene scene = app_scene_from_status(app->status);
    handlers[scene].input(app, event);
    return false;
}
