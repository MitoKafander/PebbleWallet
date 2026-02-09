#pragma once
#include <pebble.h>

// --- Constants ---
#define MAX_CARDS 10
#define MAX_NAME_LEN 32
#define MAX_DATA_LEN 1024
#define PERSIST_KEY_COUNT 500
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

typedef struct {
    BarcodeFormat format;
    char name[MAX_NAME_LEN];
    char description[MAX_NAME_LEN];
    char data[MAX_DATA_LEN];
} WalletCard;

// --- Global State ---
extern WalletCard g_cards[MAX_CARDS];
extern int g_card_count;

// --- Storage ---
void storage_load_cards(void);
void storage_save_card(int index, WalletCard *card);
void storage_save_count(int count);

// --- QR Generator ---
bool qr_generate_packed(const char *data, uint8_t *output_buffer, uint8_t *out_size);

// --- Barcode Renderer ---
void barcode_draw(GContext *ctx, GRect bounds, const char *data, BarcodeFormat format);
