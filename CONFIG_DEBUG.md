# Config Page Debugging Reference

## STATUS: RESOLVED (Session 4, Feb 8, 2026)

**Root cause**: `enableMultiJS: true` in `package.json` caused the Rebble Cloud SDK to bundle JS as `index.js` in the PBW. The Core Devices Pebble app's `DiskUtil.kt` checks for `pebble-js-app.js` (not `index.js`) to determine if PebbleKit JS exists. With `index.js`, `hasPKJS` returned `false`, the JS runtime was never initialized, and `showConfiguration` never fired.

**Fix applied (3 changes):**
1. `package.json`: `"enableMultiJS": false`
2. JS file moved: `src/pkjs/index.js` → `src/js/pebble-js-app.js`
3. `wscript`: JS glob changed to `src/js/**/*.js`, removed `js_entry_file` parameter

**Key source code reference**: `DiskUtil.kt` in the mobileapp repo:
```kotlin
val pkjsFileExists = openZip(pbwPath).exists("pebble-js-app.js".toPath())
```

---

## Companion App: Core Devices Pebble App (NOT Rebble)

The companion app in use is the **new official Pebble app by Core Devices** (Eric Migicovsky's company):
- **Google Play**: `coredevices.coreapp`
- **App Store**: Pebble Core (id6743771967)
- **Source code**: https://github.com/coredevices/mobileapp
- Built on **libpebble3** (Kotlin multiplatform)
- Supports all old Pebble watches on firmware 3.x+ AND new Core 2 devices

## How Config Pages Work in the Core Devices App

Research from the mobileapp source code (Feb 2026):

### Architecture

The app has a full PebbleKit JS runtime running in an **Android WebView**. The config flow is:

1. User taps gear icon in the Pebble app
2. App calls `signalShowConfiguration()` in the WebView JS runtime
3. This dispatches the `showConfiguration` event to your app's PebbleKit JS code
4. Your JS calls `Pebble.openURL(configUrl)`
5. The URL goes through `PKJSInterface.openURL()` via `@JavascriptInterface` bridge
6. URL is sent through a `urlOpenRequests: Channel<String>` coroutine channel
7. `WatchappSettingsScreen.kt` receives URL and opens it in a config WebView
8. When config page navigates to `pebblejs://close#data`, the `SettingsRequestInterceptor` catches it
9. Data is extracted, URL-decoded, and passed back as `webviewclosed` event response

### Key Source Files in mobileapp repo

| File | Purpose |
|------|---------|
| `libpebble3/src/androidMain/assets/startup.js` | JS runtime bootstrap, defines `signalShowConfiguration()` |
| `libpebble3/src/androidMain/kotlin/.../js/WebViewJsRunner.kt` | Calls `signalShowConfiguration()` on WebView |
| `libpebble3/src/androidMain/kotlin/.../js/WebViewPKJSInterface.kt` | Public `@JavascriptInterface` bridge (openURL, etc.) |
| `libpebble3/src/androidMain/kotlin/.../js/WebViewPrivatePKJSInterface.kt` | Private bridge |
| `pebble/src/commonMain/kotlin/.../ui/WatchappSettingsScreen.kt` | Config WebView + close URL interception |
| `libpebble3/src/androidMain/kotlin/.../di/PKJSModule.android.kt` | DI wiring |

### Close URL Handling

The `SettingsRequestInterceptor` in `WatchappSettingsScreen.kt` uses this regex:

```kotlin
private val PREFIX = "pebblejs://close"
val closeUrlRegex = Regex("""^pebblejs://close(?:#|/\?|/)(.*?)$""")
```

This accepts three URL formats:
- `pebblejs://close#configData` (spec-compliant, **what we use**)
- `pebblejs://close/?configData` (non-compliant variant)
- `pebblejs://close/configData` (non-compliant variant)

**Our `pebblejs://close#` approach is correct.**

### Important: NOT the same as pypkjs

The **pypkjs** repo (Python PebbleKit JS, used for emulator) uses a different approach:
- It CANNOT intercept `pebblejs://close` links
- Instead uses a `return_to` query parameter with a local HTTP server
- This is **NOT** how the mobile app works — do not use `return_to`

## Known Issues in Core Devices App

### Issue #90: AppMessage Gets Wedged (OPEN)
- **Bug**: `signalAppMessageNack` in `startup.js` tries to access `transactionId` on undefined object
- **Effect**: JS messaging pipeline gets stuck; PebbleKit JS may stop responding
- **Impact on us**: If AppMessage wedges, `showConfiguration` may still fire but the JS runtime could be in a broken state
- **Workaround**: Force-close and reopen the Pebble app to reset JS runtime

### Issue #93: PebbleKitJS + PebbleKit2 Conflict (OPEN)
- Apps using both PebbleKit JS AND PebbleKit Android cannot work simultaneously
- Only one or the other is supported
- **Not relevant to us** (we only use PebbleKit JS)

### PR #56: Non-compliant Close URL Fix (MERGED Jan 2, 2026)
- Before this fix, only `pebblejs://close#` worked
- After fix, `pebblejs://close/?` and `pebblejs://close/` also work
- **Make sure app is updated** to get this fix

### TODO: Config Screen Cleanup
- `WebViewJsRunner.stop()` has a comment `//TODO: Close config screens`
- Config screens may not be properly cleaned up when JS runtime stops
- Could cause stale state issues

## Debugging Checklist

### 1. Verify JS is bundled as pebble-js-app.js in .pbw
```bash
# In Codespace:
cd /workspaces/codespaces-pebble/PebbleWallet/
unzip -l build/*.pbw | grep js
```
**MUST** show `pebble-js-app.js` in the archive. If it shows `index.js` instead, `enableMultiJS` is `true` — set it to `false` and place JS at `src/js/pebble-js-app.js`.

### 2. Verify package.json has configurable capability
```json
"capabilities": ["configurable"]
```
This tells the companion app to show the gear icon AND trigger showConfiguration.

### 3. Force-close and reopen Pebble app
Android: Settings > Apps > Pebble > Force Stop, then reopen.
This resets the JS runtime and clears any wedged AppMessage state (Issue #90).

### 4. Reinstall the .pbw
After force-closing the Pebble app:
1. Open the .pbw file on your phone
2. The Pebble app should prompt to install
3. This ensures a fresh JS bundle is loaded

### 5. Check Android logcat (if ADB available)
```bash
adb logcat | grep -i "pebble\|pkjs\|webview\|config\|showConfiguration\|openURL"
```
Look for:
- `signalShowConfiguration` being called
- `openURL` being called with the config URL
- Any JS errors or exceptions
- WebView loading errors

### 6. Update the Pebble app
Make sure you have the latest version from Google Play to get PR #56 fix.

### 7. Test with a minimal config page
Create a bare-bones test to isolate the issue:
```javascript
// Minimal showConfiguration handler
Pebble.addEventListener('showConfiguration', function() {
    Pebble.openURL('https://google.com');
});
```
If Google.com opens, the JS runtime works and the issue is with our config URL.
If nothing happens, the JS runtime isn't running or showConfiguration never fires.

## Current Config Page Code

### pebble-js-app.js showConfiguration handler (line ~611)
```javascript
Pebble.addEventListener('showConfiguration', function() {
    var cards = loadCards();
    var cardsHash = encodeURIComponent(JSON.stringify(cards));
    var configUrl = 'https://mitokafander.github.io/PebbleWallet/config/#' + cardsHash;
    Pebble.openURL(configUrl);
});
```

### pebble-js-app.js webviewclosed handler (line ~626)
```javascript
Pebble.addEventListener('webviewclosed', function(event) {
    if (event.response && event.response !== 'CANCELLED') {
        var cards = JSON.parse(decodeURIComponent(event.response));
        saveCards(cards);
        sendCardsToWatch();
    }
});
```

### config/index.html save function
```javascript
function saveAndClose() {
    var result = encodeURIComponent(JSON.stringify(cards));
    document.location = "pebblejs://close#" + result;
}
```

## References

- Core Devices mobileapp: https://github.com/coredevices/mobileapp
- Core Devices pypkjs: https://github.com/coredevices/pypkjs
- PR #56 (close URL fix): https://github.com/coredevices/mobileapp/pull/56
- Issue #90 (AppMessage wedge): https://github.com/coredevices/mobileapp/issues/90
- Issue #93 (PKJS + PK2 conflict): https://github.com/coredevices/mobileapp/issues/93
- PebbleKit JS docs: https://developer.repebble.com/guides/communication/using-pebblekit-js/
- App Configuration docs: https://developer.repebble.com/guides/user-interfaces/app-configuration-static/
- Pebble on Google Play: https://play.google.com/store/apps/details?id=coredevices.coreapp
- Eric Migicovsky's blog: https://ericmigi.com/
- Core Devices GitHub: https://github.com/coredevices
