#pragma once

#include "services/spoolman_api.h"

#include <stdbool.h>

bool app_spool_is_untagged(const SpoolmanSpool* spool);

const SpoolmanSpool*
    app_spool_find_by_tag(const SpoolmanSpoolList* spools, const char* tag, int exclude_spool_id);

bool app_spool_get_remaining_weight(const SpoolmanSpool* spool, double* remaining_weight);

const char*
    app_spool_get_display_name(const SpoolmanSpool* spool, const SpoolmanFilament* fallback_filament);

const char* app_spool_get_display_material(
    const SpoolmanSpool* spool,
    const SpoolmanFilament* fallback_filament);
