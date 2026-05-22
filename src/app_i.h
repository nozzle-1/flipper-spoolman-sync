#pragma once

#include "app.h"
#include "services/nfc_reader.h"
#include "services/spoolman_api.h"

#include <gui/gui.h>
#include <gui/view_holder.h>
#include <input/input.h>
#include <notification/notification.h>

#define SPOOLMAN_URL_MAX_LEN 255

typedef struct UART_TextInput UART_TextInput;

typedef enum {
    AppStatusCheckingSpoolman,
    AppStatusSpoolmanError,
    AppStatusModeSelect,
    AppStatusEditingBaseUrl,
    AppStatusTestingBaseUrl,
    AppStatusLoadingUpdate,
    AppStatusUpdateMode,
    AppStatusUpdateError,
    AppStatusFindReady,
    AppStatusFindReading,
    AppStatusFindSearching,
    AppStatusFindSuccess,
    AppStatusFindError,
    AppStatusScanReady,
    AppStatusScanReading,
    AppStatusScanSuccess,
    AppStatusScanError,
    AppStatusCreateReady,
    AppStatusCreateReading,
    AppStatusCreateSearching,
    AppStatusCreateConfirm,
    AppStatusCreateSaving,
    AppStatusCreateSuccess,
    AppStatusCreateError,
    AppStatusCount,
} AppStatus;

typedef enum {
    AppModeCreate,
    AppModeFind,
    AppModeUpdate,
    AppModeScan,
    AppModeConfig,
    AppModeCount,
} AppMode;

typedef enum {
    AppEventTypeInput,
    AppEventTypeBaseUrlSave,
    AppEventTypeBaseUrlBack,
    AppEventTypeCreateProcess,
    AppEventTypeFindProcess,
    AppEventTypeCount,
} AppEventType;

typedef struct {
    AppEventType type;
    InputEvent input;
} AppEvent;

typedef struct {
    FuriMutex* mutex;
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;
    NotificationApp* notifications;
} AppRuntime;

typedef struct {
    ViewHolder* base_url_view_holder;
    UART_TextInput* base_url_text_input;
    FuriThread* base_url_test_thread;
    NfcReader* nfc_reader;
    SpoolmanApi* spoolman_api;
} AppServices;

typedef struct {
    int status_code;
    FuriString* base_url;
    char base_url_input[SPOOLMAN_URL_MAX_LEN + 1];
    FuriString* info_message;
    FuriString* error;
} AppServerState;

typedef struct {
    NfcScanResult scan_result;
    uint8_t read_page;
    FuriString* error;
} AppScanState;

typedef struct {
    NfcScanResult scan_result;
    FuriString* error;
    FuriString* filament_name;
    FuriString* filament_material;
    FuriString* existing_spool_name;
    FuriString* existing_spool_material;
    int filament_id;
    int spool_id;
    int existing_spool_id;
    double filament_weight;
    double existing_spool_weight;
    bool existing_spool_has_remaining_weight;
} AppCreateState;

typedef struct {
    NfcScanResult scan_result;
    FuriString* error;
    FuriString* filament_name;
    FuriString* filament_material;
    int filament_id;
    int spool_id;
    double spool_weight;
    bool spool_has_remaining_weight;
} AppFindState;

typedef struct {
    SpoolmanSpoolList spools;
    NfcScanResult scan_result;
    size_t untagged_spools_count;
    size_t total_spools_count;
    size_t current_untagged_index;
    bool detail_visible;
    bool scan_active;
    bool scan_has_result;
    bool scan_error;
    bool patch_in_progress;
    bool patch_error;
} AppUpdateState;

struct SpoolmanSyncApp {
    AppStatus status;
    AppMode selected_mode;
    AppRuntime runtime;
    AppServices services;
    AppServerState server;
    AppScanState scan;
    AppCreateState create;
    AppFindState find;
    AppUpdateState update;
};

const SpoolmanSpool*
    app_get_untagged_spool_by_index(const SpoolmanSyncApp* app, size_t untagged_index);

void app_check_spoolman_health(SpoolmanSyncApp* app);
void app_start_update_mode(SpoolmanSyncApp* app);
void app_start_scan_mode(SpoolmanSyncApp* app);
void app_start_scan_read(SpoolmanSyncApp* app);
void app_start_find_mode(SpoolmanSyncApp* app);
void app_start_find_scan(SpoolmanSyncApp* app);
void app_start_create_mode(SpoolmanSyncApp* app);
void app_start_create_scan(SpoolmanSyncApp* app);
void app_confirm_create_spool(SpoolmanSyncApp* app);
void app_start_update_scan(SpoolmanSyncApp* app);
void app_cancel_update_scan(SpoolmanSyncApp* app);
void app_update_skip_current_spool(SpoolmanSyncApp* app, bool forward);
bool app_update_confirm_current_spool(SpoolmanSyncApp* app);
void app_stop_create_mode(SpoolmanSyncApp* app);
void app_return_to_menu(SpoolmanSyncApp* app);
void app_open_base_url_editor(SpoolmanSyncApp* app);
