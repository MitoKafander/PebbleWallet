#pragma once
#include <pebble.h>

// --- Constants ---
#define MAX_CARDS 10
#define MAX_NAME_LEN 32
#define MAX_DATA_LEN 1024
// 1400 bytes of raw bits = 11200 pixels (~105x106 2D, e.g. a full boarding-pass
// PDF417). Matrices this large are streamed from the phone in 80-byte chunks
// (see main.c chunked-sync protocol) so they are no longer capped by the
// AppMessage inbox size. Note: Pebble persistent storage is ~4KB total per app,
// so only a few cards this large can be stored at once (see storage.c).
#define MAX_BITS_LEN 1400
#define PERSIST_KEY_COUNT 500
#define PERSIST_KEY_SCHEMA 501
// Bump when the persistent card layout changes so upgrades wipe cleanly.
// v3 = chunked-sync layout (KEYS_PER_CARD 15, MAX_BITS_LEN 1400) introduced 2.3.0.
#define STORAGE_SCHEMA_VERSION 3
#define PERSIST_KEY_BASE 24200

// --- Types ---
typedef enum {
    FORMAT_CODE128 = 0,
    FORMAT_CODE39 = 1,
    FORMAT_EAN13 = 2,
    FORMAT_QR = 3,
    FORMAT_AZTEC = 4,
    FORMAT_PDF417 = 5
} BarcodeFormat;

// Lightweight card info (kept in RAM for all cards)
typedef struct {
    BarcodeFormat format;
    char name[MAX_NAME_LEN];
    char description[MAX_NAME_LEN];
    uint16_t width;    // Pre-rendered barcode width in pixels
    uint16_t height;   // Pre-rendered barcode height in pixels
    uint16_t data_len; // Length of stored binary data in bytes
} WalletCardInfo;

// --- Global State ---
extern WalletCardInfo g_cards[MAX_CARDS];
extern int g_card_count;
extern uint8_t g_active_bits[MAX_BITS_LEN]; // On-demand loaded barcode data

// --- Storage ---
void storage_load_cards(void);
bool storage_save_card(int index, WalletCardInfo *info, const uint8_t *bits, int bits_len);
void storage_save_count(int count);
void storage_load_card_data(int index, uint8_t *buffer, int max_len);
void storage_wipe_all_cards(void);

// --- QR Generator (on-watch fallback for small alphanumeric QR) ---
bool qr_generate_packed(const char *data, uint8_t *output_buffer, uint8_t *out_size);

// --- Barcode Renderer ---
void barcode_draw(GContext *ctx, GRect bounds, BarcodeFormat format,
                  uint16_t width, uint16_t height, const uint8_t *bits);
