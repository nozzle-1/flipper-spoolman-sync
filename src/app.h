#pragma once

#include "./services/nfc_reader.h"
#include "./services/spoolman_api.h"

#include <gui/gui.h>
#include <gui/view_holder.h>
#include <input/input.h>
#include <notification/notification.h>
#include <storage/storage.h>

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
    AppStatusScanning,
    AppStatusReading,
    AppStatusSuccess,
    AppStatusError,
} AppStatus;

typedef enum {
    AppModeUpdate,
    AppModeCreate,
    AppModeConfig,
} AppMode;

typedef enum {
    AppEventTypeInput,
    AppEventTypeBaseUrlSave,
    AppEventTypeBaseUrlBack,
} AppEventType;

typedef struct {
    AppEventType type;
    InputEvent input;
} AppEvent;

typedef struct {
    AppStatus status;
    AppMode selected_mode;
    NfcScanResult scan_result;
    NfcScanResult update_scan_result;
    FuriMutex* mutex;
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;
    NotificationApp* notifications;
    ViewHolder* base_url_view_holder;
    UART_TextInput* base_url_text_input;
    FuriThread* base_url_test_thread;
    NfcReader* nfc_service;
    SpoolmanApi* spoolman_api;
    FuriString* spoolman_base_url;
    char spoolman_base_url_input[SPOOLMAN_URL_MAX_LEN + 1];
    FuriString* info_message;
    FuriString* spoolman_error;
    int spoolman_status_code;
    size_t spools_to_update_count;
    size_t total_spools_count;
    SpoolmanSpoolList spools;
    size_t current_untagged_spool_index;
    bool update_detail_visible;
    bool update_scan_active;
    bool update_scan_has_result;
    bool update_scan_error;
    bool update_patch_in_progress;
    bool update_patch_error;
} SpoolmanSyncApp;

bool app_spool_is_untagged(const SpoolmanSpool* spool);

const SpoolmanSpool*
    app_get_untagged_spool_by_index(const SpoolmanSyncApp* app, size_t untagged_index);

void app_check_spoolman_health(SpoolmanSyncApp* app);

void app_start_update_mode(SpoolmanSyncApp* app);

void app_start_update_scan(SpoolmanSyncApp* app);

void app_cancel_update_scan(SpoolmanSyncApp* app);

void app_update_skip_current_spool(SpoolmanSyncApp* app, bool forward);

bool app_update_confirm_current_spool(SpoolmanSyncApp* app);

void app_stop_create_mode(SpoolmanSyncApp* app);

void app_return_to_menu(SpoolmanSyncApp* app);

void app_open_base_url_editor(SpoolmanSyncApp* app);

void app_test_and_save_base_url(SpoolmanSyncApp* app);
