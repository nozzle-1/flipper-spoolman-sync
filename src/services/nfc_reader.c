#include "nfc_reader.h"

#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/protocols/mf_classic/mf_classic_poller.h>

static const char* TAG = "NfcReader";

struct NfcReader {
    Nfc* nfc;
    NfcPoller* poller;
    MfClassicKey sector_keys[NFC_SECTOR_COUNT];
    uint8_t next_sector_to_read;
    bool has_result;
    NfcScanResult result;
    NfcReadCallback callback;
    void* context;
};

static void nfc_reader_stop_poller(NfcReader* service);

static uint32_t sha256_rotr(uint32_t value, uint8_t shift) {
    return (value >> shift) | (value << (32 - shift));
}

static uint32_t sha256_load_be(const uint8_t* data) {
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) |
           data[3];
}

static void sha256_store_be(uint32_t value, uint8_t* data) {
    data[0] = value >> 24;
    data[1] = value >> 16;
    data[2] = value >> 8;
    data[3] = value;
}

static void sha256_transform(uint32_t state[8], const uint8_t block[64]) {
    static const uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
        0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
        0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
        0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
        0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
        0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
        0xc67178f2};
    uint32_t w[64];

    for(size_t i = 0; i < 16; i++) {
        w[i] = sha256_load_be(&block[i * 4]);
    }
    for(size_t i = 16; i < 64; i++) {
        uint32_t s0 = sha256_rotr(w[i - 15], 7) ^ sha256_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = sha256_rotr(w[i - 2], 17) ^ sha256_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t f = state[5];
    uint32_t g = state[6];
    uint32_t h = state[7];

    for(size_t i = 0; i < 64; i++) {
        uint32_t s1 = sha256_rotr(e, 6) ^ sha256_rotr(e, 11) ^ sha256_rotr(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t temp1 = h + s1 + ch + k[i] + w[i];
        uint32_t s0 = sha256_rotr(a, 2) ^ sha256_rotr(a, 13) ^ sha256_rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

static void sha256(const uint8_t* data, size_t data_len, uint8_t out[32]) {
    uint32_t state[8] = {
        0x6a09e667,
        0xbb67ae85,
        0x3c6ef372,
        0xa54ff53a,
        0x510e527f,
        0x9b05688c,
        0x1f83d9ab,
        0x5be0cd19};
    size_t offset = 0;

    while(data_len - offset >= 64) {
        sha256_transform(state, &data[offset]);
        offset += 64;
    }

    uint8_t block[128] = {};
    size_t remaining = data_len - offset;
    memcpy(block, &data[offset], remaining);
    block[remaining] = 0x80;

    uint64_t bit_len = data_len * 8;
    size_t final_len = (remaining >= 56) ? 128 : 64;
    for(size_t i = 0; i < 8; i++) {
        block[final_len - 1 - i] = bit_len >> (i * 8);
    }

    sha256_transform(state, block);
    if(final_len == 128) {
        sha256_transform(state, &block[64]);
    }

    for(size_t i = 0; i < 8; i++) {
        sha256_store_be(state[i], &out[i * 4]);
    }
}

static void hmac_sha256(
    const uint8_t* key,
    size_t key_len,
    const uint8_t* data,
    size_t data_len,
    uint8_t* out) {
    furi_assert(key_len <= 64);
    furi_assert(data_len <= 65);

    uint8_t inner[64 + 65] = {};
    uint8_t outer[64 + 32] = {};
    uint8_t inner_hash[32];

    for(size_t i = 0; i < 64; i++) {
        uint8_t key_byte = (i < key_len) ? key[i] : 0;
        inner[i] = key_byte ^ 0x36;
        outer[i] = key_byte ^ 0x5c;
    }

    memcpy(&inner[64], data, data_len);
    sha256(inner, 64 + data_len, inner_hash);

    memcpy(&outer[64], inner_hash, sizeof(inner_hash));
    sha256(outer, sizeof(outer), out);
}

static void hkdf_expand(
    const uint8_t* prk,
    size_t prk_len,
    const uint8_t* info,
    size_t info_len,
    uint8_t* out,
    size_t out_len) {
    furi_assert(prk);
    furi_assert(prk_len <= 64);
    furi_assert(info);
    furi_assert(out);
    furi_assert(out_len > 0);
    furi_assert(info_len <= 32);

    uint8_t previous[32];
    size_t previous_len = 0;
    uint8_t block_input[32 + 32 + 1];
    uint8_t counter = 1;
    size_t offset = 0;

    while(offset < out_len) {
        memcpy(block_input, previous, previous_len);
        memcpy(&block_input[previous_len], info, info_len);
        block_input[previous_len + info_len] = counter;

        hmac_sha256(prk, prk_len, block_input, previous_len + info_len + 1, previous);

        size_t copy_len = MIN(sizeof(previous), out_len - offset);
        memcpy(&out[offset], previous, copy_len);
        offset += copy_len;
        previous_len = sizeof(previous);
        counter++;
    }
}

static void derive_bambu_keys(const uint8_t* uid, size_t uid_len, MfClassicKey* key_out) {
    const uint8_t salt[16] = {
        0x9A,
        0x75,
        0x9C,
        0xF2,
        0xC4,
        0xF7,
        0xCA,
        0xFF,
        0x22,
        0x2C,
        0xB9,
        0x76,
        0x9B,
        0x41,
        0xBC,
        0x96};
    uint8_t prk[32];
    hmac_sha256(salt, 16, uid, uid_len, prk);

    const uint8_t info[] = {'R', 'F', 'I', 'D', '-', 'A', '\0'};
    uint8_t okm[NFC_SECTOR_COUNT * sizeof(MfClassicKey)];
    hkdf_expand(prk, sizeof(prk), info, sizeof(info), okm, sizeof(okm));

    for(size_t i = 0; i < NFC_SECTOR_COUNT; i++) {
        memcpy(key_out[i].data, &okm[i * sizeof(MfClassicKey)], sizeof(key_out[i].data));
    }
}

static void emit(NfcReader* service, SpoolmanNfcServiceEvent event, NfcScanResult* result) {
    if(service->callback) {
        service->callback(event, result, service->context);
    }
}

static NfcCommand callback(NfcGenericEvent event, void* context) {
    NfcReader* service = context;

    if(event.protocol != NfcProtocolMfClassic) {
        return NfcCommandContinue;
    }

    MfClassicPollerEvent* mfc_event = event.event_data;

    if(mfc_event->type == MfClassicPollerEventTypeRequestMode) {
        service->next_sector_to_read = 0;
        service->has_result = false;
        memset(&service->result, 0, sizeof(service->result));
        emit(service, SpoolmanNfcServiceEventReading, NULL);

        const MfClassicData* mfc_data = nfc_poller_get_data(service->poller);
        size_t uid_len = 0;
        const uint8_t* uid = mf_classic_get_uid(mfc_data, &uid_len);
        FURI_LOG_I(TAG, "Tag detected, uid_len=%u", (unsigned)uid_len);
        derive_bambu_keys(uid, uid_len, service->sector_keys);

        mfc_event->data->poller_mode.mode = MfClassicPollerModeRead;
    } else if(mfc_event->type == MfClassicPollerEventTypeRequestReadSector) {
        MfClassicPollerEventDataReadSectorRequest* request =
            &mfc_event->data->read_sector_request_data;

        if(service->next_sector_to_read < NFC_SECTOR_COUNT) {
            request->sector_num = service->next_sector_to_read;
            request->key = service->sector_keys[service->next_sector_to_read];
            request->key_type = MfClassicKeyTypeA;
            request->key_provided = true;
            FURI_LOG_D(TAG, "Request read sector %u", (unsigned)request->sector_num);
            service->next_sector_to_read++;
        } else {
            request->key_provided = false;
        }
    } else if(mfc_event->type == MfClassicPollerEventTypeSuccess) {
        const MfClassicData* mfc_data = nfc_poller_get_data(service->poller);

        size_t uid_len = 0;
        const uint8_t* uid = mf_classic_get_uid(mfc_data, &uid_len);
        if(uid && uid_len <= NFC_MAX_UID_SIZE) {
            service->result.uid_len = uid_len;
            memcpy(service->result.uid, uid, uid_len);
        }

        for(size_t i = 0; i < NFC_BLOCK_COUNT; i++) {
            service->result.block_read[i] = mf_classic_is_block_read(mfc_data, i);
            if(service->result.block_read[i]) {
                memcpy(service->result.blocks[i], mfc_data->block[i].data, NFC_BLOCK_SIZE);
            }
        }

        if(service->result.block_read[9]) {
            size_t read_count = 0;
            for(size_t i = 0; i < NFC_BLOCK_COUNT; i++) {
                if(service->result.block_read[i]) {
                    read_count++;
                }
            }
            FURI_LOG_I(TAG, "Read complete, blocks_read=%u/%u", (unsigned)read_count, NFC_BLOCK_COUNT);
            service->has_result = true;
            emit(service, SpoolmanNfcServiceEventSuccess, &service->result);
        } else {
            FURI_LOG_E(TAG, "Read finished but block 9 is unreadable");
            emit(service, SpoolmanNfcServiceEventError, NULL);
        }

        return NfcCommandStop;
    } else if(mfc_event->type == MfClassicPollerEventTypeFail) {
        FURI_LOG_E(
            TAG,
            "Read failed near sector %u",
            (unsigned)(service->next_sector_to_read == 0 ? 0 : service->next_sector_to_read - 1));
        emit(service, SpoolmanNfcServiceEventError, NULL);
        return NfcCommandStop;
    }

    return NfcCommandContinue;
}

NfcReader* nfc_reader_alloc(NfcReadCallback callback, void* context) {
    NfcReader* service = malloc(sizeof(NfcReader));
    memset(service, 0, sizeof(NfcReader));
    service->callback = callback;
    service->context = context;
    service->nfc = nfc_alloc();
    service->poller = nfc_poller_alloc(service->nfc, NfcProtocolMfClassic);

    return service;
}

void nfc_reader_stop(NfcReader* service) {
    furi_assert(service);
    nfc_reader_stop_poller(service);
    nfc_poller_free(service->poller);
    nfc_free(service->nfc);
    free(service);
}

void nfc_reader_start(NfcReader* service) {
    furi_assert(service);
    nfc_poller_start(service->poller, callback, service);
}

static void nfc_reader_stop_poller(NfcReader* service) {
    furi_assert(service);
    nfc_poller_stop(service->poller);
}

bool nfc_reader_get_result(const NfcReader* service, NfcScanResult* result) {
    furi_assert(service);
    furi_assert(result);

    if(service->has_result) {
        *result = service->result;
    }

    return service->has_result;
}
