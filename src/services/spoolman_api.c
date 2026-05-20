#include "spoolman_api.h"

#include "flipper_http.h"

#include <furi.h>
#include <stdlib.h>
#include <string.h>

#define TAG "SpoolmanApi"

#define SPOOLMAN_API_PATH_HEALTH "/api/v1/health"
#define SPOOLMAN_API_PATH_SPOOLS "/api/v1/spool"
#define SPOOLMAN_API_HEADERS \
    "{\"Content-Type\":\"application/json\",\"Accept\":\"application/json\"}"
#define SPOOLMAN_API_TIMEOUT_MS (15000)

struct SpoolmanApi {
    FlipperHTTP* http;
    FuriString* base_url;
    FuriString* last_error;
    int status_code;
};

typedef struct {
    FuriString* response;
    bool in_body;
    FlipperHTTP_Callback previous_callback;
    void* previous_context;
} SpoolmanApiResponseCollector;

static void spoolman_vendor_init(SpoolmanVendor* vendor) {
    memset(vendor, 0, sizeof(SpoolmanVendor));
    vendor->name = furi_string_alloc();
    vendor->external_id = furi_string_alloc();
}

static void spoolman_vendor_clear(SpoolmanVendor* vendor) {
    if(vendor->name) {
        furi_string_free(vendor->name);
    }
    if(vendor->external_id) {
        furi_string_free(vendor->external_id);
    }
    memset(vendor, 0, sizeof(SpoolmanVendor));
}

static void spoolman_filament_init(SpoolmanFilament* filament) {
    memset(filament, 0, sizeof(SpoolmanFilament));
    filament->name = furi_string_alloc();
    spoolman_vendor_init(&filament->vendor);
    filament->material = furi_string_alloc();
    filament->color_hex = furi_string_alloc();
    filament->external_id = furi_string_alloc();
}

static void spoolman_filament_clear(SpoolmanFilament* filament) {
    if(filament->name) {
        furi_string_free(filament->name);
    }
    spoolman_vendor_clear(&filament->vendor);
    if(filament->material) {
        furi_string_free(filament->material);
    }
    if(filament->color_hex) {
        furi_string_free(filament->color_hex);
    }
    if(filament->external_id) {
        furi_string_free(filament->external_id);
    }
    memset(filament, 0, sizeof(SpoolmanFilament));
}

static void spoolman_spool_init(SpoolmanSpool* spool) {
    memset(spool, 0, sizeof(SpoolmanSpool));
    spool->registered = furi_string_alloc();
    spool->first_used = furi_string_alloc();
    spool->last_used = furi_string_alloc();
    spoolman_filament_init(&spool->filament);
    spool->tag = furi_string_alloc();
    spool->active_tray = furi_string_alloc();
}

static void spoolman_spool_clear(SpoolmanSpool* spool) {
    if(spool->registered) {
        furi_string_free(spool->registered);
    }
    if(spool->first_used) {
        furi_string_free(spool->first_used);
    }
    if(spool->last_used) {
        furi_string_free(spool->last_used);
    }
    spoolman_filament_clear(&spool->filament);
    if(spool->tag) {
        furi_string_free(spool->tag);
    }
    if(spool->active_tray) {
        furi_string_free(spool->active_tray);
    }
    memset(spool, 0, sizeof(SpoolmanSpool));
}

void spoolman_spool_list_init(SpoolmanSpoolList* list) {
    furi_assert(list);
    memset(list, 0, sizeof(SpoolmanSpoolList));
}

void spoolman_spool_list_clear(SpoolmanSpoolList* list) {
    if(!list) {
        return;
    }

    for(size_t i = 0; i < list->count; i++) {
        spoolman_spool_clear(&list->items[i]);
    }
    free(list->items);
    memset(list, 0, sizeof(SpoolmanSpoolList));
}

static bool spoolman_spool_list_push(SpoolmanSpoolList* list, const SpoolmanSpool* spool) {
    if(list->count == list->capacity) {
        size_t next_capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        SpoolmanSpool* next_items = realloc(list->items, next_capacity * sizeof(SpoolmanSpool));
        if(!next_items) {
            return false;
        }
        list->items = next_items;
        list->capacity = next_capacity;
    }

    list->items[list->count++] = *spool;
    return true;
}

static void spoolman_api_set_error(SpoolmanApi* api, const char* error) {
    furi_assert(api);
    furi_string_set(api->last_error, error);
}

static void spoolman_api_collect_response_line(const char* line, void* context) {
    SpoolmanApiResponseCollector* collector = context;
    if(!collector || !line) {
        return;
    }

    const char* get_success = strstr(line, "[GET/SUCCESS]");
    const char* get_end = strstr(line, "[GET/END]");

    if(get_success != NULL) {
        collector->in_body = true;

        const char* suffix = get_success + strlen("[GET/SUCCESS]");
        if(*suffix != '\0' && collector->response) {
            if(strncmp(suffix, "{\"Status-Code\":", strlen("{\"Status-Code\":")) == 0) {
                const char* header_end = strchr(suffix, '}');
                if(header_end && *(header_end + 1) != '\0') {
                    furi_string_cat_str(collector->response, header_end + 1);
                }
            } else {
                furi_string_cat_str(collector->response, suffix);
            }
        }
    } else if(get_end != NULL) {
        if(collector->in_body && collector->response && get_end > line) {
            furi_string_cat_printf(
                collector->response, "%.*s", (int)(get_end - line), line);
        }
        collector->in_body = false;
    } else if(collector->in_body && collector->response) {
        furi_string_cat_str(collector->response, line);
    }

    if(collector->previous_callback) {
        collector->previous_callback(line, collector->previous_context);
    }
}

static const char* spoolman_json_skip_ws(const char* cursor, const char* end) {
    while(cursor < end &&
          (*cursor == ' ' || *cursor == '\n' || *cursor == '\r' || *cursor == '\t')) {
        cursor++;
    }
    return cursor;
}

static const char* spoolman_json_skip_string(const char* cursor, const char* end) {
    if(cursor >= end || *cursor != '"') {
        return NULL;
    }

    cursor++;
    bool escaped = false;
    while(cursor < end) {
        if(escaped) {
            escaped = false;
        } else if(*cursor == '\\') {
            escaped = true;
        } else if(*cursor == '"') {
            return cursor + 1;
        }
        cursor++;
    }
    return NULL;
}

static const char* spoolman_json_skip_value(const char* cursor, const char* end) {
    cursor = spoolman_json_skip_ws(cursor, end);
    if(cursor >= end) {
        return NULL;
    }

    if(*cursor == '"') {
        return spoolman_json_skip_string(cursor, end);
    }
    if(*cursor != '{' && *cursor != '[') {
        while(cursor < end && *cursor != ',' && *cursor != '}' && *cursor != ']') {
            cursor++;
        }
        return cursor;
    }

    const char opening = *cursor;
    const char closing = opening == '{' ? '}' : ']';
    size_t depth = 1;
    cursor++;
    while(cursor < end) {
        if(*cursor == '"') {
            cursor = spoolman_json_skip_string(cursor, end);
            if(!cursor) {
                return NULL;
            }
            continue;
        }
        if(*cursor == opening) {
            depth++;
        } else if(*cursor == closing) {
            depth--;
            if(depth == 0) {
                return cursor + 1;
            }
        } else if((opening == '{' && *cursor == '[') || (opening == '[' && *cursor == '{')) {
            const char* nested_end = spoolman_json_skip_value(cursor, end);
            if(!nested_end) {
                return NULL;
            }
            cursor = nested_end;
            continue;
        }
        cursor++;
    }
    return NULL;
}

static bool
    spoolman_json_key_equals(const char* key_start, const char* key_end, const char* expected) {
    size_t expected_len = strlen(expected);
    return (size_t)(key_end - key_start) == expected_len &&
           strncmp(key_start, expected, expected_len) == 0;
}

static bool spoolman_json_object_get(
    const char* object,
    size_t object_len,
    const char* key,
    const char** value,
    size_t* value_len) {
    const char* end = object + object_len;
    const char* cursor = spoolman_json_skip_ws(object, end);
    if(cursor >= end || *cursor != '{') {
        return false;
    }
    cursor++;

    while(cursor < end) {
        cursor = spoolman_json_skip_ws(cursor, end);
        if(cursor >= end || *cursor == '}') {
            return false;
        }
        if(*cursor != '"') {
            return false;
        }

        const char* key_start = cursor + 1;
        const char* key_after = spoolman_json_skip_string(cursor, end);
        if(!key_after) {
            return false;
        }
        const char* key_end = key_after - 1;

        cursor = spoolman_json_skip_ws(key_after, end);
        if(cursor >= end || *cursor != ':') {
            return false;
        }
        cursor++;

        const char* found_value = spoolman_json_skip_ws(cursor, end);
        const char* found_value_end = spoolman_json_skip_value(found_value, end);
        if(!found_value_end) {
            return false;
        }

        if(spoolman_json_key_equals(key_start, key_end, key)) {
            *value = found_value;
            *value_len = (size_t)(found_value_end - found_value);
            return true;
        }

        cursor = spoolman_json_skip_ws(found_value_end, end);
        if(cursor < end && *cursor == ',') {
            cursor++;
        }
    }

    return false;
}

static void spoolman_json_get_string(
    const char* object,
    size_t object_len,
    const char* key,
    FuriString* output) {
    furi_string_reset(output);

    const char* value = NULL;
    size_t value_len = 0;
    if(!spoolman_json_object_get(object, object_len, key, &value, &value_len) || value_len < 2 ||
       value[0] != '"') {
        return;
    }

    const char* cursor = value + 1;
    const char* end = value + value_len - 1;
    while(cursor < end) {
        if(*cursor == '\\' && cursor + 1 < end) {
            cursor++;
            if(*cursor == 'n') {
                furi_string_push_back(output, '\n');
            } else if(*cursor == 'r') {
                furi_string_push_back(output, '\r');
            } else if(*cursor == 't') {
                furi_string_push_back(output, '\t');
            } else {
                furi_string_push_back(output, *cursor);
            }
        } else {
            furi_string_push_back(output, *cursor);
        }
        cursor++;
    }

    if(furi_string_size(output) >= 2 && furi_string_get_char(output, 0) == '"' &&
       furi_string_get_char(output, furi_string_size(output) - 1) == '"') {
        FuriString* stripped = furi_string_alloc();
        furi_string_set_n(stripped, output, 1, furi_string_size(output) - 2);
        furi_string_set(output, stripped);
        furi_string_free(stripped);
    }
}

static double spoolman_json_get_double(
    const char* object,
    size_t object_len,
    const char* key,
    double fallback) {
    const char* value = NULL;
    size_t value_len = 0;
    if(!spoolman_json_object_get(object, object_len, key, &value, &value_len)) {
        return fallback;
    }
    UNUSED(value_len);
    return strtod(value, NULL);
}

static int
    spoolman_json_get_int(const char* object, size_t object_len, const char* key, int fallback) {
    const char* value = NULL;
    size_t value_len = 0;
    if(!spoolman_json_object_get(object, object_len, key, &value, &value_len)) {
        return fallback;
    }
    UNUSED(value_len);
    return (int)strtol(value, NULL, 10);
}

static bool
    spoolman_json_get_bool(const char* object, size_t object_len, const char* key, bool fallback) {
    const char* value = NULL;
    size_t value_len = 0;
    if(!spoolman_json_object_get(object, object_len, key, &value, &value_len)) {
        return fallback;
    }
    return value_len >= 4 && strncmp(value, "true", 4) == 0;
}

static bool spoolman_parse_vendor(const char* object, size_t object_len, SpoolmanVendor* vendor) {
    vendor->id = spoolman_json_get_int(object, object_len, "id", 0);
    spoolman_json_get_string(object, object_len, "name", vendor->name);
    spoolman_json_get_string(object, object_len, "external_id", vendor->external_id);
    return true;
}

static bool
    spoolman_parse_filament(const char* object, size_t object_len, SpoolmanFilament* filament) {
    filament->id = spoolman_json_get_int(object, object_len, "id", 0);
    spoolman_json_get_string(object, object_len, "name", filament->name);
    spoolman_json_get_string(object, object_len, "material", filament->material);
    filament->density = spoolman_json_get_double(object, object_len, "density", 0);
    filament->diameter = spoolman_json_get_double(object, object_len, "diameter", 0);
    filament->weight = spoolman_json_get_double(object, object_len, "weight", 0);
    filament->spool_weight = spoolman_json_get_double(object, object_len, "spool_weight", 0);
    filament->settings_extruder_temp =
        spoolman_json_get_int(object, object_len, "settings_extruder_temp", 0);
    filament->settings_bed_temp =
        spoolman_json_get_int(object, object_len, "settings_bed_temp", 0);
    spoolman_json_get_string(object, object_len, "color_hex", filament->color_hex);
    spoolman_json_get_string(object, object_len, "external_id", filament->external_id);

    const char* vendor = NULL;
    size_t vendor_len = 0;
    if(spoolman_json_object_get(object, object_len, "vendor", &vendor, &vendor_len)) {
        spoolman_parse_vendor(vendor, vendor_len, &filament->vendor);
    }

    return true;
}

static bool spoolman_parse_spool(const char* object, size_t object_len, SpoolmanSpool* spool) {
    spool->id = spoolman_json_get_int(object, object_len, "id", 0);
    spoolman_json_get_string(object, object_len, "registered", spool->registered);
    spoolman_json_get_string(object, object_len, "first_used", spool->first_used);
    spoolman_json_get_string(object, object_len, "last_used", spool->last_used);
    spool->remaining_weight = spoolman_json_get_double(object, object_len, "remaining_weight", 0);
    spool->initial_weight = spoolman_json_get_double(object, object_len, "initial_weight", 0);
    spool->spool_weight = spoolman_json_get_double(object, object_len, "spool_weight", 0);
    spool->used_weight = spoolman_json_get_double(object, object_len, "used_weight", 0);
    spool->remaining_length = spoolman_json_get_double(object, object_len, "remaining_length", 0);
    spool->used_length = spoolman_json_get_double(object, object_len, "used_length", 0);
    spool->archived = spoolman_json_get_bool(object, object_len, "archived", false);

    const char* filament = NULL;
    size_t filament_len = 0;
    if(spoolman_json_object_get(object, object_len, "filament", &filament, &filament_len)) {
        spoolman_parse_filament(filament, filament_len, &spool->filament);
    }

    const char* extra = NULL;
    size_t extra_len = 0;
    if(spoolman_json_object_get(object, object_len, "extra", &extra, &extra_len)) {
        spoolman_json_get_string(extra, extra_len, "tag", spool->tag);
        spoolman_json_get_string(extra, extra_len, "active_tray", spool->active_tray);
    }

    return true;
}

static void spoolman_spool_copy_into(SpoolmanSpool* destination, const SpoolmanSpool* source) {
    furi_assert(destination);
    furi_assert(source);

    destination->id = source->id;
    furi_string_set(destination->registered, source->registered);
    furi_string_set(destination->first_used, source->first_used);
    furi_string_set(destination->last_used, source->last_used);

    destination->filament.id = source->filament.id;
    furi_string_set(destination->filament.name, source->filament.name);
    destination->filament.vendor.id = source->filament.vendor.id;
    furi_string_set(destination->filament.vendor.name, source->filament.vendor.name);
    furi_string_set(
        destination->filament.vendor.external_id, source->filament.vendor.external_id);
    furi_string_set(destination->filament.material, source->filament.material);
    destination->filament.density = source->filament.density;
    destination->filament.diameter = source->filament.diameter;
    destination->filament.weight = source->filament.weight;
    destination->filament.spool_weight = source->filament.spool_weight;
    destination->filament.settings_extruder_temp = source->filament.settings_extruder_temp;
    destination->filament.settings_bed_temp = source->filament.settings_bed_temp;
    furi_string_set(destination->filament.color_hex, source->filament.color_hex);
    furi_string_set(destination->filament.external_id, source->filament.external_id);

    destination->remaining_weight = source->remaining_weight;
    destination->initial_weight = source->initial_weight;
    destination->spool_weight = source->spool_weight;
    destination->used_weight = source->used_weight;
    destination->remaining_length = source->remaining_length;
    destination->used_length = source->used_length;
    destination->archived = source->archived;
    furi_string_set(destination->tag, source->tag);
    furi_string_set(destination->active_tray, source->active_tray);
}

static bool spoolman_parse_spool_list(const char* json, SpoolmanSpoolList* spools) {
    const char* end = json + strlen(json);
    const char* cursor = spoolman_json_skip_ws(json, end);
    if(cursor >= end || *cursor != '[') {
        return false;
    }
    cursor++;

    while(cursor < end) {
        cursor = spoolman_json_skip_ws(cursor, end);
        if(cursor < end && *cursor == ']') {
            return true;
        }
        if(cursor >= end || *cursor != '{') {
            return false;
        }

        const char* spool_end = spoolman_json_skip_value(cursor, end);
        if(!spool_end) {
            return false;
        }

        SpoolmanSpool spool;
        spoolman_spool_init(&spool);
        bool parsed = spoolman_parse_spool(cursor, (size_t)(spool_end - cursor), &spool);
        if(!parsed || !spoolman_spool_list_push(spools, &spool)) {
            spoolman_spool_clear(&spool);
            return false;
        }

        cursor = spoolman_json_skip_ws(spool_end, end);
        if(cursor < end && *cursor == ',') {
            cursor++;
        }
    }

    return false;
}

static bool spoolman_spool_has_tag(const char* object, size_t object_len) {
    const char* extra = NULL;
    size_t extra_len = 0;
    if(!spoolman_json_object_get(object, object_len, "extra", &extra, &extra_len)) {
        return false;
    }

    const char* tag = NULL;
    size_t tag_len = 0;
    if(!spoolman_json_object_get(extra, extra_len, "tag", &tag, &tag_len)) {
        return false;
    }

    return tag_len > 2 && tag[0] == '"' && tag[tag_len - 1] == '"';
}

static bool
    spoolman_count_spool_list(const char* json, size_t* untagged_count, size_t* total_count) {
    if(!untagged_count || !total_count) {
        return false;
    }

    const char* end = json + strlen(json);
    const char* cursor = spoolman_json_skip_ws(json, end);
    if(cursor >= end || *cursor != '[') {
        return false;
    }
    cursor++;

    size_t parsed_untagged_count = 0;
    size_t parsed_total_count = 0;
    while(cursor < end) {
        cursor = spoolman_json_skip_ws(cursor, end);
        if(cursor < end && *cursor == ']') {
            *untagged_count = parsed_untagged_count;
            *total_count = parsed_total_count;
            return true;
        }
        if(cursor >= end || *cursor != '{') {
            return false;
        }

        const char* spool_end = spoolman_json_skip_value(cursor, end);
        if(!spool_end) {
            return false;
        }

        parsed_total_count++;
        if(!spoolman_spool_has_tag(cursor, (size_t)(spool_end - cursor))) {
            parsed_untagged_count++;
        }
        cursor = spoolman_json_skip_ws(spool_end, end);
        if(cursor < end && *cursor == ',') {
            cursor++;
        }
    }

    return false;
}

static void spoolman_api_build_url(SpoolmanApi* api, FuriString* url, const char* path) {
    furi_assert(api);
    furi_assert(url);
    furi_assert(path);

    furi_string_set(url, furi_string_get_cstr(api->base_url));
    if(furi_string_end_with(url, "/")) {
        furi_string_left(url, furi_string_size(url) - 1);
    }
    furi_string_cat_str(url, path);
}

static SpoolmanApiResult spoolman_api_request(
    SpoolmanApi* api,
    HTTPMethod method,
    const char* path,
    const char* payload) {
    if(!api || !path) {
        return SpoolmanApiResultInvalidArguments;
    }
    if(!api->http) {
        return SpoolmanApiResultHttpUnavailable;
    }

    furi_string_reset(api->last_error);
    api->status_code = 0;
    api->http->status_code = 0;
    api->http->last_response[0] = '\0';

    FuriString* url = furi_string_alloc();
    spoolman_api_build_url(api, url, path);

    const char* method_name = "REQ";
    if(method == PUT) {
        method_name = "PUT";
    } else if(method == PATCH) {
        method_name = "PATCH";
    } else if(method == POST) {
        method_name = "POST";
    } else if(method == DELETE) {
        method_name = "DELETE";
    }

    FURI_LOG_I(
        TAG,
        "%s %s payload=%s",
        method_name,
        furi_string_get_cstr(url),
        payload ? payload : "(null)");
    bool request_sent = flipper_http_request(
        api->http, method, furi_string_get_cstr(url), SPOOLMAN_API_HEADERS, payload);
    furi_string_free(url);

    if(!request_sent) {
        spoolman_api_set_error(
            api, method == PUT ? "Failed to send PUT request" : "Request failed");
        return SpoolmanApiResultRequestFailed;
    }
    api->http->state = RECEIVING;

    uint32_t elapsed_ms = 0;
    while(api->http->state != IDLE && api->http->state != ISSUE &&
          elapsed_ms < SPOOLMAN_API_TIMEOUT_MS) {
        furi_delay_ms(100);
        elapsed_ms += 100;
    }

    api->status_code = api->http->status_code;
    FURI_LOG_I(
        TAG,
        "%s %s finished: state=%d status=%d last='%s'",
        method_name,
        path,
        api->http->state,
        api->status_code,
        api->http->last_response ? api->http->last_response : "");
    if(api->http->state == ISSUE) {
        spoolman_api_set_error(api, "FlipperHTTP reported an issue");
        return SpoolmanApiResultRequestFailed;
    }
    if(api->http->state != IDLE) {
        spoolman_api_set_error(
            api,
            method == PUT ? "PUT request timed out" :
                            method == PATCH ? "PATCH request timed out" : "Request timed out");
        return SpoolmanApiResultTimeout;
    }
    if(api->status_code < 200 || api->status_code >= 300) {
        if(api->http->last_response && api->http->last_response[0] != '\0') {
            spoolman_api_set_error(api, api->http->last_response);
        } else {
            spoolman_api_set_error(api, "Spoolman returned a non-2xx status");
        }
        return SpoolmanApiResultHttpError;
    }

    return SpoolmanApiResultOk;
}

static SpoolmanApiResult spoolman_api_get_path(
    SpoolmanApi* api,
    const char* path,
    FuriString* response,
    bool send_headers) {
    if(!api || !path) {
        return SpoolmanApiResultInvalidArguments;
    }
    if(!api->http) {
        return SpoolmanApiResultHttpUnavailable;
    }

    if(response) {
        furi_string_reset(response);
    }
    furi_string_reset(api->last_error);
    api->status_code = 0;
    api->http->status_code = 0;
    api->http->last_response[0] = '\0';

    SpoolmanApiResponseCollector collector = {
        .response = response,
        .in_body = false,
        .previous_callback = api->http->user_rx_line_cb,
        .previous_context = api->http->user_callback_context,
    };
    api->http->user_rx_line_cb = spoolman_api_collect_response_line;
    api->http->user_callback_context = &collector;

    FuriString* url = furi_string_alloc();
    spoolman_api_build_url(api, url, path);

    FURI_LOG_I(TAG, "GET %s", furi_string_get_cstr(url));
    bool request_sent = flipper_http_request(
        api->http,
        GET,
        furi_string_get_cstr(url),
        send_headers ? SPOOLMAN_API_HEADERS : NULL,
        NULL);
    furi_string_free(url);

    if(!request_sent) {
        api->http->user_rx_line_cb = collector.previous_callback;
        api->http->user_callback_context = collector.previous_context;
        spoolman_api_set_error(api, "Failed to send GET request");
        return SpoolmanApiResultRequestFailed;
    }
    api->http->state = RECEIVING;

    uint32_t elapsed_ms = 0;
    while(api->http->state != IDLE && api->http->state != ISSUE &&
          elapsed_ms < SPOOLMAN_API_TIMEOUT_MS) {
        furi_delay_ms(100);
        elapsed_ms += 100;
    }

    api->status_code = api->http->status_code;
    FURI_LOG_I(
        TAG,
        "GET %s finished: state=%d status=%d last='%s'",
        path,
        api->http->state,
        api->status_code,
        api->http->last_response ? api->http->last_response : "");

    api->http->user_rx_line_cb = collector.previous_callback;
    api->http->user_callback_context = collector.previous_context;

    if(api->http->state == ISSUE) {
        spoolman_api_set_error(api, "FlipperHTTP reported an issue");
        return SpoolmanApiResultRequestFailed;
    }
    if(api->http->state != IDLE) {
        spoolman_api_set_error(api, "GET request timed out");
        return SpoolmanApiResultTimeout;
    }
    if(api->status_code < 200 || api->status_code >= 300) {
        spoolman_api_set_error(api, "Spoolman returned a non-2xx status");
        return SpoolmanApiResultHttpError;
    }

    if(response && furi_string_empty(response)) {
        furi_string_set(response, api->http->last_response);
    }
    return SpoolmanApiResultOk;
}

SpoolmanApi* spoolman_api_alloc(const char* base_url) {
    if(!base_url || strlen(base_url) == 0) {
        return NULL;
    }

    SpoolmanApi* api = malloc(sizeof(SpoolmanApi));
    memset(api, 0, sizeof(SpoolmanApi));

    api->base_url = furi_string_alloc_set(base_url);
    api->last_error = furi_string_alloc();
    api->http = flipper_http_alloc();

    if(!api->http) {
        spoolman_api_free(api);
        return NULL;
    }

    return api;
}

void spoolman_api_free(SpoolmanApi* api) {
    if(!api) {
        return;
    }

    if(api->http) {
        flipper_http_free(api->http);
    }
    if(api->base_url) {
        furi_string_free(api->base_url);
    }
    if(api->last_error) {
        furi_string_free(api->last_error);
    }
    free(api);
}

SpoolmanApiResult spoolman_api_health_check(SpoolmanApi* api) {
    SpoolmanApiResult result = spoolman_api_get_path(api, SPOOLMAN_API_PATH_HEALTH, NULL, false);
    if(result == SpoolmanApiResultOk && api->status_code != 200) {
        spoolman_api_set_error(api, "Spoolman /health did not return 200");
        return SpoolmanApiResultHttpError;
    }

    return result;
}

SpoolmanApiResult spoolman_api_get_spool_count(SpoolmanApi* api, size_t* count) {
    size_t total_count = 0;
    return spoolman_api_get_spool_counts(api, count, &total_count);
}

SpoolmanApiResult
    spoolman_api_get_spool_counts(SpoolmanApi* api, size_t* untagged_count, size_t* total_count) {
    if(!untagged_count || !total_count) {
        return SpoolmanApiResultInvalidArguments;
    }

    *untagged_count = 0;
    *total_count = 0;

    FuriString* response = furi_string_alloc();
    SpoolmanApiResult result =
        spoolman_api_get_path(api, SPOOLMAN_API_PATH_SPOOLS, response, true);
    if(result == SpoolmanApiResultOk &&
       !spoolman_count_spool_list(furi_string_get_cstr(response), untagged_count, total_count)) {
        spoolman_api_set_error(api, "Invalid spool response");
        result = SpoolmanApiResultParseError;
    }
    furi_string_free(response);

    return result;
}

SpoolmanApiResult spoolman_api_get_spools(SpoolmanApi* api, SpoolmanSpoolList* spools) {
    if(!spools) {
        return SpoolmanApiResultInvalidArguments;
    }

    spoolman_spool_list_clear(spools);

    FuriString* response = furi_string_alloc();
    SpoolmanApiResult result =
        spoolman_api_get_path(api, SPOOLMAN_API_PATH_SPOOLS, response, true);
    if(result == SpoolmanApiResultOk &&
       !spoolman_parse_spool_list(furi_string_get_cstr(response), spools)) {
        spoolman_spool_list_clear(spools);
        spoolman_api_set_error(api, "Invalid spool response");
        result = SpoolmanApiResultParseError;
    }
    furi_string_free(response);

    return result;
}

void spoolman_api_fill_missing_spool_details(SpoolmanApi* api, SpoolmanSpoolList* spools) {
    if(!api || !spools) {
        return;
    }

    for(size_t i = 0; i < spools->count; i++) {
        SpoolmanSpool* spool = &spools->items[i];
        if(!furi_string_empty(spool->filament.name) && !furi_string_empty(spool->filament.material)) {
            continue;
        }

        char path[48];
        snprintf(path, sizeof(path), "%s/%d", SPOOLMAN_API_PATH_SPOOLS, spool->id);

        FuriString* response = furi_string_alloc();
        SpoolmanApiResult result = spoolman_api_get_path(api, path, response, true);
        if(result == SpoolmanApiResultOk) {
            SpoolmanSpool detailed_spool;
            spoolman_spool_init(&detailed_spool);

            bool parsed = spoolman_parse_spool(
                furi_string_get_cstr(response), furi_string_size(response), &detailed_spool);
            if(parsed) {
                spoolman_spool_copy_into(spool, &detailed_spool);
            } else {
                FURI_LOG_W(TAG, "Failed to parse details for spool #%d", spool->id);
            }

            spoolman_spool_clear(&detailed_spool);
        } else {
            FURI_LOG_W(TAG, "Failed to fetch details for spool #%d: %d", spool->id, result);
        }

        furi_string_free(response);
    }
}

SpoolmanApiResult spoolman_api_update_spool_tag(SpoolmanApi* api, int spool_id, const char* tag) {
    if(!api || !tag || spool_id <= 0) {
        return SpoolmanApiResultInvalidArguments;
    }

    char path[48];
    char payload[132];

    snprintf(path, sizeof(path), "%s/%d", SPOOLMAN_API_PATH_SPOOLS, spool_id);
    snprintf(payload, sizeof(payload), "{\"extra\":{\"tag\":\"\\\"%s\\\"\"}}", tag);

    return spoolman_api_request(api, PATCH, path, payload);
}

int spoolman_api_get_status_code(const SpoolmanApi* api) {
    furi_assert(api);
    return api->status_code;
}

const char* spoolman_api_get_last_error(const SpoolmanApi* api) {
    furi_assert(api);
    return furi_string_get_cstr(api->last_error);
}
