#pragma once

#include "services/nfc_reader.h"
#include "services/spoolman_api.h"

#include <stdbool.h>

bool app_filament_extract_scan_lookup(
    const NfcScanResult* scan_result,
    char* article_number,
    size_t article_number_size,
    char* color_query,
    size_t color_query_size,
    char* tag_hex,
    size_t tag_hex_size);

SpoolmanApiResult app_filament_resolve_from_scan(
    SpoolmanApi* spoolman_api,
    const NfcScanResult* scan_result,
    char* tag_hex,
    size_t tag_hex_size,
    SpoolmanFilament* filament);
