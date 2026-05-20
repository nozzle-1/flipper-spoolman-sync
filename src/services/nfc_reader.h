#pragma once

#include <stdbool.h>
#include <stdint.h>

#define NFC_BLOCK_SIZE (16)

typedef struct NfcReader NfcReader;

typedef enum {
    SpoolmanNfcServiceEventReading,
    SpoolmanNfcServiceEventSuccess,
    SpoolmanNfcServiceEventError,
} SpoolmanNfcServiceEvent;

typedef struct {
    uint8_t block9[NFC_BLOCK_SIZE];
} NfcScanResult;

typedef void (
    *NfcReadCallback)(SpoolmanNfcServiceEvent event, const NfcScanResult* result, void* context);

NfcReader* nfc_reader_alloc(NfcReadCallback callback, void* context);

void nfc_reader_stop(NfcReader* service);

void nfc_reader_start(NfcReader* service);

bool nfc_reader_get_result(const NfcReader* service, NfcScanResult* result);
