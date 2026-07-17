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
// 2D Code Renderer (QR, Aztec, PDF417)
// Fills as much of the screen as possible using PROPORTIONAL module boundaries
// (not integer scaling) so codes are as large as the display allows while a
// small white quiet zone remains. Modules end up 5px/6px etc. — camera scanners
// read the grid fine, and the code is noticeably bigger than integer scaling.
// Wide codes (PDF417) auto-rotate 90 degrees onto the taller axis.
// `max_bytes` guards against reading past the pixel buffer.
// ============================================================================

static int imin(int a, int b) { return a < b ? a : b; }

static void draw_2d(GContext *ctx, GRect bounds, uint16_t w, uint16_t h,
                    const uint8_t *bits, int max_bytes) {
    if (w == 0 || h == 0) return;

    int screen_w = bounds.size.w;
    int screen_h = bounds.size.h;

    // Drawable area = screen minus a small quiet-zone margin. On round displays
    // shrink to the inscribed square (~70%) so corners stay on-glass.
    int QZ = 5;
    int avail_w = screen_w - 2 * QZ;
    int avail_h = screen_h - 2 * QZ;
#if defined(PBL_ROUND)
    avail_w = (screen_w * 70) / 100;
    avail_h = (screen_h * 70) / 100;
#endif

    // Pick orientation (upright vs rotated 90) that yields the larger module
    // size. Compare in fixed-point (x1000) to avoid floats. Square codes tie and
    // stay upright.
    int m_upright = imin((avail_w * 1000) / (int)w, (avail_h * 1000) / (int)h);
    int m_rot     = imin((avail_w * 1000) / (int)h, (avail_h * 1000) / (int)w);
    bool rotate = m_rot > m_upright;

    int dx = rotate ? (int)h : (int)w;   // modules across screen-x
    int dy = rotate ? (int)w : (int)h;   // modules down screen-y

    // Largest drawn rectangle that keeps modules square and fits the drawable
    // area (preserve aspect: drawn_x/dx == drawn_y/dy).
    int drawn_x, drawn_y;
    if (avail_w * dy <= avail_h * dx) {           // width-limited
        drawn_x = avail_w;
        drawn_y = (avail_w * dy) / dx;
    } else {                                       // height-limited
        drawn_y = avail_h;
        drawn_x = (avail_h * dx) / dy;
    }
    int ox = (screen_w - drawn_x) / 2;
    int oy = (screen_h - drawn_y) / 2;

    for (int r = 0; r < (int)h; r++) {
        for (int c = 0; c < (int)w; c++) {
            int bit_idx = r * (int)w + c;
            if ((bit_idx >> 3) >= max_bytes) continue;   // never read past buffer
            if (!(bits[bit_idx >> 3] & (1 << (7 - (bit_idx & 7))))) continue;

            // Module coords in the (dx across, dy down) grid after optional rotation.
            int mcx = rotate ? ((int)h - 1 - r) : c;
            int mcy = rotate ? c : r;

            // Proportional boundaries → adjacent modules tile with no gaps.
            int x0 = ox + (mcx * drawn_x) / dx;
            int x1 = ox + ((mcx + 1) * drawn_x) / dx;
            int y0 = oy + (mcy * drawn_y) / dy;
            int y1 = oy + ((mcy + 1) * drawn_y) / dy;

            graphics_fill_rect(ctx, GRect(x0, y0, x1 - x0, y1 - y0), 0, GCornerNone);
        }
    }
}

// ============================================================================
// 1D Code Renderer (Code 128, Code 39, EAN-13)
// Rotates 90 degrees to use full screen height for bar pattern.
// Uses INTEGER scaling only — never fractional — to preserve bar width ratios.
// ============================================================================

static void draw_1d_rotated(GContext *ctx, GRect bounds, uint16_t w, uint16_t h,
                            const uint8_t *bits) {
    int screen_w = bounds.size.w;
    int screen_h = bounds.size.h;

    // Quiet zone margins (6px each end of bar pattern axis)
    int margin = 6;
    int available_h = screen_h - (margin * 2);

    // Two fit modes:
    //  - Fits (w <= available_h): integer scale, exact crisp bar-width ratios.
    //  - Too many modules: proportional fit — map each module boundary to a
    //    fractional position so the WHOLE code always shows (compressed) with
    //    proportionally-correct bar/space widths, instead of clipping or refusing.
    bool fits = ((int)w <= available_h);
    int scale = fits ? (available_h / (int)w) : 0;
    if (fits && scale < 1) scale = 1;
    int drawn_h = fits ? ((int)w * scale) : available_h;
    int y_base = margin + (available_h - drawn_h) / 2;
    if (y_base < margin) y_base = margin;

    // Bar length across the screen. 1D codes need a quiet zone only at the two
    // ENDS of the bar pattern (the module axis), not along the bar length — so
    // make bars nearly full-width for a bigger, easier-to-scan target.
    int bar_len = screen_w - 8;
    int x_offset = (screen_w - bar_len) / 2;

    // Sample middle row of bitmap (all rows identical for 1D barcodes)
    int r = h / 2;

    // RLE: group consecutive black modules into single draw calls. Run edges are
    // positioned by pos(c) so both modes share one path (integer or proportional).
    int run_start = -1;
    int limit = (int)w + 1;  // one extra step to flush a trailing run
    for (int c = 0; c < limit; c++) {
        bool is_black = false;
        if (c < (int)w) {
            int bit_idx = r * (int)w + c;
            if ((bit_idx >> 3) >= MAX_BITS_LEN) break;   // never read past buffer
            is_black = (bits[bit_idx / 8] & (1 << (7 - (bit_idx % 8))));
        }

        if (is_black) {
            if (run_start == -1) run_start = c;
        } else if (run_start != -1) {
            int y0 = y_base + (fits ? run_start * scale : (run_start * available_h) / (int)w);
            int y1 = y_base + (fits ? c * scale : (c * available_h) / (int)w);
            int rh = y1 - y0;
            if (rh < 1) rh = 1;  // keep every bar visible even when compressed
            graphics_fill_rect(ctx, GRect(x_offset, y0, bar_len, rh), 0, GCornerNone);
            run_start = -1;
        }
    }
}

// ============================================================================
// QR Code Drawing (on-watch fallback for small alphanumeric QR)
// ============================================================================

static void draw_qr_code_onwatch(GContext *ctx, GRect bounds, const char *data) {
    uint8_t packed[200];
    uint8_t size = 0;

    if (qr_generate_packed(data, packed, &size)) {
        int avail = (bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h) - 10;
        int scale = avail / size;
        if (scale < 2) scale = 2;
        int pix_size = size * scale;
        int ox = (bounds.size.w - pix_size) / 2;
        int oy = (bounds.size.h - pix_size) / 2;

        graphics_context_set_fill_color(ctx, GColorWhite);
        graphics_fill_rect(ctx, GRect(ox - 4, oy - 4, pix_size + 8, pix_size + 8), 0, GCornerNone);

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
        graphics_draw_text(ctx, "QR Too Large",
            fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), bounds,
            GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    }
}

// ============================================================================
// Main Dispatcher
// ============================================================================

void barcode_draw(GContext *ctx, GRect bounds, BarcodeFormat format,
                  uint16_t width, uint16_t height, const uint8_t *bits) {
    // Clear background
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    if (width > 0 && height > 0 && bits) {
        // Pre-rendered binary data from bwip-js
        graphics_context_set_fill_color(ctx, GColorBlack);
        switch (format) {
            case FORMAT_CODE128:
            case FORMAT_CODE39:
            case FORMAT_EAN13:
                draw_1d_rotated(ctx, bounds, width, height, bits);
                break;
            case FORMAT_QR:
            case FORMAT_AZTEC:
            case FORMAT_PDF417:
            default:
                draw_2d(ctx, bounds, width, height, bits, MAX_BITS_LEN);
                break;
        }
        return;
    }

    // Fallback: no pre-rendered data. Bits buffer contains raw text.
    // This handles demo cards and edge cases.
    const char *text_data = (const char *)bits;
    if (!text_data || text_data[0] == '\0') {
        graphics_context_set_text_color(ctx, GColorBlack);
        graphics_draw_text(ctx, "No Data\nSync from phone",
            fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), bounds,
            GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
        return;
    }

    switch (format) {
        case FORMAT_CODE128:
        case FORMAT_CODE39:
        case FORMAT_EAN13:
            draw_code128_barcode(ctx, bounds, text_data);
            break;

        case FORMAT_QR:
            draw_qr_code_onwatch(ctx, bounds, text_data);
            break;

        default:
            // Aztec/PDF417 without pre-rendering - can't render on watch
            graphics_context_set_text_color(ctx, GColorBlack);
            graphics_draw_text(ctx, "Resync from phone",
                fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), bounds,
                GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
            break;
    }
}
