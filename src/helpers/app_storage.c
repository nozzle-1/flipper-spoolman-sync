#include "app_storage.h"

#include <storage/storage.h>
#include <string.h>

#define APP_STORAGE_SPOOLMAN_URL_CONFIG_PATH APP_DATA_PATH("spoolman_url.txt")
#define APP_STORAGE_SPOOLMAN_URL_MAX_LEN 255
#define TAG "AppStorage"

static bool app_storage_is_ascii_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

void app_storage_trim_ascii_whitespace(char* value) {
    if(!value) {
        return;
    }

    size_t start = 0;
    size_t len = strlen(value);
    while(start < len && app_storage_is_ascii_whitespace(value[start])) {
        start++;
    }

    size_t end = len;
    while(end > start && app_storage_is_ascii_whitespace(value[end - 1])) {
        end--;
    }

    size_t trimmed_len = end - start;
    if(start > 0 && trimmed_len > 0) {
        memmove(value, value + start, trimmed_len);
    }
    value[trimmed_len] = '\0';
}

bool app_storage_write_spoolman_url(const char* base_url) {
    if(!base_url || base_url[0] == '\0') {
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool saved = false;

    if(storage_file_open(
           file,
           APP_STORAGE_SPOOLMAN_URL_CONFIG_PATH,
           FSAM_WRITE,
           FSOM_CREATE_ALWAYS)) {
        size_t expected_size = strlen(base_url);
        size_t written_size = storage_file_write(file, base_url, expected_size);
        if(written_size != expected_size) {
            FURI_LOG_W(TAG, "Failed to write Spoolman URL config");
        } else {
            saved = true;
        }
        storage_file_close(file);
    } else {
        FURI_LOG_W(
            TAG,
            "Failed to create config file %s",
            APP_STORAGE_SPOOLMAN_URL_CONFIG_PATH);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return saved;
}

FuriString* app_storage_load_spoolman_url(void) {
    FuriString* base_url = furi_string_alloc();
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(!storage_file_open(
           file,
           APP_STORAGE_SPOOLMAN_URL_CONFIG_PATH,
           FSAM_READ,
           FSOM_OPEN_EXISTING)) {
        FURI_LOG_I(TAG, "Config not found at %s", APP_STORAGE_SPOOLMAN_URL_CONFIG_PATH);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return base_url;
    }

    size_t file_size = storage_file_size(file);
    if(file_size > APP_STORAGE_SPOOLMAN_URL_MAX_LEN) {
        file_size = APP_STORAGE_SPOOLMAN_URL_MAX_LEN;
    }

    char url_buffer[APP_STORAGE_SPOOLMAN_URL_MAX_LEN + 1];
    memset(url_buffer, 0, sizeof(url_buffer));
    size_t read_size = storage_file_read(file, url_buffer, file_size);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    url_buffer[read_size] = '\0';
    app_storage_trim_ascii_whitespace(url_buffer);

    if(read_size == 0 || url_buffer[0] == '\0') {
        FURI_LOG_W(TAG, "Config file is empty");
        return base_url;
    }

    furi_string_set_str(base_url, url_buffer);
    FURI_LOG_I(TAG, "Loaded Spoolman URL: %s", furi_string_get_cstr(base_url));
    return base_url;
}
