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
    var desc = (c.description !== undefined && c.description !== null) ? String(c.description) : '';
    var dict = {
        'KEY_INDEX': index,
        'KEY_NAME': (c.name || '').substring(0, 31),
        'KEY_DESCRIPTION': desc.substring(0, 31),
        'KEY_FORMAT': parseInt(c.format) || 0
    };

    // Parse pre-rendered "w,h,hex" data from config page
    var rawData = c.data || c.text || '';
    if (rawData.indexOf(',') > -1) {
        var parts = rawData.split(',');
        if (parts.length >= 3) {
            dict['KEY_WIDTH'] = parseInt(parts[0]);
            dict['KEY_HEIGHT'] = parseInt(parts[1]);
            // Send hex string directly (not byte array) for Core Devices compatibility
            dict['KEY_DATA'] = parts[2];
        }
    } else {
        // Plain text fallback (no pre-rendering)
        dict['KEY_WIDTH'] = 0;
        dict['KEY_HEIGHT'] = 0;
        dict['KEY_DATA'] = rawData;
    }

    console.log('Sending card ' + index + ': ' + c.name +
        ' desc="' + desc + '"' +
        ' w=' + dict['KEY_WIDTH'] + ' h=' + dict['KEY_HEIGHT'] +
        ' data_len=' + (dict['KEY_DATA'] ? dict['KEY_DATA'].length : 0) +
        ' format=' + dict['KEY_FORMAT']);

    Pebble.sendAppMessage(dict, function() {
        console.log('Card ' + index + ' sent OK');
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
