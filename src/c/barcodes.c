#include "common.h"
#include <string.h>

// ============================================================================
// Code 128 Barcode Rendering (our original implementation)
// Supports Code 128B (alphanumeric) and Code 128C (numeric pairs)
// ============================================================================

static const uint8_t CODE128_PATTERNS[][6] = {
    {2, 1, 2, 2, 2, 2}, // 0: Space
    {2, 2, 2, 1, 2, 2}, // 1: !
    {2, 2, 2, 2, 2, 1}, // 2: "
    {1, 2, 1, 2, 2, 3}, // 3: #
    {1, 2, 1, 3, 2, 2}, // 4: $
    {1, 3, 1, 2, 2, 2}, // 5: %
    {1, 2, 2, 2, 1, 3}, // 6: &
    {1, 2, 2, 3, 1, 2}, // 7: '
    {1, 3, 2, 2, 1, 2}, // 8: (
    {2, 2, 1, 2, 1, 3}, // 9: )
    {2, 2, 1, 3, 1, 2}, // 10: *
    {2, 3, 1, 2, 1, 2}, // 11: +
    {1, 1, 2, 2, 3, 2}, // 12: ,
    {1, 2, 2, 1, 3, 2}, // 13: -
    {1, 2, 2, 2, 3, 1}, // 14: .
    {1, 1, 3, 2, 2, 2}, // 15: /
    {1, 2, 3, 1, 2, 2}, // 16: 0
    {1, 2, 3, 2, 2, 1}, // 17: 1
    {2, 2, 3, 2, 1, 1}, // 18: 2
    {2, 2, 1, 1, 3, 2}, // 19: 3
    {2, 2, 1, 2, 3, 1}, // 20: 4
    {2, 1, 3, 2, 1, 2}, // 21: 5
    {2, 2, 3, 1, 1, 2}, // 22: 6
    {3, 1, 2, 1, 3, 1}, // 23: 7
    {3, 1, 1, 2, 2, 2}, // 24: 8
    {3, 2, 1, 1, 2, 2}, // 25: 9
    {3, 2, 1, 2, 2, 1}, // 26: :
    {3, 1, 2, 2, 1, 2}, // 27: ;
    {3, 2, 2, 1, 1, 2}, // 28: <
    {3, 2, 2, 2, 1, 1}, // 29: =
    {2, 1, 2, 1, 2, 3}, // 30: >
    {2, 1, 2, 3, 2, 1}, // 31: ?
    {2, 3, 2, 1, 2, 1}, // 32: @
    {1, 1, 1, 3, 2, 3}, // 33: A
    {1, 3, 1, 1, 2, 3}, // 34: B
    {1, 3, 1, 3, 2, 1}, // 35: C
    {1, 1, 2, 3, 1, 3}, // 36: D
    {1, 3, 2, 1, 1, 3}, // 37: E
    {1, 3, 2, 3, 1, 1}, // 38: F
    {2, 1, 1, 3, 1, 3}, // 39: G
    {2, 3, 1, 1, 1, 3}, // 40: H
    {2, 3, 1, 3, 1, 1}, // 41: I
    {1, 1, 2, 1, 3, 3}, // 42: J
    {1, 1, 2, 3, 3, 1}, // 43: K
    {1, 3, 2, 1, 3, 1}, // 44: L
    {1, 1, 3, 1, 2, 3}, // 45: M
    {1, 1, 3, 3, 2, 1}, // 46: N
    {1, 3, 3, 1, 2, 1}, // 47: O
    {3, 1, 3, 1, 2, 1}, // 48: P
    {2, 1, 1, 3, 3, 1}, // 49: Q
    {2, 3, 1, 1, 3, 1}, // 50: R
    {2, 1, 3, 1, 1, 3}, // 51: S
    {2, 1, 3, 3, 1, 1}, // 52: T
    {2, 1, 3, 1, 3, 1}, // 53: U
    {3, 1, 1, 1, 2, 3}, // 54: V
    {3, 1, 1, 3, 2, 1}, // 55: W
    {3, 3, 1, 1, 2, 1}, // 56: X
    {3, 1, 2, 1, 1, 3}, // 57: Y
    {3, 1, 2, 3, 1, 1}, // 58: Z
    {3, 3, 2, 1, 1, 1}, // 59: [
    {3, 1, 4, 1, 1, 1}, // 60: backslash
    {2, 2, 1, 4, 1, 1}, // 61: ]
    {4, 3, 1, 1, 1, 1}, // 62: ^
    {1, 1, 1, 2, 2, 4}, // 63: _
    {1, 1, 1, 4, 2, 2}, // 64: `
    {1, 2, 1, 1, 2, 4}, // 65: a
    {1, 2, 1, 4, 2, 1}, // 66: b
    {1, 4, 1, 1, 2, 2}, // 67: c
    {1, 4, 1, 2, 2, 1}, // 68: d
    {1, 1, 2, 2, 1, 4}, // 69: e
    {1, 1, 2, 4, 1, 2}, // 70: f
    {1, 2, 2, 1, 1, 4}, // 71: g
    {1, 2, 2, 4, 1, 1}, // 72: h
    {1, 4, 2, 1, 1, 2}, // 73: i
    {1, 4, 2, 2, 1, 1}, // 74: j
    {2, 4, 1, 2, 1, 1}, // 75: k
    {2, 2, 1, 1, 1, 4}, // 76: l
    {4, 1, 3, 1, 1, 1}, // 77: m
    {2, 4, 1, 1, 1, 2}, // 78: n
    {1, 3, 4, 1, 1, 1}, // 79: o
    {1, 1, 1, 2, 4, 2}, // 80: p
    {1, 2, 1, 1, 4, 2}, // 81: q
    {1, 2, 1, 2, 4, 1}, // 82: r
    {1, 1, 4, 2, 1, 2}, // 83: s
    {1, 2, 4, 1, 1, 2}, // 84: t
    {1, 2, 4, 2, 1, 1}, // 85: u
    {4, 1, 1, 2, 1, 2}, // 86: v
    {4, 2, 1, 1, 1, 2}, // 87: w
    {4, 2, 1, 2, 1, 1}, // 88: x
    {2, 1, 2, 1, 4, 1}, // 89: y
    {2, 1, 4, 1, 2, 1}, // 90: z
    {4, 1, 2, 1, 2, 1}, // 91: {
    {1, 1, 1, 1, 4, 3}, // 92: |
    {1, 1, 1, 3, 4, 1}, // 93: }
    {1, 3, 1, 1, 4, 1}, // 94: ~
    {1, 1, 4, 1, 1, 3}, // 95: DEL
    {1, 1, 4, 3, 1, 1}, // 96: FNC3
    {4, 1, 1, 1, 1, 3}, // 97: FNC2
    {4, 1, 1, 3, 1, 1}, // 98: SHIFT
    {1, 1, 3, 1, 4, 1}, // 99: Code C
    {1, 1, 4, 1, 3, 1}, // 100: Code B / FNC4
    {3, 1, 1, 1, 4, 1}, // 101: Code A / FNC4
    {4, 1, 1, 1, 3, 1}, // 102: FNC1
    {2, 1, 1, 4, 1, 2}, // 103: Start A
    {2, 1, 1, 2, 1, 4}, // 104: Start B
    {2, 1, 1, 2, 3, 2}, // 105: Start C
};

static const uint8_t CODE128_STOP[] = {2, 3, 3, 1, 1, 1, 2};

// --- Helpers ---

static void draw_bar(GContext *ctx, int x, int y, int width, int height, bool black) {
    graphics_context_set_fill_color(ctx, black ? GColorBlack : GColorWhite);
    graphics_fill_rect(ctx, GRect(x, y, width, height), 0, GCornerNone);
}

static bool is_all_digits(const char *str) {
    for (int i = 0; str[i]; i++) {
        if (str[i] < '0' || str[i] > '9') return false;
    }
    return true;
}

static int get_code128_value(char c) {
    if (c >= 32 && c <= 127) return c - 32;
    return 0;
}

static int calculate_code128_checksum(const char *data) {
    int checksum = 104; // Start B
    int len = strlen(data);
    for (int i = 0; i < len && i < MAX_DATA_LEN; i++) {
        checksum += get_code128_value(data[i]) * (i + 1);
    }
    return checksum % 103;
}

static int calculate_code128c_checksum(const char *data) {
    int checksum = 105; // Start C
    int len = strlen(data);
    int pos = 1;
    for (int i = 0; i < len; i += 2) {
        int value = (data[i] - '0') * 10;
        if (i + 1 < len) value += (data[i + 1] - '0');
        checksum += value * pos;
        pos++;
    }
    return checksum % 103;
}

// --- Code 128 Barcode Drawing ---

static void draw_code128_barcode(GContext *ctx, GRect bounds, const char *data) {
    int data_len = strlen(data);
    if (data_len == 0 || data_len > MAX_DATA_LEN) return;

    bool is_numeric = is_all_digits(data);
    bool use_code_c = is_numeric;

    char padded_data[MAX_DATA_LEN + 2];
    const char *barcode_data = data;
    int barcode_len = data_len;

    if (is_numeric && (data_len % 2 == 1)) {
        padded_data[0] = '0';
        strncpy(padded_data + 1, data, MAX_DATA_LEN);
        padded_data[MAX_DATA_LEN + 1] = '\0';
        barcode_data = padded_data;
        barcode_len = data_len + 1;
    }

    int bar_modules;
    if (use_code_c) {
        int pairs = barcode_len / 2;
        bar_modules = 11 + (pairs * 11) + 11 + 13;
    } else {
        bar_modules = 11 + (barcode_len * 11) + 11 + 13;
    }

    int bar_height = bounds.size.h - 50;
    int bar_y = 25;
    int screen_margin = 6;
    int available_width = bounds.size.w - (screen_margin * 2);
    int module_width = available_width / bar_modules;
    if (module_width < 1) module_width = 1;

    int barcode_width = bar_modules * module_width;
    int start_x = screen_margin + (available_width - barcode_width) / 2;
    if (start_x < screen_margin) start_x = screen_margin;
    int right_limit = bounds.size.w - screen_margin;

    // White background
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, GRect(screen_margin, bar_y - 5, available_width, bar_height + 10), 0, GCornerNone);

    int x = start_x;

    if (use_code_c) {
        // Start C (index 105)
        for (int i = 0; i < 6 && x < right_limit; i++) {
            int w = CODE128_PATTERNS[105][i] * module_width;
            if (x + w > right_limit) w = right_limit - x;
            draw_bar(ctx, x, bar_y, w, bar_height, (i % 2 == 0));
            x += w;
        }
        // Digit pairs
        for (int i = 0; i < barcode_len && x < right_limit; i += 2) {
            int value = (barcode_data[i] - '0') * 10;
            if (i + 1 < barcode_len) value += (barcode_data[i + 1] - '0');
            for (int j = 0; j < 6 && x < right_limit; j++) {
                int w = CODE128_PATTERNS[value][j] * module_width;
                if (x + w > right_limit) w = right_limit - x;
                draw_bar(ctx, x, bar_y, w, bar_height, (j % 2 == 0));
                x += w;
            }
        }
        // Checksum
        int checksum = calculate_code128c_checksum(barcode_data);
        for (int i = 0; i < 6 && x < right_limit; i++) {
            int w = CODE128_PATTERNS[checksum][i] * module_width;
            if (x + w > right_limit) w = right_limit - x;
            draw_bar(ctx, x, bar_y, w, bar_height, (i % 2 == 0));
            x += w;
        }
    } else {
        // Start B (index 104)
        for (int i = 0; i < 6 && x < right_limit; i++) {
            int w = CODE128_PATTERNS[104][i] * module_width;
            if (x + w > right_limit) w = right_limit - x;
            draw_bar(ctx, x, bar_y, w, bar_height, (i % 2 == 0));
            x += w;
        }
        // Data characters
        for (int i = 0; i < barcode_len && x < right_limit; i++) {
            int value = get_code128_value(barcode_data[i]);
            for (int j = 0; j < 6 && x < right_limit; j++) {
                int w = CODE128_PATTERNS[value][j] * module_width;
                if (x + w > right_limit) w = right_limit - x;
                draw_bar(ctx, x, bar_y, w, bar_height, (j % 2 == 0));
                x += w;
            }
        }
        // Checksum
        int checksum = calculate_code128_checksum(barcode_data);
        for (int i = 0; i < 6 && x < right_limit; i++) {
            int w = CODE128_PATTERNS[checksum][i] * module_width;
            if (x + w > right_limit) w = right_limit - x;
            draw_bar(ctx, x, bar_y, w, bar_height, (i % 2 == 0));
            x += w;
        }
    }

    // Stop pattern
    for (int i = 0; i < 7 && x < right_limit; i++) {
        int w = CODE128_STOP[i] * module_width;
        if (x + w > right_limit) w = right_limit - x;
        draw_bar(ctx, x, bar_y, w, bar_height, (i % 2 == 0));
        x += w;
    }
}

// ============================================================================
// Pre-rendered Matrix Drawing (for Aztec, PDF417, phone-generated QR)
// Data format: "width,height,hexdata"
// Uses RLE optimization - groups consecutive black pixels into single rectangles
// ============================================================================

static void draw_precalc_matrix(GContext *ctx, GRect bounds, const char *data) {
    int width = 0, height = 0;
    const char *p = data;

    // Parse width
    while (*p && *p != ',') {
        if (*p >= '0' && *p <= '9') width = width * 10 + (*p - '0');
        p++;
    }
    if (*p == ',') p++; else return;

    // Parse height
    while (*p && *p != ',') {
        if (*p >= '0' && *p <= '9') height = height * 10 + (*p - '0');
        p++;
    }
    if (*p == ',') p++; else return;

    if (width <= 0 || height <= 0) return;

    // Calculate scale to fit display
    int avail_w = bounds.size.w - 10;
    int avail_h = bounds.size.h - 10;
    int scale_w = avail_w / width;
    int scale_h = avail_h / height;
    int scale = (scale_w < scale_h) ? scale_w : scale_h;
    if (scale < 1) scale = 1;

    int pix_w = width * scale;
    int pix_h = height * scale;
    int ox = (bounds.size.w - pix_w) / 2;
    int oy = (bounds.size.h - pix_h) / 2;

    // White background with quiet zone
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, GRect(ox - 4, oy - 4, pix_w + 8, pix_h + 8), 0, GCornerNone);
    graphics_context_set_fill_color(ctx, GColorBlack);

    // Check for truncated data
    int total_pixels = width * height;
    size_t actual_data_len = strlen(p);
    if (actual_data_len * 4 < (size_t)total_pixels) {
        graphics_context_set_text_color(ctx, GColorBlack);
        graphics_draw_text(ctx, "Data Truncated\nPlease Resync",
            fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), bounds,
            GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
        return;
    }

    // Draw with RLE: group consecutive black pixels into rectangles
    int current_bit = 0;
    int row = 0, col = 0;
    int run_start_col = -1;

    while (*p && current_bit < total_pixels) {
        char c = *p;
        int val = 0;
        if (c >= '0' && c <= '9') val = c - '0';
        else if (c >= 'A' && c <= 'F') val = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') val = c - 'a' + 10;

        for (int b = 3; b >= 0; b--) {
            if (current_bit >= total_pixels) break;
            bool is_black = (val & (1 << b));
            if (is_black) {
                if (run_start_col == -1) run_start_col = col;
            } else {
                if (run_start_col != -1) {
                    int run_width = col - run_start_col;
                    graphics_fill_rect(ctx, GRect(ox + run_start_col * scale, oy + row * scale, run_width * scale, scale), 0, GCornerNone);
                    run_start_col = -1;
                }
            }
            col++;
            if (col >= width) {
                if (run_start_col != -1) {
                    int run_width = col - run_start_col;
                    graphics_fill_rect(ctx, GRect(ox + run_start_col * scale, oy + row * scale, run_width * scale, scale), 0, GCornerNone);
                    run_start_col = -1;
                }
                col = 0;
                row++;
            }
            current_bit++;
        }
        p++;
    }
}

// ============================================================================
// QR Code Drawing (on-watch generated)
// ============================================================================

static void draw_qr_code(GContext *ctx, GRect bounds, const char *data) {
    uint8_t packed[200];
    uint8_t size = 0;

    if (qr_generate_packed(data, packed, &size)) {
        int avail = (bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h) - 10;
        int scale = avail / size;
        if (scale < 2) scale = 2;
        int pix_size = size * scale;
        int ox = (bounds.size.w - pix_size) / 2;
        int oy = (bounds.size.h - pix_size) / 2;

        // White quiet zone
        graphics_context_set_fill_color(ctx, GColorWhite);
        graphics_fill_rect(ctx, GRect(ox - 4, oy - 4, pix_size + 8, pix_size + 8), 0, GCornerNone);

        // Draw modules
        graphics_context_set_fill_color(ctx, GColorBlack);
        for (int r = 0; r < size; r++) {
            for (int c = 0; c < size; c++) {
                int idx = r * size + c;
                if (packed[idx / 8] & (1 << (7 - (idx % 8)))) {
                    graphics_fill_rect(ctx, GRect(ox + c * scale, oy + r * scale, scale, scale), 0, GCornerNone);
                }
            }
        }
    } else {
        graphics_context_set_text_color(ctx, GColorBlack);
        graphics_draw_text(ctx, "QR Error",
            fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), bounds,
            GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    }
}

// ============================================================================
// Main Dispatcher
// ============================================================================

void barcode_draw(GContext *ctx, GRect bounds, const char *data, BarcodeFormat format) {
    // Clear background
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    switch (format) {
        case FORMAT_CODE128:
        case FORMAT_CODE39:
        case FORMAT_EAN13:
            draw_code128_barcode(ctx, bounds, data);
            break;

        case FORMAT_AZTEC:
        case FORMAT_PDF417:
            draw_precalc_matrix(ctx, bounds, data);
            break;

        case FORMAT_QR:
            // If data starts with a digit, it might be pre-rendered "w,h,hex" format
            if (data[0] >= '0' && data[0] <= '9' && strchr(data, ',') != NULL) {
                draw_precalc_matrix(ctx, bounds, data);
            } else {
                draw_qr_code(ctx, bounds, data);
            }
            break;
    }
}
