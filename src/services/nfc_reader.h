#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NFC_BLOCK_SIZE (16)
#define NFC_MAX_UID_SIZE (10)
#define NFC_SECTOR_COUNT (16)
#define NFC_BLOCK_COUNT (NFC_SECTOR_COUNT * 4)

typedef struct NfcReader NfcReader;

typedef enum {
    SpoolmanNfcServiceEventReading,
    SpoolmanNfcServiceEventSuccess,
    SpoolmanNfcServiceEventError,
} SpoolmanNfcServiceEvent;

typedef struct {
    size_t uid_len;
    uint8_t uid[NFC_MAX_UID_SIZE];
    bool block_read[NFC_BLOCK_COUNT];
    uint8_t blocks[NFC_BLOCK_COUNT][NFC_BLOCK_SIZE];
} NfcScanResult;

typedef void (
    *NfcReadCallback)(SpoolmanNfcServiceEvent event, const NfcScanResult* result, void* context);

NfcReader* nfc_reader_alloc(NfcReadCallback callback, void* context);

void nfc_reader_stop(NfcReader* service);

void nfc_reader_start(NfcReader* service);

bool nfc_reader_get_result(const NfcReader* service, NfcScanResult* result);
