/**
 * Pebble Wallet - Phone-side JavaScript
 * Handles configuration page and binary card sync.
 * All barcode encoding is done by bwip-js in the config page.
 * Cards arrive as {name, description, data: "w,h,hex", text: "original", format}.
 */

var CONFIG_URL = 'https://mitokafander.github.io/PebbleWallet/config/';

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

function syncToWatch(cards) {
    console.log('Syncing ' + cards.length + ' cards to watch');
    Pebble.sendAppMessage({'CMD_SYNC_START': 1}, function() {
        sendNextCard(cards, 0);
    }, function(e) {
        console.log('Sync start failed: ' + JSON.stringify(e));
    });
}

function sendNextCard(cards, index) {
    if (index >= cards.length) {
        Pebble.sendAppMessage({'CMD_SYNC_COMPLETE': 1});
        console.log('Sync complete');
        return;
    }

    var c = cards[index];
    var dict = {
        'KEY_INDEX': index,
        'KEY_NAME': c.name || '',
        'KEY_DESCRIPTION': c.description || '',
        'KEY_FORMAT': parseInt(c.format) || 0
    };

    // Parse pre-rendered "w,h,hex" data from config page
    var rawData = c.data || c.text || '';
    if (rawData.indexOf(',') > -1) {
        var parts = rawData.split(',');
        var rawW = parseInt(parts[0]);
        var rawH = parseInt(parts[1]);
        var hex = parts[2];

        // Crop whitespace borders to maximize scaling on watch
        var optimized = cropBitmap(rawW, rawH, hex);

        dict['KEY_WIDTH'] = optimized.width;
        dict['KEY_HEIGHT'] = optimized.height;
        var bytes = [];
        for (var i = 0; i < optimized.hex.length; i += 2) {
            bytes.push(parseInt(optimized.hex.substr(i, 2), 16));
        }
        dict['KEY_DATA'] = bytes;
    } else {
        dict['KEY_WIDTH'] = 0;
        dict['KEY_HEIGHT'] = 0;
        var bytes = [];
        for (var i = 0; i < rawData.length; i++) {
            bytes.push(rawData.charCodeAt(i));
        }
        dict['KEY_DATA'] = bytes;
    }

    console.log('Sending card ' + index + ': ' + c.name +
        ' w=' + dict['KEY_WIDTH'] + ' h=' + dict['KEY_HEIGHT'] +
        ' data=' + (dict['KEY_DATA'] ? dict['KEY_DATA'].length : 0) + ' bytes');

    Pebble.sendAppMessage(dict, function() {
        sendNextCard(cards, index + 1);
    }, function(e) {
        console.log('Card ' + index + ' failed, retrying: ' + JSON.stringify(e));
        setTimeout(function() { sendNextCard(cards, index); }, 1000);
    });
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
    var url = CONFIG_URL + '#' + encodeURIComponent(JSON.stringify(cards));
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
