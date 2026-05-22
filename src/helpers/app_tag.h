#pragma once

#include "../services/nfc_reader.h"

#include <stdbool.h>

void app_tag_format_block_hex(const NfcScanResult* result, size_t block_index, char* output);

void app_tag_format_block9_hex(const NfcScanResult* result, char* output, size_t output_size);

bool app_tag_extract_article_number(
    const NfcScanResult* result,
    char* output,
    size_t output_size);

bool app_tag_extract_color_query(const NfcScanResult* result, char* output, size_t output_size);
