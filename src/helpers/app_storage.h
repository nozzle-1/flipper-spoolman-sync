#pragma once

#include <stdbool.h>

#include <furi.h>

bool app_storage_write_spoolman_url(const char* base_url);

FuriString* app_storage_load_spoolman_url(void);

void app_storage_trim_ascii_whitespace(char* value);
