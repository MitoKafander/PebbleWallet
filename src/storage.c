#include "common.h"
#include <stddef.h>

// Binary storage for pre-rendered barcode data.
// Key layout per card (12 keys):
//   BASE + (i*12) + 0:    WalletCardInfo struct
//   BASE + (i*12) + 1..11: Binary pixel data in 100-byte chunks

#define KEYS_PER_CARD 12
#define STORAGE_CHUNK_SIZE 100

// --- Legacy cleanup (v2.0.0 used 8 keys per card with hex compression) ---
#define LEGACY_KEY_COUNT 100
#define LEGACY_KEY_BASE 1000
#define V2_KEYS_PER_CARD 8

static void storage_wipe_legacy(void) {
    // Wipe v1 legacy storage
    if (persist_exists(LEGACY_KEY_COUNT)) {
        persist_delete(LEGACY_KEY_COUNT);
        for (int i = 0; i < 50; i++) {
            if (persist_exists(LEGACY_KEY_BASE + i)) persist_delete(LEGACY_KEY_BASE + i);
        }
    }
    // Wipe v2.0.0 storage (same base key but different struct layout)
    // Only wipe if we detect old-format data (card info was different size)
    // This is handled implicitly: new code reads WalletCardInfo which is smaller,
    // and old data chunks get overwritten on first sync.
}

// --- Public API ---

void storage_load_cards(void) {
    storage_wipe_legacy();

    if (!persist_exists(PERSIST_KEY_COUNT)) {
        g_card_count = 0;
        return;
    }

    g_card_count = persist_read_int(PERSIST_KEY_COUNT);
    if (g_card_count > MAX_CARDS) g_card_count = MAX_CARDS;

    for (int i = 0; i < g_card_count; i++) {
        int base_key = PERSIST_KEY_BASE + (i * KEYS_PER_CARD);
        persist_read_data(base_key, &g_cards[i], sizeof(WalletCardInfo));
    }
    APP_LOG(APP_LOG_LEVEL_INFO, "Loaded %d cards from storage", g_card_count);
}

void storage_load_card_data(int index, uint8_t *buffer, int max_len) {
    if (!buffer || index < 0 || index >= g_card_count) return;
    int base_key = PERSIST_KEY_BASE + (index * KEYS_PER_CARD);

    int total_read = 0;
    for (int k = 1; k < KEYS_PER_CARD; k++) {
        int chunk_key = base_key + k;
        if (persist_exists(chunk_key) && total_read < max_len) {
            int read = persist_read_data(chunk_key, buffer + total_read, STORAGE_CHUNK_SIZE);
            total_read += read;
        } else {
            break;
        }
    }
}

void storage_save_card(int index, WalletCardInfo *info, const uint8_t *bits, int bits_len) {
    if (index < 0 || index >= MAX_CARDS) return;
    int base_key = PERSIST_KEY_BASE + (index * KEYS_PER_CARD);

    // Save card info struct
    persist_write_data(base_key, info, sizeof(WalletCardInfo));

    // Save binary data in chunks
    int offset = 0;
    for (int k = 1; k < KEYS_PER_CARD; k++) {
        int chunk_key = base_key + k;
        if (offset < bits_len) {
            int remaining = bits_len - offset;
            int write_len = (remaining > STORAGE_CHUNK_SIZE) ? STORAGE_CHUNK_SIZE : remaining;
            int result = persist_write_data(chunk_key, bits + offset, write_len);
            if (result < 0) {
                APP_LOG(APP_LOG_LEVEL_ERROR, "Storage write failed at chunk %d", k);
            }
            offset += write_len;
        } else {
            if (persist_exists(chunk_key)) persist_delete(chunk_key);
        }
    }
}

void storage_save_count(int count) {
    if (count > MAX_CARDS) count = MAX_CARDS;
    g_card_count = count;
    persist_write_int(PERSIST_KEY_COUNT, count);
}
