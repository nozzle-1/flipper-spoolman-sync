#pragma once

#include "app.h"

void app_page_spoolman_draw(Canvas* canvas, const SpoolmanSyncApp* app);

bool app_page_spoolman_handle_input(SpoolmanSyncApp* app, const InputEvent* event);

void app_page_mode_select_draw(Canvas* canvas, const SpoolmanSyncApp* app);

bool app_page_mode_select_handle_input(SpoolmanSyncApp* app, const InputEvent* event);

void app_page_update_draw(Canvas* canvas, const SpoolmanSyncApp* app);

bool app_page_update_handle_input(SpoolmanSyncApp* app, const InputEvent* event);

void app_page_scan_draw(Canvas* canvas, const SpoolmanSyncApp* app);

bool app_page_scan_handle_input(SpoolmanSyncApp* app, const InputEvent* event);

void app_page_create_draw(Canvas* canvas, const SpoolmanSyncApp* app);

bool app_page_create_handle_input(SpoolmanSyncApp* app, const InputEvent* event);
