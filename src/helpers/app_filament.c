#include "app_filament.h"

#include "app_tag.h"

bool app_filament_extract_scan_lookup(
    const NfcScanResult* scan_result,
    char* article_number,
    size_t article_number_size,
    char* color_query,
    size_t color_query_size,
    char* tag_hex,
    size_t tag_hex_size) {
    if(!scan_result || !article_number || !color_query || !tag_hex) {
        return false;
    }

    bool has_article_number =
        app_tag_extract_article_number(scan_result, article_number, article_number_size);
    bool has_color_query =
        app_tag_extract_color_query(scan_result, color_query, color_query_size);
    app_tag_format_block9_hex(scan_result, tag_hex, tag_hex_size);

    return has_article_number && has_color_query;
}

SpoolmanApiResult app_filament_resolve_from_scan(
    SpoolmanApi* spoolman_api,
    const NfcScanResult* scan_result,
    char* tag_hex,
    size_t tag_hex_size,
    SpoolmanFilament* filament) {
    if(!spoolman_api || !scan_result || !tag_hex || !filament) {
        return !spoolman_api ? SpoolmanApiResultHttpUnavailable : SpoolmanApiResultInvalidArguments;
    }

    char article_number[8] = "";
    char color_query[18] = "";

    if(!app_filament_extract_scan_lookup(
           scan_result,
           article_number,
           sizeof(article_number),
           color_query,
           sizeof(color_query),
           tag_hex,
           tag_hex_size)) {
        return SpoolmanApiResultInvalidArguments;
    }

    return spoolman_api_find_bambu_filament(
        spoolman_api, article_number, color_query, filament);
}
