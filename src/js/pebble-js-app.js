/**
 * Pebble Wallet - Phone-side JavaScript
 * Handles configuration page and card sync with watch.
 * QR generation is done on-watch (no JS QR generator needed).
 */

var CONFIG_URL = 'https://mitokafander.github.io/PebbleWallet/config/';

// --- Storage ---
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

    // Step 1: Send SYNC_START
    Pebble.sendAppMessage({'CMD_SYNC_START': 1}, function() {
        sendNextCard(cards, 0);
    }, function(e) {
        console.log('Sync start failed: ' + JSON.stringify(e));
    });
}

function sendNextCard(cards, index) {
    if (index >= cards.length) {
        // Step 3: Send SYNC_COMPLETE
        Pebble.sendAppMessage({'CMD_SYNC_COMPLETE': 1});
        console.log('Sync complete');
        return;
    }

    // Step 2: Send each card
    var c = cards[index];
    var dict = {
        'KEY_INDEX': index,
        'KEY_NAME': (c.name || '').substring(0, 31),
        'KEY_DESCRIPTION': (c.description || '').substring(0, 31),
        'KEY_DATA': (c.data || '').substring(0, 1023),
        'KEY_FORMAT': parseInt(c.format) || 0
    };

    console.log('Sending card ' + index + ': ' + c.name + ' (len=' + (c.data || '').length + ')');

    Pebble.sendAppMessage(dict, function() {
        sendNextCard(cards, index + 1);
    }, function(e) {
        console.log('Card ' + index + ' failed, retrying: ' + JSON.stringify(e));
        setTimeout(function() { sendNextCard(cards, index); }, 1000);
    });
}

// --- Legacy sync (for old config pages that don't use new protocol) ---
function sendCardsLegacy(cards) {
    syncToWatch(cards);
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
