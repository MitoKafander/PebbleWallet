/**
 * Pebble Wallet - Phone-side JavaScript
 * Handles configuration page and binary card sync.
 * All barcode encoding is done by bwip-js in the config page.
 * Cards arrive as {name, description, data: "w,h,hex", text: "original", format}.
 */

var CONFIG_URL = 'https://mitokafander.github.io/PebbleWallet/config/';
// Bump on every release so the phone webview loads the latest config page
// instead of a cached copy (the config page holds the barcode encoder).
var CONFIG_VERSION = '2.4.1';

var MAX_TEXT_LEN = 255;   // must match MAX_TEXT_LEN in common.h

function loadCards() {
    try {
        return JSON.parse(localStorage.getItem('pebble_wallet_cards') || '[]');
    } catch (e) { return []; }
}

function saveCards(cards) {
    localStorage.setItem('pebble_wallet_cards', JSON.stringify(cards));
}

// --- Bitmap Optimization ---

// Remove whitespace borders from pre-rendered barcode bitmap.
// Uses continuous bit packing (matching config page output and C reader).
function cropBitmap(width, height, hex) {
    var totalBits = width * height;
    var totalBytes = Math.ceil(totalBits / 8);

    // Parse hex to bytes
    var bytes = [];
    for (var i = 0; i < hex.length; i += 2) {
        bytes.push(parseInt(hex.substr(i, 2), 16));
    }
    if (bytes.length < totalBytes) {
        return { width: width, height: height, hex: hex };
    }

    // Scan for bounding box of black pixels using continuous indexing
    var minX = width, maxX = 0, minY = height, maxY = 0;
    var found = false;
    for (var y = 0; y < height; y++) {
        for (var x = 0; x < width; x++) {
            var bitIdx = y * width + x;
            var byteIdx = Math.floor(bitIdx / 8);
            var bitPos = 7 - (bitIdx % 8);
            if ((bytes[byteIdx] >> bitPos) & 1) {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
                found = true;
            }
        }
    }

    if (!found) {
        return { width: width, height: height, hex: hex };
    }

    var newW = maxX - minX + 1;
    var newH = maxY - minY + 1;
    var newTotalBits = newW * newH;
    var newTotalBytes = Math.ceil(newTotalBits / 8);
    var newBytes = [];
    for (var k = 0; k < newTotalBytes; k++) newBytes.push(0);

    // Copy cropped region using continuous bit packing
    for (var y = 0; y < newH; y++) {
        for (var x = 0; x < newW; x++) {
            var srcBitIdx = (y + minY) * width + (x + minX);
            var srcByteIdx = Math.floor(srcBitIdx / 8);
            var srcBitPos = 7 - (srcBitIdx % 8);
            if ((bytes[srcByteIdx] >> srcBitPos) & 1) {
                var dstBitIdx = y * newW + x;
                var dstByteIdx = Math.floor(dstBitIdx / 8);
                var dstBitPos = 7 - (dstBitIdx % 8);
                newBytes[dstByteIdx] |= (1 << dstBitPos);
            }
        }
    }

    var newHex = '';
    for (var i = 0; i < newBytes.length; i++) {
        var h = newBytes[i].toString(16);
        if (h.length < 2) h = '0' + h;
        newHex += h;
    }

    return { width: newW, height: newH, hex: newHex };
}

// --- Sync Protocol ---
//
// Large barcodes (e.g. PDF417 boarding passes) no longer have to fit in one
// AppMessage. Each card is streamed as:
//   1. a header  { KEY_INDEX, KEY_NAME, KEY_DESCRIPTION, KEY_FORMAT,
//                  KEY_WIDTH, KEY_HEIGHT, KEY_DATA_LEN }
//   2. N chunks  { KEY_INDEX, KEY_DATA_OFFSET, KEY_DATA(<=80 bytes) }
// The watch reassembles the chunks by offset (see main.c).

var CHUNK_SIZE = 80;            // bytes of pixel data per AppMessage
var MAX_CARD_BYTES = 1400;      // must match MAX_BITS_LEN in common.h
var STORAGE_BUDGET = 3900;      // Pebble persist is ~4KB/app; keep a safety margin
var PER_CARD_OVERHEAD = 80;     // approx bytes the WalletCardInfo struct persists

// Turn a card into { width, height, bytes[] } using the config page's "w,h,hex".
function cardToMatrix(c) {
    var rawData = c.data || c.text || '';
    if (rawData.indexOf(',') === -1) {
        return { width: 0, height: 0, bytes: [] };
    }
    var parts = rawData.split(',');
    var opt = cropBitmap(parseInt(parts[0]), parseInt(parts[1]), parts[2]);
    var bytes = [];
    for (var i = 0; i < opt.hex.length; i += 2) {
        bytes.push(parseInt(opt.hex.substr(i, 2), 16));
    }
    if (bytes.length > MAX_CARD_BYTES) {
        // Too big for the watch buffer. Sending a truncated matrix would render
        // a corrupt, unscannable partial — send nothing instead so the card is
        // clearly blank ("Resync from phone") rather than misleadingly wrong.
        return { width: 0, height: 0, bytes: [], oversize: true };
    }
    return { width: opt.width, height: opt.height, bytes: bytes, oversize: false };
}

// Send a queue of AppMessages one at a time, retrying each up to 5 times.
function sendQueue(queue, idx, retries, onDone) {
    if (idx >= queue.length) { if (onDone) onDone(); return; }
    Pebble.sendAppMessage(queue[idx], function() {
        setTimeout(function() { sendQueue(queue, idx + 1, 0, onDone); }, 60);
    }, function(e) {
        if (retries < 5) {
            setTimeout(function() { sendQueue(queue, idx, retries + 1, onDone); }, 200);
        } else {
            console.log('Sync aborted at message ' + idx + ': ' + JSON.stringify(e));
        }
    });
}

function syncToWatch(cards) {
    console.log('Syncing ' + cards.length + ' cards to watch');
    var queue = [{ 'CMD_SYNC_START': 1 }];
    var projected = 16;  // small global overhead

    var synced = 0, dropped = 0;
    for (var index = 0; index < cards.length && synced < 10; index++) {
        var c = cards[index];
        var m = cardToMatrix(c);
        // The raw text rides in the header so the watch can show it on demand.
        var cardText = (c.text || '').substring(0, MAX_TEXT_LEN);
        var cost = PER_CARD_OVERHEAD + m.bytes.length + cardText.length;

        // Blocking budget guard: never queue a card that would push the watch
        // past its ~4KB persist limit — a partially-persisted card renders as a
        // truncated, unscannable barcode. Skip it (and warn) instead.
        if (projected + cost > STORAGE_BUDGET) {
            dropped++;
            console.log('Storage budget reached (' + projected + '/' + STORAGE_BUDGET +
                ' bytes) — skipping card "' + c.name + '".');
            continue;
        }
        projected += cost;

        queue.push({
            'KEY_INDEX': synced,
            'KEY_NAME': c.name || '',
            'KEY_DESCRIPTION': c.description || '',
            'KEY_FORMAT': parseInt(c.format) || 0,
            'KEY_WIDTH': m.width,
            'KEY_HEIGHT': m.height,
            'KEY_DATA_LEN': m.bytes.length,
            'KEY_TEXT': cardText
        });

        for (var off = 0; off < m.bytes.length; off += CHUNK_SIZE) {
            queue.push({
                'KEY_INDEX': synced,
                'KEY_DATA_OFFSET': off,
                'KEY_DATA': m.bytes.slice(off, off + CHUNK_SIZE)
            });
        }
        if (m.oversize) {
            console.log('WARNING: card "' + c.name + '" barcode is too large for the ' +
                'watch (>' + MAX_CARD_BYTES + ' bytes) — sent blank. Use fewer characters ' +
                'or a denser format.');
        }
        console.log('Queued card ' + synced + ': ' + c.name +
            ' ' + m.width + 'x' + m.height + ' (' + m.bytes.length + ' bytes)');
        synced++;
    }

    queue.push({ 'CMD_SYNC_COMPLETE': 1 });

    if (dropped > 0) {
        console.log('NOTE: ' + dropped + ' card(s) did not fit in Pebble storage and were skipped.');
    }

    sendQueue(queue, 0, 0, function() { console.log('Sync complete (' + synced + ' cards)'); });
}

// --- Events ---

Pebble.addEventListener('ready', function() {
    console.log('Pebble Wallet JS ready');
});

Pebble.addEventListener('appmessage', function(event) {
    if (event.payload.REQUEST_CARDS) {
        console.log('Watch requested cards');
        syncToWatch(loadCards());
    }
});

Pebble.addEventListener('showConfiguration', function() {
    var cards = loadCards();
    var url = CONFIG_URL + '?v=' + CONFIG_VERSION + '#' + encodeURIComponent(JSON.stringify(cards));
    console.log('Opening config page');
    Pebble.openURL(url);
});

Pebble.addEventListener('webviewclosed', function(e) {
    if (!e.response || e.response === 'CANCELLED') return;
    try {
        var cards = JSON.parse(decodeURIComponent(e.response));
        console.log('Config returned ' + cards.length + ' cards');
        saveCards(cards);
        syncToWatch(cards);
    } catch (err) {
        console.log('Config parse error: ' + err);
    }
});
