#include "common.h"
#include <string.h>

// --- Global State ---
WalletCardInfo g_cards[MAX_CARDS];
int g_card_count = 0;
uint8_t g_active_bits[MAX_BITS_LEN];

static Window *s_main_window;
static MenuLayer *s_menu_layer;
static Window *s_detail_window;
static Layer *s_barcode_layer;
static int s_current_index = 0;
static bool s_loading = true;

// Chunked-sync reassembly state. A card arrives as one header message
// (KEY_DATA_LEN) followed by N data-chunk messages (KEY_DATA_OFFSET + KEY_DATA).
// Chunks are reassembled into g_active_bits (reused as staging to save RAM on
// aplite) and flushed to storage once the whole matrix has arrived.
static int s_rx_index = -1;    // card index currently being received (-1 = none)
static int s_rx_expected = 0;  // total matrix bytes expected for this card
static int s_rx_received = 0;  // bytes reassembled so far

// Forward declarations
static void request_cards_from_phone(void *data);

// ============================================================================
// Demo Cards (fallback when no phone and no persisted cards)
// Demo cards use width=0, height=0 to trigger on-watch Code128/QR fallback.
// The raw text is stored in g_active_bits when the demo card is viewed.
// ============================================================================

// Static text buffers for demo card data (loaded into g_active_bits on demand)
static const char *s_demo_data[] = {
    "6035550123456789",   // Starbucks
    "4012345678901",      // Target
    "29857341",           // Library
    "M1DOE/JOHN E ABC123" // Demo Flight (short enough for on-watch QR)
};

static void add_demo_cards(void) {
    strncpy(g_cards[0].name, "Starbucks", MAX_NAME_LEN - 1);
    strncpy(g_cards[0].description, "Rewards Card", MAX_NAME_LEN - 1);
    g_cards[0].format = FORMAT_CODE128;
    g_cards[0].width = 0; g_cards[0].height = 0; g_cards[0].data_len = 0;

    strncpy(g_cards[1].name, "Target Circle", MAX_NAME_LEN - 1);
    strncpy(g_cards[1].description, "Loyalty Program", MAX_NAME_LEN - 1);
    g_cards[1].format = FORMAT_CODE128;
    g_cards[1].width = 0; g_cards[1].height = 0; g_cards[1].data_len = 0;

    strncpy(g_cards[2].name, "Library Card", MAX_NAME_LEN - 1);
    strncpy(g_cards[2].description, "Public Library", MAX_NAME_LEN - 1);
    g_cards[2].format = FORMAT_CODE128;
    g_cards[2].width = 0; g_cards[2].height = 0; g_cards[2].data_len = 0;

    strncpy(g_cards[3].name, "Demo Flight", MAX_NAME_LEN - 1);
    strncpy(g_cards[3].description, "JFK to LAX", MAX_NAME_LEN - 1);
    g_cards[3].format = FORMAT_QR;
    g_cards[3].width = 0; g_cards[3].height = 0; g_cards[3].data_len = 0;

    g_card_count = 4;
}

// Load demo card text data into g_active_bits as a null-terminated string
static bool load_demo_data(int index) {
    if (index < 0 || index >= 4) return false;
    int len = strlen(s_demo_data[index]);
    if (len >= MAX_BITS_LEN) len = MAX_BITS_LEN - 1;
    memcpy(g_active_bits, s_demo_data[index], len);
    g_active_bits[len] = '\0';
    return true;
}

// ============================================================================
// AppMessage Handling
// ============================================================================

// Persist a fully-reassembled card and refresh the menu.
static void finalize_rx_card(int i) {
    bool ok = storage_save_card(i, &g_cards[i], g_active_bits, s_rx_expected);
    if (i >= g_card_count) {
        g_card_count = i + 1;
        storage_save_count(g_card_count);
    }
    APP_LOG(ok ? APP_LOG_LEVEL_INFO : APP_LOG_LEVEL_WARNING,
            "Card %d: %s (%dx%d, %d bytes, fmt=%d)%s",
            i, g_cards[i].name, g_cards[i].width, g_cards[i].height,
            s_rx_expected, (int)g_cards[i].format,
            ok ? "" : " [STORAGE FULL - may be truncated]");
    s_rx_index = -1;
    s_rx_expected = 0;
    s_rx_received = 0;
    s_loading = false;
    menu_layer_reload_data(s_menu_layer);
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
    // 1. Sync start (clears watch for incoming sync)
    if (dict_find(iter, MESSAGE_KEY_CMD_SYNC_START)) {
        g_card_count = 0;
        storage_save_count(0);
        storage_wipe_all_cards();  // free orphaned data from a previous larger sync
        s_rx_index = -1;
        s_rx_expected = 0;
        s_rx_received = 0;
        s_loading = false;
        menu_layer_reload_data(s_menu_layer);
        return;
    }

    Tuple *t_idx = dict_find(iter, MESSAGE_KEY_KEY_INDEX);
    Tuple *t_len = dict_find(iter, MESSAGE_KEY_KEY_DATA_LEN);
    Tuple *t_off = dict_find(iter, MESSAGE_KEY_KEY_DATA_OFFSET);
    Tuple *t_data = dict_find(iter, MESSAGE_KEY_KEY_DATA);

    // 2. Card header: KEY_DATA_LEN present. Records metadata + opens reassembly.
    if (t_idx && t_len) {
        int i = t_idx->value->int32;
        if (i < 0 || i >= MAX_CARDS) return;

        Tuple *t_name = dict_find(iter, MESSAGE_KEY_KEY_NAME);
        Tuple *t_desc = dict_find(iter, MESSAGE_KEY_KEY_DESCRIPTION);
        Tuple *t_fmt = dict_find(iter, MESSAGE_KEY_KEY_FORMAT);
        Tuple *t_w = dict_find(iter, MESSAGE_KEY_KEY_WIDTH);
        Tuple *t_h = dict_find(iter, MESSAGE_KEY_KEY_HEIGHT);

        strncpy(g_cards[i].name, t_name ? t_name->value->cstring : "", MAX_NAME_LEN - 1);
        g_cards[i].name[MAX_NAME_LEN - 1] = '\0';
        strncpy(g_cards[i].description, t_desc ? t_desc->value->cstring : "", MAX_NAME_LEN - 1);
        g_cards[i].description[MAX_NAME_LEN - 1] = '\0';
        g_cards[i].format = t_fmt ? (BarcodeFormat)t_fmt->value->int32 : FORMAT_CODE128;
        g_cards[i].width = t_w ? t_w->value->int32 : 0;
        g_cards[i].height = t_h ? t_h->value->int32 : 0;

        int expected = t_len->value->int32;
        if (expected < 0) expected = 0;
        if (expected > MAX_BITS_LEN) expected = MAX_BITS_LEN;  // clamp to buffer
        g_cards[i].data_len = (uint16_t)expected;

        s_rx_index = i;
        s_rx_expected = expected;
        s_rx_received = 0;
        memset(g_active_bits, 0, MAX_BITS_LEN);

        if (expected == 0) {
            finalize_rx_card(i);  // metadata-only card (no barcode data)
        }
        return;
    }

    // 3. Data chunk: KEY_DATA_OFFSET + KEY_DATA. Written at offset into staging.
    if (t_idx && t_off && t_data) {
        int i = t_idx->value->int32;
        if (i != s_rx_index) return;  // header not seen / out of order — ignore

        int offset = t_off->value->int32;
        int len = (int)t_data->length;
        if (offset < 0 || offset >= MAX_BITS_LEN) return;
        if (offset + len > MAX_BITS_LEN) len = MAX_BITS_LEN - offset;
        if (len <= 0) return;

        memcpy(g_active_bits + offset, t_data->value->data, len);
        s_rx_received += len;

        // Chunks arrive in increasing-offset order, so completion = the final
        // chunk landing. Using offset (not a byte counter) is safe against a
        // duplicated chunk from an ack that was retried after the watch already
        // stored it — which would otherwise finalize early with a missing tail.
        if (offset + len >= s_rx_expected) {
            finalize_rx_card(i);
        }
        return;
    }

    // 4. Sync complete
    if (dict_find(iter, MESSAGE_KEY_CMD_SYNC_COMPLETE)) {
        APP_LOG(APP_LOG_LEVEL_INFO, "Sync complete: %d cards", g_card_count);
        return;
    }

    // 5. Config updated notification (legacy)
    if (dict_find(iter, MESSAGE_KEY_CONFIG_UPDATED)) {
        request_cards_from_phone(NULL);
    }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", (int)reason);
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox failed: %d", (int)reason);
}

// ============================================================================
// Detail Window (Barcode Display)
// ============================================================================

#define DETAIL_NAME_H 22   // top strip showing the card name

static void barcode_update_proc(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);

    // White background for the whole screen (incl. the name strip).
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    if (s_current_index >= 0 && s_current_index < g_card_count) {
        WalletCardInfo *info = &g_cards[s_current_index];

        // Card name at the top (so you can tell which card you're on while
        // cycling with up/down), barcode fills the area below it.
        graphics_context_set_text_color(ctx, GColorBlack);
        graphics_draw_text(ctx, info->name,
            fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
            GRect(2, -1, bounds.size.w - 4, DETAIL_NAME_H),
            GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

        GRect code_bounds = GRect(bounds.origin.x, bounds.origin.y + DETAIL_NAME_H,
                                  bounds.size.w, bounds.size.h - DETAIL_NAME_H);
        barcode_draw(ctx, code_bounds, info->format,
                     info->width, info->height, g_active_bits);
    }
}

static void load_current_card_data(void) {
    if (s_current_index >= 0 && s_current_index < g_card_count) {
        // Clear first: g_active_bits is shared with the sync reassembly buffer,
        // so wipe any stale bytes before loading this card.
        memset(g_active_bits, 0, MAX_BITS_LEN);

        // Demo cards carry no pre-rendered pixel data (width==0, data_len==0);
        // stage their raw text so the on-watch fallback renderer can draw them.
        WalletCardInfo *c = &g_cards[s_current_index];
        if (c->width == 0 && c->height == 0 && c->data_len == 0) {
            load_demo_data(s_current_index);
        } else {
            storage_load_card_data(s_current_index, g_active_bits, MAX_BITS_LEN);
        }
    }
}

static void detail_click_handler(ClickRecognizerRef recognizer, void *context) {
    if (g_card_count <= 1) return;

    ButtonId btn = click_recognizer_get_button_id(recognizer);
    if (btn == BUTTON_ID_DOWN) {
        s_current_index = (s_current_index + 1) % g_card_count;
    } else if (btn == BUTTON_ID_UP) {
        s_current_index = (s_current_index - 1 + g_card_count) % g_card_count;
    }

    load_current_card_data();
    layer_mark_dirty(s_barcode_layer);
}

static void detail_back_handler(ClickRecognizerRef recognizer, void *context) {
    window_stack_pop(true);
}

static void detail_config_provider(void *ctx) {
    window_single_click_subscribe(BUTTON_ID_UP, detail_click_handler);
    window_single_click_subscribe(BUTTON_ID_DOWN, detail_click_handler);
    window_single_click_subscribe(BUTTON_ID_BACK, detail_back_handler);
}

static void detail_window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    s_barcode_layer = layer_create(layer_get_bounds(root));
    layer_set_update_proc(s_barcode_layer, barcode_update_proc);
    layer_add_child(root, s_barcode_layer);
}

static void detail_window_unload(Window *window) {
    layer_destroy(s_barcode_layer);
    s_barcode_layer = NULL;
}

static void show_detail_window(int index) {
    s_current_index = index;

    // Load card data into g_active_bits before showing
    load_current_card_data();

    if (!s_detail_window) {
        s_detail_window = window_create();
        window_set_window_handlers(s_detail_window, (WindowHandlers){
            .load = detail_window_load,
            .unload = detail_window_unload
        });
        window_set_click_config_provider(s_detail_window, detail_config_provider);
    }

    window_stack_push(s_detail_window, true);
}

// ============================================================================
// Main Menu
// ============================================================================

static uint16_t menu_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *data) {
    if (s_loading) return 1;
    return (g_card_count > 0) ? g_card_count : 1;
}

static int16_t menu_get_cell_height(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
    return 50;
}

static void menu_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
    GRect bounds = layer_get_bounds(cell_layer);

    if (s_loading) {
        graphics_draw_text(ctx, "Loading cards...",
            fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
            GRect(5, bounds.size.h / 2 - 12, bounds.size.w - 10, 24),
            GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
        return;
    }

    if (g_card_count == 0) {
        menu_cell_basic_draw(ctx, cell_layer, "No Cards", "Add via Settings", NULL);
        return;
    }

    WalletCardInfo *c = &g_cards[cell_index->row];

    // Draw card name
    graphics_draw_text(ctx, c->name,
        fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
        GRect(5, 2, bounds.size.w - 10, 28),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    // Draw subtitle
    const char *subtitle;
    char fmt_subtitle[MAX_NAME_LEN];
    if (strlen(c->description) > 0) {
        subtitle = c->description;
    } else {
        static const char *fmt_names[] = {
            "Code 128", "Code 39", "EAN-13", "QR Code", "Aztec", "PDF417"
        };
        int fi = (int)c->format;
        if (fi >= 0 && fi <= 5) {
            snprintf(fmt_subtitle, sizeof(fmt_subtitle), "%s", fmt_names[fi]);
        } else {
            snprintf(fmt_subtitle, sizeof(fmt_subtitle), "Barcode");
        }
        subtitle = fmt_subtitle;
    }

    graphics_draw_text(ctx, subtitle,
        fonts_get_system_font(FONT_KEY_GOTHIC_14),
        GRect(5, 28, bounds.size.w - 10, 18),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void menu_select(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
    if (!s_loading && g_card_count > 0 && cell_index->row < (uint16_t)g_card_count) {
        show_detail_window(cell_index->row);
    }
}

static void main_window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    s_menu_layer = menu_layer_create(layer_get_bounds(root));
    menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks){
        .get_num_rows = menu_get_num_rows,
        .get_cell_height = menu_get_cell_height,
        .draw_row = menu_draw_row,
        .select_click = menu_select
    });
    menu_layer_set_click_config_onto_window(s_menu_layer, window);
    layer_add_child(root, menu_layer_get_layer(s_menu_layer));
}

static void main_window_unload(Window *window) {
    menu_layer_destroy(s_menu_layer);
}

// ============================================================================
// Request Cards / Timeout
// ============================================================================

static void request_cards_from_phone(void *data) {
    (void)data;
    DictionaryIterator *iter;
    AppMessageResult result = app_message_outbox_begin(&iter);
    if (result == APP_MSG_OK) {
        dict_write_uint8(iter, MESSAGE_KEY_REQUEST_CARDS, 1);
        app_message_outbox_send();
    }
}

static void loading_timeout(void *data) {
    if (s_loading) {
        s_loading = false;
        if (g_card_count == 0) {
            add_demo_cards();
        }
        menu_layer_reload_data(s_menu_layer);
    }
}

// ============================================================================
// Entry Point
// ============================================================================

static void init(void) {
    // Load persisted cards first
    storage_load_cards();
    if (g_card_count > 0) {
        s_loading = false;
    }

    // Set up AppMessage
    app_message_register_inbox_received(inbox_received_handler);
    app_message_register_inbox_dropped(inbox_dropped_callback);
    app_message_register_outbox_failed(outbox_failed_callback);
    app_message_open(2048, 256);

    // Create main window
    s_main_window = window_create();
    window_set_window_handlers(s_main_window, (WindowHandlers){
        .load = main_window_load,
        .unload = main_window_unload
    });
    window_stack_push(s_main_window, true);

    // Request fresh cards from phone
    app_timer_register(500, request_cards_from_phone, NULL);

    // Timeout: if no response and no persisted cards, show demos
    if (g_card_count == 0) {
        app_timer_register(3000, loading_timeout, NULL);
    }
}

static void deinit(void) {
    window_destroy(s_main_window);
    if (s_detail_window) window_destroy(s_detail_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
