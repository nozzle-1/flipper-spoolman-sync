#pragma once

#include <furi.h>

typedef struct SpoolmanApi SpoolmanApi;

typedef struct {
    int id;
    FuriString* name;
    FuriString* external_id;
} SpoolmanVendor;

typedef struct {
    int id;
    FuriString* name;
    SpoolmanVendor vendor;
    FuriString* material;
    double density;
    double diameter;
    double weight;
    double spool_weight;
    int settings_extruder_temp;
    int settings_bed_temp;
    FuriString* color_hex;
    FuriString* external_id;
} SpoolmanFilament;

typedef struct {
    int id;
    FuriString* registered;
    FuriString* first_used;
    FuriString* last_used;
    SpoolmanFilament filament;
    double remaining_weight;
    double initial_weight;
    double spool_weight;
    double used_weight;
    double remaining_length;
    double used_length;
    bool archived;
    FuriString* tag;
    FuriString* active_tray;
} SpoolmanSpool;

typedef struct {
    SpoolmanSpool* items;
    size_t count;
    size_t capacity;
} SpoolmanSpoolList;

typedef enum {
    SpoolmanApiResultOk,
    SpoolmanApiResultInvalidArguments,
    SpoolmanApiResultHttpUnavailable,
    SpoolmanApiResultRequestFailed,
    SpoolmanApiResultTimeout,
    SpoolmanApiResultHttpError,
    SpoolmanApiResultParseError,
} SpoolmanApiResult;

SpoolmanApi* spoolman_api_alloc(const char* base_url);

void spoolman_api_free(SpoolmanApi* api);

SpoolmanApiResult spoolman_api_health_check(SpoolmanApi* api);

void spoolman_spool_list_init(SpoolmanSpoolList* list);

void spoolman_spool_list_clear(SpoolmanSpoolList* list);

SpoolmanApiResult spoolman_api_get_spool_count(SpoolmanApi* api, size_t* count);

SpoolmanApiResult
    spoolman_api_get_spool_counts(SpoolmanApi* api, size_t* untagged_count, size_t* total_count);

SpoolmanApiResult spoolman_api_get_spools(SpoolmanApi* api, SpoolmanSpoolList* spools);

void spoolman_api_fill_missing_spool_details(SpoolmanApi* api, SpoolmanSpoolList* spools);

SpoolmanApiResult spoolman_api_update_spool_tag(SpoolmanApi* api, int spool_id, const char* tag);

int spoolman_api_get_status_code(const SpoolmanApi* api);

const char* spoolman_api_get_last_error(const SpoolmanApi* api);
