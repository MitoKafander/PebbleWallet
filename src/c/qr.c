#include "common.h"
#include <string.h>

// ============================================================================
// QR Code Generator - On-watch, alphanumeric mode, versions 1-4, EC level L
// Static memory allocation (safe for Aplite's limited RAM)
// ============================================================================

#define QR_MAX_SIZE 33

// Static matrix in BSS segment (not stack) - saves ~1KB stack
static int8_t s_qr_matrix[QR_MAX_SIZE][QR_MAX_SIZE];

// --- Lookup Tables (ROM) ---

static const uint8_t ALPHANUM_MAP[128] = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    36, 255, 255, 255, 37, 38, 255, 255, 255, 255, 39, 40, 255, 41, 42, 43,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 44, 255, 255, 255, 255, 255,
    255, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
};

static const uint8_t GF_EXP[256] = {
    1,2,4,8,16,32,64,128,29,58,116,232,205,135,19,38,76,152,45,90,180,117,234,201,
    143,3,6,12,24,48,96,192,157,39,78,156,37,74,148,53,106,212,181,119,238,193,159,35,
    70,140,5,10,20,40,80,160,93,186,105,210,185,111,222,161,95,190,97,194,153,47,94,188,
    101,202,137,15,30,60,120,240,253,231,211,187,107,214,177,127,254,225,223,163,91,182,113,226,
    217,175,67,134,17,34,68,136,13,26,52,104,208,189,103,206,129,31,62,124,248,237,199,147,
    59,118,236,197,151,51,102,204,133,23,46,92,184,109,218,169,79,158,33,66,132,21,42,84,
    168,77,154,41,82,164,85,170,73,146,57,114,228,213,183,115,230,209,191,99,198,145,63,126,
    252,229,215,179,123,246,241,255,227,219,171,75,150,49,98,196,149,55,110,220,165,87,174,65,
    130,25,50,100,200,141,7,14,28,56,112,224,221,167,83,166,81,162,89,178,121,242,249,239,
    195,155,43,86,172,69,138,9,18,36,72,144,61,122,244,245,247,243,251,235,203,139,11,22,
    44,88,176,125,250,233,207,131,27,54,108,216,173,71,142,1
};

static const uint8_t GF_LOG[256] = {
    0,0,1,25,2,50,26,198,3,223,51,238,27,104,199,75,4,100,224,14,52,141,239,129,
    28,193,105,248,200,8,76,113,5,138,101,47,225,36,15,33,53,147,142,218,240,18,130,69,
    29,181,194,125,106,39,249,185,201,154,9,120,77,228,114,166,6,191,139,98,102,221,48,253,
    226,152,37,179,16,145,34,136,54,208,148,206,143,150,219,189,241,210,19,92,131,56,70,64,
    30,66,182,163,195,72,126,110,107,58,40,84,250,133,186,61,202,94,155,159,10,21,121,43,
    78,212,229,172,115,243,167,87,7,112,192,247,140,128,99,13,103,74,222,237,49,197,254,24,
    227,165,153,119,38,184,180,124,17,68,146,217,35,32,137,46,55,63,209,91,149,188,207,205,
    144,135,151,178,220,252,190,97,242,86,211,171,20,42,93,158,132,60,57,83,71,109,65,162,
    31,45,67,216,183,123,164,118,196,23,73,236,127,12,111,246,108,161,59,82,41,157,85,170,
    251,96,134,177,187,204,62,90,203,89,95,176,156,169,160,81,11,245,22,235,122,117,44,215,
    79,174,213,233,230,231,173,232,116,214,244,234,168,80,88,175
};

// Reed-Solomon generator polynomials (GF element values, NOT exponents)
static const uint8_t RS_GEN_V1[] = {1, 127, 122, 154, 164, 11, 68, 117};
static const uint8_t RS_GEN_V2[] = {1, 216, 194, 159, 111, 199, 94, 95, 113, 157, 193};
static const uint8_t RS_GEN_V3[] = {1, 29, 196, 111, 163, 112, 74, 10, 105, 105, 139, 132, 151, 32, 134, 26};
static const uint8_t RS_GEN_V4[] = {1, 152, 185, 240, 5, 111, 99, 6, 220, 112, 150, 69, 36, 187, 22, 228, 198, 121, 121, 165, 174};
static const uint8_t *RS_GENERATORS[] = {RS_GEN_V1, RS_GEN_V2, RS_GEN_V3, RS_GEN_V4};

// {size, data_cw, ec_cw, alpha_capacity}
static const struct { uint8_t size, data_cw, ec_cw, alpha_cap; } VERSIONS[4] = {
    {21, 19,  7,  25}, {25, 34, 10,  47}, {29, 55, 15,  77}, {33, 80, 20, 114}
};

// --- GF(256) Multiplication ---

static uint8_t gf_mul(uint8_t a, uint8_t b) {
    if (a == 0 || b == 0) return 0;
    return GF_EXP[(GF_LOG[a] + GF_LOG[b]) % 255];
}

// --- Bit Writing ---

static void write_bits(uint8_t *buf, int *bit_pos, int value, int num_bits) {
    for (int i = num_bits - 1; i >= 0; i--) {
        if (value & (1 << i))
            buf[*bit_pos / 8] |= (1 << (7 - (*bit_pos % 8)));
        (*bit_pos)++;
    }
}

// --- Finder Pattern ---

static void draw_finder(int r0, int c0, int size) {
    for (int r = -1; r <= 7; r++) {
        for (int c = -1; c <= 7; c++) {
            if (r0 + r < 0 || r0 + r >= size || c0 + c < 0 || c0 + c >= size) continue;
            bool outer = (r == -1 || r == 7 || c == -1 || c == 7);
            bool border = (r == 0 || r == 6 || c == 0 || c == 6);
            bool center = (r >= 2 && r <= 4 && c >= 2 && c <= 4);
            s_qr_matrix[r0 + r][c0 + c] = (outer ? 0 : (border || center ? 1 : 0));
        }
    }
}

// --- Public API ---

bool qr_generate_packed(const char *data, uint8_t *output_buffer, uint8_t *out_size) {
    if (!data || !output_buffer) return false;

    // Validate and select version
    int len = strlen(data);
    int ver_idx = -1;
    for (int i = 0; i < 4; i++) {
        if (len <= VERSIONS[i].alpha_cap) { ver_idx = i; break; }
    }
    if (ver_idx < 0) return false;

    // Uppercase and validate characters
    char upper[115];
    for (int i = 0; i < len; i++) {
        char c = data[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        if ((uint8_t)c >= 128 || ALPHANUM_MAP[(uint8_t)c] == 255) return false;
        upper[i] = c;
    }

    // Create data codewords
    uint8_t data_cw[80] = {0};
    int bit_pos = 0;
    write_bits(data_cw, &bit_pos, 0x2, 4);  // Mode: alphanumeric
    write_bits(data_cw, &bit_pos, len, 9);   // Character count

    for (int i = 0; i < len; i += 2) {
        int val = ALPHANUM_MAP[(uint8_t)upper[i]];
        if (i + 1 < len) {
            val = val * 45 + ALPHANUM_MAP[(uint8_t)upper[i + 1]];
            write_bits(data_cw, &bit_pos, val, 11);
        } else {
            write_bits(data_cw, &bit_pos, val, 6);
        }
    }

    // Terminator and padding
    int total_bits = VERSIONS[ver_idx].data_cw * 8;
    int term = total_bits - bit_pos;
    if (term > 4) term = 4;
    write_bits(data_cw, &bit_pos, 0, term);
    if (bit_pos % 8 != 0) write_bits(data_cw, &bit_pos, 0, 8 - (bit_pos % 8));

    int byte_cnt = bit_pos / 8;
    int pad_byte = 236;
    while (byte_cnt < VERSIONS[ver_idx].data_cw) {
        data_cw[byte_cnt++] = pad_byte;
        pad_byte = (pad_byte == 236) ? 17 : 236;
    }

    // Generate error correction codewords
    uint8_t ec_cw[20] = {0};
    int ec_len = VERSIONS[ver_idx].ec_cw;
    for (int i = 0; i < VERSIONS[ver_idx].data_cw; i++) {
        uint8_t factor = data_cw[i] ^ ec_cw[0];
        memmove(ec_cw, ec_cw + 1, ec_len - 1);
        ec_cw[ec_len - 1] = 0;
        for (int j = 0; j < ec_len; j++)
            ec_cw[j] ^= gf_mul(RS_GENERATORS[ver_idx][j + 1], factor);
    }

    // Build matrix
    int size = VERSIONS[ver_idx].size;
    memset(s_qr_matrix, -1, sizeof(s_qr_matrix));

    // Finder patterns
    draw_finder(0, 0, size);
    draw_finder(0, size - 7, size);
    draw_finder(size - 7, 0, size);

    // Alignment pattern for V2+
    if (ver_idx >= 1) {
        int a = size - 7;
        for (int r = -2; r <= 2; r++) {
            for (int c = -2; c <= 2; c++) {
                if (s_qr_matrix[a + r][a + c] == -1)
                    s_qr_matrix[a + r][a + c] = (r == -2 || r == 2 || c == -2 || c == 2 || (r == 0 && c == 0)) ? 1 : 0;
            }
        }
    }

    // Timing patterns
    for (int i = 8; i < size - 8; i++) {
        if (s_qr_matrix[6][i] == -1) s_qr_matrix[6][i] = (i % 2 == 0);
        if (s_qr_matrix[i][6] == -1) s_qr_matrix[i][6] = (i % 2 == 0);
    }

    // Reserve format areas
    for (int i = 0; i < 9; i++) {
        if (s_qr_matrix[8][i] == -1) s_qr_matrix[8][i] = 0;
        if (s_qr_matrix[i][8] == -1) s_qr_matrix[i][8] = 0;
    }
    for (int i = 0; i < 8; i++) {
        if (s_qr_matrix[8][size - 1 - i] == -1) s_qr_matrix[8][size - 1 - i] = 0;
        if (s_qr_matrix[size - 1 - i][8] == -1) s_qr_matrix[size - 1 - i][8] = 0;
    }
    s_qr_matrix[size - 8][8] = 1; // Dark module

    // Place data bits
    uint8_t all_cw[100];
    memcpy(all_cw, data_cw, VERSIONS[ver_idx].data_cw);
    memcpy(all_cw + VERSIONS[ver_idx].data_cw, ec_cw, ec_len);

    int bit_idx = 0;
    int col = size - 1;
    bool up = true;
    while (col >= 0) {
        if (col == 6) col--;
        for (int r_off = 0; r_off < size; r_off++) {
            int r = up ? (size - 1 - r_off) : r_off;
            for (int k = 0; k < 2; k++) {
                int c = col - k;
                if (c >= 0 && s_qr_matrix[r][c] == -1) {
                    if (bit_idx < (VERSIONS[ver_idx].data_cw + ec_len) * 8) {
                        s_qr_matrix[r][c] = (all_cw[bit_idx / 8] >> (7 - (bit_idx % 8))) & 1;
                        bit_idx++;
                    } else {
                        s_qr_matrix[r][c] = 0;
                    }
                }
            }
        }
        col -= 2;
        up = !up;
    }

    // Apply mask (pattern 0: checkerboard) and format info
    const uint8_t FMT[] = {1,1,1,0,1,1,1,1,1,0,0,0,1,0,0};
    for (int r = 0; r < size; r++) {
        for (int c = 0; c < size; c++) {
            bool reserved = (r < 9 && c < 9) || (r < 9 && c >= size - 8) || (r >= size - 8 && c < 9) || (r == 6 || c == 6);
            if (ver_idx >= 1) {
                int a = size - 7;
                if (r >= a - 2 && r <= a + 2 && c >= a - 2 && c <= a + 2) reserved = true;
            }
            if (!reserved && (r + c) % 2 == 0) s_qr_matrix[r][c] ^= 1;
        }
    }

    // Write format bits
    for (int i = 0; i < 6; i++) s_qr_matrix[8][i] = FMT[i];
    s_qr_matrix[8][7] = FMT[6]; s_qr_matrix[8][8] = FMT[7]; s_qr_matrix[7][8] = FMT[8];
    for (int i = 9; i < 15; i++) s_qr_matrix[14 - i][8] = FMT[i];
    for (int i = 0; i < 7; i++) s_qr_matrix[size - 1 - i][8] = FMT[i];
    for (int i = 7; i < 15; i++) s_qr_matrix[8][size - 15 + i] = FMT[i];

    // Pack output
    *out_size = size;
    int total_bytes = (size * size + 7) / 8;
    memset(output_buffer, 0, total_bytes);
    for (int r = 0; r < size; r++) {
        for (int c = 0; c < size; c++) {
            if (s_qr_matrix[r][c]) {
                int idx = r * size + c;
                output_buffer[idx / 8] |= (1 << (7 - (idx % 8)));
            }
        }
    }

    return true;
}
