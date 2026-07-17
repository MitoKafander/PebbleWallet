#include "common.h"
#include <stddef.h>

// Binary storage for pre-rendered barcode data.
// Key layout per card (16 keys):
//   BASE + (i*16) + 0:     WalletCardInfo struct
//   BASE + (i*16) + 1..14: Binary pixel data in 100-byte chunks
//   BASE + (i*16) + 15:    Human-readable text (<=255 bytes, one value)
// 14 data chunks x 100 = 1400 bytes = MAX_BITS_LEN (fits a full boarding pass).
// NOTE: Pebble gives each app only ~4KB of persistent storage total, so the
// phone side (pebble-js-app.js) budgets the whole card set before syncing.

#define KEYS_PER_CARD 16
#define STORAGE_CHUNK_SIZE 100
#define TEXT_KEY_OFFSET 15    // per-card key holding the raw text

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

    // Schema migration: the per-card key stride changed in v2.3.0 (12 -> 15),
    // so cards written by an older version would be misread at the new offsets.
    // If the stored schema doesn't match, wipe all card storage and start clean.
    // No data is lost: the phone re-syncs from its own localStorage on launch.
    int schema = persist_exists(PERSIST_KEY_SCHEMA)
                     ? persist_read_int(PERSIST_KEY_SCHEMA) : 0;
    if (schema != STORAGE_SCHEMA_VERSION) {
        int last_key = PERSIST_KEY_BASE + (MAX_CARDS * KEYS_PER_CARD);
        for (int key = PERSIST_KEY_BASE; key < last_key; key++) {
            if (persist_exists(key)) persist_delete(key);
        }
        if (persist_exists(PERSIST_KEY_COUNT)) persist_delete(PERSIST_KEY_COUNT);
        persist_write_int(PERSIST_KEY_SCHEMA, STORAGE_SCHEMA_VERSION);
        g_card_count = 0;
        APP_LOG(APP_LOG_LEVEL_INFO, "Storage migrated to schema v%d (cleared)",
                STORAGE_SCHEMA_VERSION);
        return;
    }

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
    // Matrix data lives in chunk keys 1..14 (stop before the text key at 15).
    for (int k = 1; k < TEXT_KEY_OFFSET; k++) {
        int chunk_key = base_key + k;
        if (persist_exists(chunk_key) && total_read < max_len) {
            // Never let a 100-byte chunk write past the caller's buffer.
            int want = max_len - total_read;
            if (want > STORAGE_CHUNK_SIZE) want = STORAGE_CHUNK_SIZE;
            int read = persist_read_data(chunk_key, buffer + total_read, want);
            if (read <= 0) break;
            total_read += read;
        } else {
            break;
        }
    }
}

// Load the card's raw human-readable text into buffer (always null-terminated).
void storage_load_card_text(int index, char *buffer, int max_len) {
    if (!buffer || max_len <= 0) return;
    buffer[0] = '\0';
    if (index < 0 || index >= g_card_count) return;
    int text_key = PERSIST_KEY_BASE + (index * KEYS_PER_CARD) + TEXT_KEY_OFFSET;
    if (!persist_exists(text_key)) return;
    int read = persist_read_data(text_key, buffer, max_len - 1);
    if (read < 0) read = 0;
    if (read > max_len - 1) read = max_len - 1;
    buffer[read] = '\0';
}

// Delete every persisted card slot's data (used on sync start so a shrinking
// card set doesn't leak orphaned pixel data against the ~4KB persist budget).
void storage_wipe_all_cards(void) {
    int last_key = PERSIST_KEY_BASE + (MAX_CARDS * KEYS_PER_CARD);
    for (int key = PERSIST_KEY_BASE; key < last_key; key++) {
        if (persist_exists(key)) persist_delete(key);
    }
}

bool storage_save_card(int index, WalletCardInfo *info, const uint8_t *bits,
                       int bits_len, const char *text, int text_len) {
    if (index < 0 || index >= MAX_CARDS) return false;
    int base_key = PERSIST_KEY_BASE + (index * KEYS_PER_CARD);
    bool ok = true;

    // Save card info struct
    if (persist_write_data(base_key, info, sizeof(WalletCardInfo)) < 0) ok = false;

    // Save binary matrix data in chunks (keys 1..14, before the text key).
    int offset = 0;
    for (int k = 1; k < TEXT_KEY_OFFSET; k++) {
        int chunk_key = base_key + k;
        if (offset < bits_len) {
            int remaining = bits_len - offset;
            int write_len = (remaining > STORAGE_CHUNK_SIZE) ? STORAGE_CHUNK_SIZE : remaining;
            int result = persist_write_data(chunk_key, bits + offset, write_len);
            if (result < 0) {
                APP_LOG(APP_LOG_LEVEL_ERROR, "Storage write failed at chunk %d (budget full?)", k);
                ok = false;
            }
            offset += write_len;
        } else {
            if (persist_exists(chunk_key)) persist_delete(chunk_key);
        }
    }

    // Save the raw text in its own key (key 15), or clear it if empty.
    int text_key = base_key + TEXT_KEY_OFFSET;
    if (text && text_len > 0) {
        if (text_len > MAX_TEXT_LEN) text_len = MAX_TEXT_LEN;
        if (persist_write_data(text_key, text, text_len) < 0) ok = false;
    } else {
        if (persist_exists(text_key)) persist_delete(text_key);
    }
    return ok;
}

// Remember the last-viewed card so the app can open straight to it next launch.
void storage_save_last_index(int index) {
    persist_write_int(PERSIST_KEY_LAST, index);
}

int storage_load_last_index(void) {
    return persist_exists(PERSIST_KEY_LAST) ? persist_read_int(PERSIST_KEY_LAST) : 0;
}

void storage_save_count(int count) {
    if (count > MAX_CARDS) count = MAX_CARDS;
    g_card_count = count;
    persist_write_int(PERSIST_KEY_COUNT, count);
}
