#pragma once

#include "app_scene.h"

typedef void (*AppSceneDrawCallback)(Canvas* canvas, const SpoolmanSyncApp* app);
typedef bool (*AppSceneInputCallback)(SpoolmanSyncApp* app, const InputEvent* event);

void app_scene_spoolman_draw(Canvas* canvas, const SpoolmanSyncApp* app);
bool app_scene_spoolman_handle_input(SpoolmanSyncApp* app, const InputEvent* event);

void app_scene_mode_select_draw(Canvas* canvas, const SpoolmanSyncApp* app);
bool app_scene_mode_select_handle_input(SpoolmanSyncApp* app, const InputEvent* event);

void app_scene_update_draw(Canvas* canvas, const SpoolmanSyncApp* app);
bool app_scene_update_handle_input(SpoolmanSyncApp* app, const InputEvent* event);

void app_scene_find_draw(Canvas* canvas, const SpoolmanSyncApp* app);
bool app_scene_find_handle_input(SpoolmanSyncApp* app, const InputEvent* event);

void app_scene_scan_draw(Canvas* canvas, const SpoolmanSyncApp* app);
bool app_scene_scan_handle_input(SpoolmanSyncApp* app, const InputEvent* event);

void app_scene_create_draw(Canvas* canvas, const SpoolmanSyncApp* app);
bool app_scene_create_handle_input(SpoolmanSyncApp* app, const InputEvent* event);
