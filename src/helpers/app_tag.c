#include "app_tag.h"

#include <furi.h>
#include <stdio.h>
#include <string.h>

static void app_tag_format_bytes_hex(
    const uint8_t* bytes,
    size_t bytes_len,
    char* output,
    size_t output_size) {
    furi_assert(bytes);
    furi_assert(output);
    furi_assert(output_size >= (bytes_len * 2) + 1);

    for(size_t i = 0; i < bytes_len; i++) {
        snprintf(&output[i * 2], 3, "%02X", bytes[i]);
    }
}

void app_tag_format_block_hex(const NfcScanResult* result, size_t block_index, char* output) {
    furi_assert(result);
    furi_assert(block_index < NFC_BLOCK_COUNT);
    app_tag_format_bytes_hex(
        result->blocks[block_index], NFC_BLOCK_SIZE, output, (NFC_BLOCK_SIZE * 2) + 1);
}

void app_tag_format_block9_hex(const NfcScanResult* result, char* output, size_t output_size) {
    furi_assert(result);
    furi_assert(output);
    furi_assert(output_size >= (NFC_BLOCK_SIZE * 2) + 1);
    app_tag_format_block_hex(result, 9, output);
}

static void app_tag_copy_ascii_fragment(
    char* output,
    size_t output_size,
    const uint8_t* bytes,
    size_t bytes_len) {
    size_t copy_len = bytes_len < (output_size - 1) ? bytes_len : (output_size - 1);
    for(size_t i = 0; i < copy_len; i++) {
        char c = (char)bytes[i];
        if(c < 32 || c > 126) {
            break;
        }
        output[i] = c;
        output[i + 1] = '\0';
    }
}

bool app_tag_extract_article_number(
    const NfcScanResult* result,
    char* output,
    size_t output_size) {
    if(!result || !output || output_size < 6) {
        return false;
    }

    output[0] = '\0';

    char material_id[17] = "";
    app_tag_copy_ascii_fragment(material_id, sizeof(material_id), &result->blocks[1][8], 8);
    if(material_id[0] == 'G' && material_id[1] == 'F' && strlen(material_id) >= 5) {
        snprintf(output, output_size, "%.5s", material_id);
        return true;
    }

    char block_1[17] = "";
    app_tag_copy_ascii_fragment(block_1, sizeof(block_1), result->blocks[1], 16);
    for(size_t i = 0; block_1[i] != '\0'; i++) {
        if(block_1[i] == 'G' && block_1[i + 1] == 'F') {
            size_t remaining = strlen(&block_1[i]);
            if(remaining >= 5) {
                snprintf(output, output_size, "%.5s", &block_1[i]);
                return true;
            }
        }
    }

    return false;
}

static bool app_tag_is_zero_hex_color(const char* color_hex) {
    return color_hex && strcmp(color_hex, "00000000") == 0;
}

bool app_tag_extract_color_query(const NfcScanResult* result, char* output, size_t output_size) {
    if(!result || !output || output_size < 9) {
        return false;
    }

    char primary[9] = "";
    char secondary[9] = "";
    app_tag_format_bytes_hex(result->blocks[5], 4, primary, sizeof(primary));
    app_tag_format_bytes_hex(&result->blocks[16][4], 4, secondary, sizeof(secondary));

    if(primary[0] == '\0') {
        return false;
    }

    if(secondary[0] != '\0' && !app_tag_is_zero_hex_color(secondary) &&
       strcmp(primary, secondary) != 0) {
        snprintf(output, output_size, "%s;%s", primary, secondary);
    } else {
        snprintf(output, output_size, "%s", primary);
    }

    return true;
}
