#pragma once

#include "../app_i.h"

typedef enum {
    AppSceneSpoolman,
    AppSceneModeSelect,
    AppSceneUpdate,
    AppSceneFind,
    AppSceneScan,
    AppSceneCreate,
    AppSceneCount,
} AppScene;

void app_scene_draw(Canvas* canvas, SpoolmanSyncApp* app);

bool app_scene_handle_input(SpoolmanSyncApp* app, const InputEvent* event);
