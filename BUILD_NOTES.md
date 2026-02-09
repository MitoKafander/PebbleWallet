# PebbleWallet Build Notes

## Current Status (Feb 8, 2026 - Session 4)
- **C code**: Compiles successfully for all platforms (aplite, basalt, chalk)
- **JavaScript bundling**: WORKING (as `pebble-js-app.js` — see enableMultiJS note below)
- **Barcode rendering**: WORKING - All barcodes scan correctly
- **QR code generation**: WORKING - Scans correctly (RS bug fixed Session 3)
- **Emulator testing**: PASSED
- **Physical watch**: WORKING - .pbw installed on Pebble Steel via Android phone
- **Config page**: WORKING - Opens correctly on Core Devices Pebble Android app (fixed Session 4)

## Build Environment
- Platform: cloud.repebble.com (GitHub Codespaces)
- Project folder: `/workspaces/codespaces-pebble/PebbleWallet/` (renamed from myfirstproject)

## Build Command
```bash
pebble build && pebble install --emulator basalt --logs --vnc
```

## Source Files

| File | Purpose | Lines |
|------|---------|-------|
| `src/app.c` | Watch app: UI, menu, barcode/QR rendering | ~770 |
| `src/qr_generate.h` | QR generator public API | ~15 |
| `src/qr_generate.c` | On-watch QR encoder (alphanumeric, v1-4, EC-L) | ~414 |
| `src/js/pebble-js-app.js` | Phone-side JS: config page, card storage, QR gen | ~645 |
| `package.json` | App metadata, 9 message keys | ~33 |
| `wscript` | Build config for Rebble Cloud SDK | ~39 |
| `config/index.html` | Hosted config page for GitHub Pages | ~120 |

## Barcode Implementation

### Code 128C (Numeric Data)
Used for all numeric-only data. Encodes digit pairs, making barcodes ~50% smaller.
- **Even-length numbers**: Encoded directly
- **Odd-length numbers**: Automatically padded with leading '0'

### Code 128B (Alphanumeric Data)
Fallback for data containing letters or special characters.

### Screen Layout
- 6px margins on left/right edges
- White background with proper quiet zones
- Barcode centered within available width
- Bounds checking prevents overflow

## QR Code Implementation

### Architecture
- Full QR encoder runs **on the watch in C** (no phone/server dependency)
- Located in `src/qr_generate.c` with API in `src/qr_generate.h`
- Single public function: `bool qr_generate(const char *data, uint8_t *out_matrix, uint8_t *out_size)`
- Lazy generation: QR matrix computed on first display, then cached in Card struct

### Capabilities
- Alphanumeric mode (0-9, A-Z, space, $%*+-./:)
- QR versions 1-4 (21x21 to 33x33 modules)
- Error correction level L
- Reed-Solomon with precomputed GF(256) tables
- Auto-uppercases input
- Mask pattern 0 (checkerboard)

### Memory Impact
- ~600 bytes ROM for lookup tables (GF exp/log, RS generators, alphanum map)
- ~2-3 KB ROM for code
- ~1.4 KB peak stack during generation (working matrix + buffers)
- Zero additional static RAM

### Integration
- `draw_qr_code()` in `app.c` calls `qr_generate()` when `card->qr_size == 0`
- Result cached in `card->qr_matrix[]` and `card->qr_size`
- Phone-side QR data (if sent) takes priority (qr_size already set)

## Config Page Architecture

### Current Approach (hosted on GitHub Pages)
- `config/index.html` hosted at `https://mitokafander.github.io/PebbleWallet/config/`
- `pebble-js-app.js` opens this URL via `Pebble.openURL()` with cards passed in URL hash
- Config page uses `pebblejs://close#` URL scheme to return data to PebbleKit JS
- Cards stored in phone's `localStorage`, synced to watch via AppMessage

### Config Page Features
- Add cards with name, barcode number, and format selection
- Delete existing cards
- Dynamic DOM rendering (no page reload)
- Supports Code 128, Code 39, EAN-13, and QR Code formats
- Max 10 cards

### Sync Flow
1. User opens settings in Pebble app on phone
2. `showConfiguration` event fires in PebbleKit JS
3. JS opens hosted config URL with existing cards in hash
4. User adds/removes cards, hits "Save & Sync"
5. Page navigates to `pebblejs://close#<cards_json>`
6. `webviewclosed` event fires, JS saves cards and sends them to watch
7. Watch receives cards via AppMessage (CARD_COUNT, then each card with delay)

## Test Results

### Barcodes
| Card | Original Data | Scanned Result | Status |
|------|--------------|----------------|--------|
| Starbucks | 6035550123456789 | 6035550123456789 | PASS |
| Target Circle | 4012345678901 | 04012345678901 | PASS (padded) |
| Library Card | 29857341 | 29857341 | PASS |

### QR Codes
| Card | Data | Version | Status |
|------|------|---------|--------|
| Demo Flight | M1DOE/JOHN E ABC123 JFKLAX | v2 (25x25) | PASS - scans correctly |
| HELLO (test) | HELLO | v1 (21x21) | PASS - verified against Python qrcode library |

## wscript Configuration (CRITICAL)

The Rebble Cloud SDK requires `binaries` as a list of dicts. Do NOT use `js_entry_file` — it produces `index.js` in the PBW, which the Core Devices app cannot find (it looks for `pebble-js-app.js`):

```python
def build(ctx):
    ctx.load('pebble_sdk')
    binaries = []
    for p in ctx.env.TARGET_PLATFORMS:
        ctx.set_env(ctx.all_envs[p])
        ctx.set_group(ctx.env.PLATFORM_NAME)
        app_elf = '{}/pebble-app.elf'.format(ctx.env.BUILD_DIR)
        ctx.pbl_program(source=ctx.path.ant_glob('src/**/*.c'), target=app_elf)
        binaries.append({'platform': p, 'app_elf': app_elf})
    ctx.set_group('bundle')
    ctx(features='bundle',
        binaries=binaries,
        js=ctx.path.ant_glob('src/js/**/*.js'))
```

## Memory Usage (from successful compile)
- CHALK: 5587 bytes RAM, 59949 bytes free heap
- BASALT: 5587 bytes RAM, 59949 bytes free heap
- APLITE: 5587 bytes RAM, 18989 bytes free heap

## File Locations

### Local (Mac)
- `/Users/mitokafander/Documents/AI Projects/Pebble apps/PebbleWallet/`

### Rebble Cloud
- `/workspaces/codespaces-pebble/PebbleWallet/` (renamed from myfirstproject)

### GitHub Pages (config page)
- Repo: `https://github.com/MitoKafander/PebbleWallet`
- Config URL: `https://mitokafander.github.io/PebbleWallet/config/`

## Bugs Fixed

### Session 2 (Feb 6, 2026 - Claude)
1. **`module.exports = QRCode;` in index.js** — Gemini added Node.js line that crashes PebbleKit JS
2. **Timer callback signature mismatch** — `request_cards_from_phone` now accepts `void *data`
3. **Barcode window memory leak** — old window destroyed before creating new one
4. **Removed temp QR debug IIFE** from index.js
5. **Removed unused `CARD_TYPE`** from package.json messageKeys
6. **Deleted 3 redundant Gemini utility files**

### Session 3 (Feb 6, 2026 - Claude)
7. **Reed-Solomon generator polynomials (CRITICAL)** — All 4 RS generators stored alpha exponents instead of GF(256) element values. `gf_mul()` treats values as elements, so every EC codeword was wrong, making QR codes unscannable. Fixed in BOTH `qr_generate.c` AND `index.js`.
8. **Config page `addCard()` reload bug** — `location.reload()` on a data URL lost newly added cards. Replaced with dynamic DOM rendering via `renderCards()`.
9. **Config page delete behavior** — `deleteCard()` no longer force-closes the page.
10. **Config page sync flow** — `webviewclosed` now calls `sendCardsToWatch()` directly.
11. **Config page hosting** — Moved from inline data URL to hosted GitHub Pages HTML file.

### Session 4 (Feb 8, 2026 - Claude)
12. **Config page not opening (ROOT CAUSE FOUND)** — `enableMultiJS: true` caused SDK to bundle JS as `index.js` in the PBW. Core Devices app's `DiskUtil.kt` checks for `pebble-js-app.js` to set `hasPKJS`. With `index.js`, `hasPKJS` was `false`, JS runtime never initialized, config never worked. Fixed by setting `enableMultiJS: false`, moving JS to `src/js/pebble-js-app.js`, and removing `js_entry_file` from wscript.

## CRITICAL: enableMultiJS Must Be false

**Root cause of config page not opening (resolved Session 4):**

The Core Devices Pebble app's `DiskUtil.kt` checks for a file named `pebble-js-app.js` inside the PBW archive to determine if PebbleKit JS exists (`pkjsFileExists`). With `enableMultiJS: true` in `package.json`, the Rebble Cloud SDK bundles JS as `index.js` instead of `pebble-js-app.js`. This caused `hasPKJS` to return `false`, so the JS runtime was never initialized and `showConfiguration` never fired.

**The fix (3 changes):**
1. `package.json`: Set `"enableMultiJS": false`
2. Move JS: `src/pkjs/index.js` → `src/js/pebble-js-app.js`
3. `wscript`: Change glob to `src/js/**/*.js` and remove `js_entry_file` parameter

**Verification:** `unzip -l build/*.pbw | grep js` must show `pebble-js-app.js` (not `index.js`).

## Known Issues (Reference)

- **AppMessage wedge (Issue #90)**: JS messaging pipeline can get stuck. Workaround: force-close Pebble app.
- **See CONFIG_DEBUG.md** for Core Devices mobileapp source analysis and debugging reference.

## Next Steps
1. Test config page end-to-end: add cards via config, save, verify sync to watch
2. Test on aplite emulator for memory constraints
3. Test more QR data at different version sizes
4. Optional: Publish to Rebble App Store at https://dev-portal.rebble.io/

## Session History
- **Session 1 (Nov 2025)**: Created project structure, Code 128 barcodes working
- **Gemini session (Feb 6, 2026)**: Failed to resolve config page (7 attempts)
- **Session 2 (Feb 6, 2026, Claude)**: Fixed 6 Gemini bugs, implemented on-watch QR generation in C
- **Session 3 (Feb 6, 2026, Claude)**: Fixed RS polynomial bug (QR now scans), fixed config page JS bugs, created hosted config page, tested on physical Pebble Steel. Config page blocked by Core Devices Pebble Android app.
- **Session 4 (Feb 8, 2026, Claude)**: Identified companion app as Core Devices Pebble (not Rebble). Researched mobileapp source code on GitHub. **Found root cause**: `enableMultiJS: true` produces `index.js` in PBW, but Core Devices app looks for `pebble-js-app.js`. Fixed by setting `enableMultiJS: false`, moving JS to `src/js/pebble-js-app.js`, updating wscript. Config page now works.
