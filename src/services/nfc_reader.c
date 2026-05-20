#include "nfc_reader.h"

#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/protocols/mf_classic/mf_classic_poller.h>

struct NfcReader {
    Nfc* nfc;
    NfcPoller* poller;
    MfClassicKey sector2_key;
    bool sector2_requested;
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
    furi_assert(data_len <= 32);

    uint8_t inner[64 + 32] = {};
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

static void derive_bambu_key(const uint8_t* uid, size_t uid_len, uint8_t* key_out) {
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

    uint8_t info[] = "RFID-A";
    uint8_t t1_input[8];
    memcpy(t1_input, info, 7);
    t1_input[7] = 0x01;

    uint8_t t1[32];
    hmac_sha256(prk, 32, t1_input, 8, t1);

    memcpy(key_out, &t1[12], 6);
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
        service->sector2_requested = false;
        service->has_result = false;
        emit(service, SpoolmanNfcServiceEventReading, NULL);

        const MfClassicData* mfc_data = nfc_poller_get_data(service->poller);
        size_t uid_len = 0;
        const uint8_t* uid = mf_classic_get_uid(mfc_data, &uid_len);
        derive_bambu_key(uid, uid_len, service->sector2_key.data);

        mfc_event->data->poller_mode.mode = MfClassicPollerModeRead;
    } else if(mfc_event->type == MfClassicPollerEventTypeRequestReadSector) {
        MfClassicPollerEventDataReadSectorRequest* request =
            &mfc_event->data->read_sector_request_data;

        if(!service->sector2_requested) {
            service->sector2_requested = true;
            request->sector_num = 2;
            request->key = service->sector2_key;
            request->key_type = MfClassicKeyTypeA;
            request->key_provided = true;
        } else {
            request->key_provided = false;
        }
    } else if(mfc_event->type == MfClassicPollerEventTypeSuccess) {
        const MfClassicData* mfc_data = nfc_poller_get_data(service->poller);

        if(mf_classic_is_block_read(mfc_data, 9)) {
            memcpy(
                service->result.block9, mfc_data->block[9].data, sizeof(service->result.block9));
            service->has_result = true;
            emit(service, SpoolmanNfcServiceEventSuccess, &service->result);
        } else {
            emit(service, SpoolmanNfcServiceEventError, NULL);
        }

        return NfcCommandStop;
    } else if(mfc_event->type == MfClassicPollerEventTypeFail) {
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
