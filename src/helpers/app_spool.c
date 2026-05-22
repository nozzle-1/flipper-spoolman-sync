#include "app_spool.h"

bool app_spool_is_untagged(const SpoolmanSpool* spool) {
    return spool && furi_string_empty(spool->tag);
}

const SpoolmanSpool*
    app_spool_find_by_tag(const SpoolmanSpoolList* spools, const char* tag, int exclude_spool_id) {
    if(!spools || !tag) {
        return NULL;
    }

    for(size_t i = 0; i < spools->count; i++) {
        const SpoolmanSpool* spool = &spools->items[i];
        if(spool->id == exclude_spool_id || furi_string_empty(spool->tag)) {
            continue;
        }

        if(strcmp(furi_string_get_cstr(spool->tag), tag) == 0) {
            return spool;
        }
    }

    return NULL;
}

bool app_spool_get_remaining_weight(const SpoolmanSpool* spool, double* remaining_weight) {
    if(remaining_weight) {
        *remaining_weight = 0;
    }

    if(!spool || !spool->has_remaining_weight) {
        return false;
    }

    if(remaining_weight) {
        *remaining_weight = spool->remaining_weight > 0 ? spool->remaining_weight : 0;
    }

    return true;
}

const char*
    app_spool_get_display_name(const SpoolmanSpool* spool, const SpoolmanFilament* fallback_filament) {
    if(spool && !furi_string_empty(spool->filament.name)) {
        return furi_string_get_cstr(spool->filament.name);
    }

    if(fallback_filament && !furi_string_empty(fallback_filament->name)) {
        return furi_string_get_cstr(fallback_filament->name);
    }

    return NULL;
}

const char* app_spool_get_display_material(
    const SpoolmanSpool* spool,
    const SpoolmanFilament* fallback_filament) {
    if(spool && !furi_string_empty(spool->filament.material)) {
        return furi_string_get_cstr(spool->filament.material);
    }

    if(fallback_filament && !furi_string_empty(fallback_filament->material)) {
        return furi_string_get_cstr(fallback_filament->material);
    }

    return NULL;
}
