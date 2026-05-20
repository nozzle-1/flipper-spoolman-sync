#pragma once

#include "app.h"

void app_pages_draw(Canvas* canvas, SpoolmanSyncApp* app);

bool app_pages_handle_input(SpoolmanSyncApp* app, const InputEvent* event);
