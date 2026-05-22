#pragma once

#include "app_controller.h"

#include "helpers/app_filament.h"
#include "helpers/app_spool.h"
#include "helpers/app_storage.h"
#include "helpers/app_tag.h"
#include "uart_text_input.h"

#include <notification/notification_messages.h>

extern const NotificationSequence app_sequence_tag_found;

void app_notify(SpoolmanSyncApp* app, const NotificationSequence* sequence);

void app_close_base_url_editor(SpoolmanSyncApp* app);

void app_reset_create_state(SpoolmanSyncApp* app);
void app_reset_find_state(SpoolmanSyncApp* app);
void app_reset_update_scan_state(SpoolmanSyncApp* app);

void app_set_existing_create_spool_details(
    SpoolmanSyncApp* app,
    const SpoolmanSpool* spool,
    const SpoolmanFilament* fallback_filament);

void app_queue_create_process(SpoolmanSyncApp* app);
void app_queue_find_process(SpoolmanSyncApp* app);
void app_stop_nfc_reader(SpoolmanSyncApp* app);
void app_start_nfc_reader(SpoolmanSyncApp* app);
