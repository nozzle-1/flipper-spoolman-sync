#pragma once

#include "app_i.h"

bool app_has_spoolman_base_url(const SpoolmanSyncApp* app);

bool app_ensure_spoolman_api(SpoolmanSyncApp* app);

void app_reset_spoolman_api(SpoolmanSyncApp* app, const char* base_url);

void app_clear_http_status(SpoolmanSyncApp* app);

void app_free_base_url_test_thread(SpoolmanSyncApp* app);

void app_handle_base_url_save(SpoolmanSyncApp* app);

void app_handle_base_url_back(SpoolmanSyncApp* app);

void app_handle_base_url_back_callback(void* context);

void app_process_create_scan(SpoolmanSyncApp* app);

void app_process_find_scan(SpoolmanSyncApp* app);
