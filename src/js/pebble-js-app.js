/**
 * Pebble Wallet - Phone-side JavaScript
 * Handles configuration page, card sync, and phone-side QR encoding
 * for Aztec/PDF417/large QR data that can't be generated on-watch.
 */

// ================================================================
// QR Code Encoder - Byte mode, Versions 1-10, EC Level L
// Returns pre-rendered matrix as "size,size,hexdata" string
// ================================================================
var QREncode = (function() {
    // GF(256) with primitive polynomial 0x11D
    var EXP = [], LOG = [];
    var x = 1;
    for (var i = 0; i < 256; i++) {
        EXP[i] = x; LOG[x] = i;
        x <<= 1; if (x & 256) x ^= 0x11D;
    }
    function gfMul(a, b) { return (a && b) ? EXP[(LOG[a] + LOG[b]) % 255] : 0; }

    // RS generator polynomial (descending order, monic)
    function rsGen(n) {
        var g = [1];
        for (var i = 0; i < n; i++) {
            var ng = [];
            for (var k = 0; k <= g.length; k++) ng[k] = 0;
            for (var j = 0; j < g.length; j++) {
                ng[j] ^= g[j];
                ng[j + 1] ^= gfMul(g[j], EXP[i]);
            }
            g = ng;
        }
        return g;
    }

    // RS encode: polynomial long division, returns n EC codewords
    function rsEncode(data, n) {
        var gen = rsGen(n);
        var msg = data.slice();
        for (var i = 0; i < n; i++) msg.push(0);
        for (var i = 0; i < data.length; i++) {
            var c = msg[i];
            if (c) for (var j = 0; j <= n; j++) msg[i + j] ^= gfMul(c, gen[j]);
        }
        return msg.slice(data.length);
    }

    // Version tables for EC Level L
    // [totalDataCW, ecPerBlock, g1Blocks, g1DataPerBlock, g2Blocks, g2DataPerBlock]
    var VER = [null,
        [19,7,1,19,0,0],[34,10,1,34,0,0],[55,15,1,55,0,0],[80,20,1,80,0,0],
        [108,26,1,108,0,0],[136,18,2,68,0,0],[156,20,2,78,0,0],
        [194,24,2,97,0,0],[232,30,2,116,0,0],[274,18,2,68,2,69]
    ];
    var CAP = [0, 17, 32, 53, 78, 106, 134, 154, 192, 230, 271];
    var ALIGN = [null,[],[6,18],[6,22],[6,26],[6,30],[6,34],
        [6,22,38],[6,24,42],[6,26,46],[6,28,50]];

    // Format info: EC Level L (01) + mask, BCH(15,5), XOR 0x5412
    function fmtBits(mask) {
        var d = (1 << 3) | mask;
        var r = d << 10;
        for (var i = 14; i >= 10; i--) if (r & (1 << i)) r ^= 0x537 << (i - 10);
        return ((d << 10) | r) ^ 0x5412;
    }

    // Version info: BCH(18,6) for versions 7+
    function verBits(ver) {
        var r = ver << 12;
        for (var i = 17; i >= 12; i--) if (r & (1 << i)) r ^= 0x1F25 << (i - 12);
        return (ver << 12) | r;
    }

    // Mask condition functions
    function maskCond(m, r, c) {
        switch (m) {
            case 0: return (r + c) % 2 === 0;
            case 1: return r % 2 === 0;
            case 2: return c % 3 === 0;
            case 3: return (r + c) % 3 === 0;
            case 4: return ((r >> 1) + Math.floor(c / 3)) % 2 === 0;
            case 5: return (r * c) % 2 + (r * c) % 3 === 0;
            case 6: return ((r * c) % 2 + (r * c) % 3) % 2 === 0;
            case 7: return ((r + c) % 2 + (r * c) % 3) % 2 === 0;
        }
    }

    // Penalty score for a masked matrix
    function penalty(m, sz) {
        var p = 0, r, c, j;
        // Rule 1: runs of 5+ same color in row/col
        for (r = 0; r < sz; r++) {
            var rr = 1, rc = 1;
            for (c = 1; c < sz; c++) {
                if (m[r][c] === m[r][c-1]) rr++; else { if (rr >= 5) p += rr - 2; rr = 1; }
                if (m[c][r] === m[c-1][r]) rc++; else { if (rc >= 5) p += rc - 2; rc = 1; }
            }
            if (rr >= 5) p += rr - 2;
            if (rc >= 5) p += rc - 2;
        }
        // Rule 2: 2x2 blocks
        for (r = 0; r < sz - 1; r++)
            for (c = 0; c < sz - 1; c++) {
                var v = m[r][c];
                if (v === m[r][c+1] && v === m[r+1][c] && v === m[r+1][c+1]) p += 3;
            }
        // Rule 3: finder-like patterns (1011101 with 4 white on either side)
        for (r = 0; r < sz; r++)
            for (j = 0; j <= sz - 11; j++) {
                if (m[r][j]===1&&m[r][j+1]===0&&m[r][j+2]===1&&m[r][j+3]===1&&m[r][j+4]===1&&
                    m[r][j+5]===0&&m[r][j+6]===1&&m[r][j+7]===0&&m[r][j+8]===0&&m[r][j+9]===0&&m[r][j+10]===0) p += 40;
                if (m[r][j]===0&&m[r][j+1]===0&&m[r][j+2]===0&&m[r][j+3]===0&&m[r][j+4]===1&&
                    m[r][j+5]===0&&m[r][j+6]===1&&m[r][j+7]===1&&m[r][j+8]===1&&m[r][j+9]===0&&m[r][j+10]===1) p += 40;
                if (m[j][r]===1&&m[j+1][r]===0&&m[j+2][r]===1&&m[j+3][r]===1&&m[j+4][r]===1&&
                    m[j+5][r]===0&&m[j+6][r]===1&&m[j+7][r]===0&&m[j+8][r]===0&&m[j+9][r]===0&&m[j+10][r]===0) p += 40;
                if (m[j][r]===0&&m[j+1][r]===0&&m[j+2][r]===0&&m[j+3][r]===0&&m[j+4][r]===1&&
                    m[j+5][r]===0&&m[j+6][r]===1&&m[j+7][r]===1&&m[j+8][r]===1&&m[j+9][r]===0&&m[j+10][r]===1) p += 40;
            }
        // Rule 4: dark/light balance
        var dark = 0;
        for (r = 0; r < sz; r++) for (c = 0; c < sz; c++) if (m[r][c]) dark++;
        p += Math.floor(Math.abs(dark * 100 / (sz * sz) - 50) / 5) * 10;
        return p;
    }

    // Main generate function
    function generate(text) {
        var bytes = [];
        for (var i = 0; i < text.length; i++) bytes.push(text.charCodeAt(i) & 0xFF);
        var len = bytes.length;

        // Find smallest version
        var ver = 0;
        for (var v = 1; v <= 10; v++) if (len <= CAP[v]) { ver = v; break; }
        if (!ver) return null;

        var vi = VER[ver], totalData = vi[0], ecPB = vi[1];
        var sz = ver * 4 + 17;

        // --- Encode data bitstream ---
        var bits = [];
        function addBits(val, n) { for (var i = n - 1; i >= 0; i--) bits.push((val >> i) & 1); }
        addBits(4, 4); // Byte mode
        addBits(len, ver <= 9 ? 8 : 16);
        for (var i = 0; i < len; i++) addBits(bytes[i], 8);
        var tLen = Math.min(4, totalData * 8 - bits.length);
        for (var i = 0; i < tLen; i++) bits.push(0);
        while (bits.length % 8) bits.push(0);
        var pad = [0xEC, 0x11], pi = 0;
        while (bits.length < totalData * 8) { addBits(pad[pi], 8); pi ^= 1; }

        // Convert to codewords
        var cw = [];
        for (var i = 0; i < bits.length; i += 8) {
            var b = 0;
            for (var j = 0; j < 8; j++) b = (b << 1) | bits[i + j];
            cw.push(b);
        }

        // --- Split into blocks, generate EC ---
        var blocks = [], ecBlocks = [], pos = 0;
        for (var g = 0; g < 2; g++) {
            var nb = vi[2 + g * 2], dpb = vi[3 + g * 2];
            for (var b = 0; b < nb; b++) {
                var block = cw.slice(pos, pos + dpb);
                pos += dpb;
                blocks.push(block);
                ecBlocks.push(rsEncode(block, ecPB));
            }
        }

        // --- Interleave ---
        var inter = [], maxD = 0;
        for (var b = 0; b < blocks.length; b++) if (blocks[b].length > maxD) maxD = blocks[b].length;
        for (var i = 0; i < maxD; i++)
            for (var b = 0; b < blocks.length; b++)
                if (i < blocks[b].length) inter.push(blocks[b][i]);
        for (var i = 0; i < ecPB; i++)
            for (var b = 0; b < ecBlocks.length; b++)
                inter.push(ecBlocks[b][i]);

        // --- Build matrix ---
        var mtx = [], isF = [];
        for (var r = 0; r < sz; r++) {
            mtx[r] = []; isF[r] = [];
            for (var c = 0; c < sz; c++) { mtx[r][c] = 0; isF[r][c] = false; }
        }
        function setF(r, c, v) {
            if (r >= 0 && r < sz && c >= 0 && c < sz) { mtx[r][c] = v; isF[r][c] = true; }
        }

        // Finder patterns + separators
        function placeFinder(row, col) {
            for (var r = -1; r <= 7; r++)
                for (var c = -1; c <= 7; c++) {
                    var rr = row + r, cc = col + c;
                    if (rr < 0 || rr >= sz || cc < 0 || cc >= sz) continue;
                    var v = (r >= 0 && r <= 6 && c >= 0 && c <= 6) &&
                            (r === 0 || r === 6 || c === 0 || c === 6 || (r >= 2 && r <= 4 && c >= 2 && c <= 4));
                    setF(rr, cc, v ? 1 : 0);
                }
        }
        placeFinder(0, 0);
        placeFinder(0, sz - 7);
        placeFinder(sz - 7, 0);

        // Alignment patterns
        var ap = ALIGN[ver];
        for (var ai = 0; ai < ap.length; ai++)
            for (var aj = 0; aj < ap.length; aj++) {
                var ar = ap[ai], ac = ap[aj];
                if (ar < 9 && ac < 9) continue;
                if (ar < 9 && ac > sz - 9) continue;
                if (ar > sz - 9 && ac < 9) continue;
                for (var dr = -2; dr <= 2; dr++)
                    for (var dc = -2; dc <= 2; dc++)
                        setF(ar + dr, ac + dc,
                            (Math.abs(dr) === 2 || Math.abs(dc) === 2 || (dr === 0 && dc === 0)) ? 1 : 0);
            }

        // Timing patterns
        for (var i = 8; i < sz - 8; i++) {
            setF(6, i, (i & 1) ? 0 : 1);
            setF(i, 6, (i & 1) ? 0 : 1);
        }

        // Dark module
        setF(sz - 8, 8, 1);

        // Reserve format info areas (2 copies, 15 bits each)
        for (var i = 0; i <= 5; i++) setF(8, i, 0);
        setF(8, 7, 0); setF(8, 8, 0); setF(7, 8, 0);
        for (var i = 9; i <= 14; i++) setF(14 - i, 8, 0);
        for (var i = 0; i < 8; i++) setF(sz - 1 - i, 8, 0);
        for (var i = 8; i < 15; i++) setF(8, sz - 15 + i, 0);

        // Reserve version info areas (v7+)
        if (ver >= 7) {
            for (var i = 0; i < 18; i++) {
                var a = sz - 11 + i % 3, b = Math.floor(i / 3);
                setF(a, b, 0);
                setF(b, a, 0);
            }
        }

        // --- Place data bits (zigzag) ---
        var dataBits = [];
        for (var i = 0; i < inter.length; i++)
            for (var b = 7; b >= 0; b--) dataBits.push((inter[i] >> b) & 1);

        var bi = 0;
        for (var right = sz - 1; right >= 1; right -= 2) {
            if (right === 6) right = 5;
            var upward = ((right + 1) & 2) === 0;
            for (var vert = 0; vert < sz; vert++) {
                var row = upward ? sz - 1 - vert : vert;
                for (var dc = 0; dc <= 1; dc++) {
                    var col = right - dc;
                    if (col < 0 || col >= sz || isF[row][col]) continue;
                    mtx[row][col] = bi < dataBits.length ? dataBits[bi++] : 0;
                }
            }
        }

        // --- Masking: try all 8, pick lowest penalty ---
        var bestMask = 0, bestPen = Infinity;
        for (var mask = 0; mask < 8; mask++) {
            // Build masked copy
            var tmp = [];
            for (var r = 0; r < sz; r++) {
                tmp[r] = mtx[r].slice();
                for (var c = 0; c < sz; c++)
                    if (!isF[r][c] && maskCond(mask, r, c)) tmp[r][c] ^= 1;
            }
            var p = penalty(tmp, sz);
            if (p < bestPen) { bestPen = p; bestMask = mask; }
        }

        // Apply best mask
        for (var r = 0; r < sz; r++)
            for (var c = 0; c < sz; c++)
                if (!isF[r][c] && maskCond(bestMask, r, c)) mtx[r][c] ^= 1;

        // --- Place format info ---
        var fmt = fmtBits(bestMask);
        // Copy 1
        for (var i = 0; i <= 5; i++) mtx[8][i] = (fmt >> i) & 1;
        mtx[8][7] = (fmt >> 6) & 1;
        mtx[8][8] = (fmt >> 7) & 1;
        mtx[7][8] = (fmt >> 8) & 1;
        for (var i = 9; i <= 14; i++) mtx[14 - i][8] = (fmt >> i) & 1;
        // Copy 2
        for (var i = 0; i < 8; i++) mtx[sz - 1 - i][8] = (fmt >> i) & 1;
        for (var i = 8; i < 15; i++) mtx[8][sz - 15 + i] = (fmt >> i) & 1;

        // --- Place version info (v7+) ---
        if (ver >= 7) {
            var vb = verBits(ver);
            for (var i = 0; i < 18; i++) {
                var bit = (vb >> i) & 1;
                var a = sz - 11 + i % 3, b = Math.floor(i / 3);
                mtx[a][b] = bit;
                mtx[b][a] = bit;
            }
        }

        // --- Convert to "size,size,hex" string ---
        var hex = sz + ',' + sz + ',';
        var nibble = 0, nbits = 0;
        for (var r = 0; r < sz; r++) {
            for (var c = 0; c < sz; c++) {
                nibble = (nibble << 1) | mtx[r][c];
                nbits++;
                if (nbits === 4) {
                    hex += nibble.toString(16).toUpperCase();
                    nibble = 0; nbits = 0;
                }
            }
        }
        if (nbits > 0) {
            nibble <<= (4 - nbits);
            hex += nibble.toString(16).toUpperCase();
        }
        return hex;
    }

    return { generate: generate };
})();

// ================================================================
// Card Sync and Configuration
// ================================================================

var CONFIG_URL = 'https://mitokafander.github.io/PebbleWallet/config/';

// Formats that need phone-side pre-rendering
var FORMAT_AZTEC = 4;
var FORMAT_PDF417 = 5;
var FORMAT_QR = 3;

function loadCards() {
    try {
        return JSON.parse(localStorage.getItem('pebble_wallet_cards') || '[]');
    } catch (e) { return []; }
}

function saveCards(cards) {
    localStorage.setItem('pebble_wallet_cards', JSON.stringify(cards));
}

// Pre-render card data if needed (Aztec, PDF417, or large QR)
function prepareCardData(card) {
    var fmt = parseInt(card.format) || 0;
    var data = card.data || '';

    // Aztec and PDF417: always pre-render as QR matrix on phone
    // QR with data > 50 chars: pre-render (on-watch only handles small alphanumeric)
    if (fmt === FORMAT_AZTEC || fmt === FORMAT_PDF417 ||
        (fmt === FORMAT_QR && data.length > 50)) {
        // Check if already pre-rendered ("w,h,hex" format)
        if (data.length > 0 && data[0] >= '0' && data[0] <= '9' && data.indexOf(',') !== -1) {
            return data; // Already pre-rendered
        }
        var rendered = QREncode.generate(data);
        if (rendered) {
            console.log('Pre-rendered ' + data.length + ' bytes -> ' + rendered.length + ' chars QR matrix');
            return rendered;
        }
        console.log('QR encode failed for ' + data.length + ' bytes, sending raw');
    }
    return data;
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
    var renderedData = prepareCardData(c);
    var dict = {
        'KEY_INDEX': index,
        'KEY_NAME': (c.name || '').substring(0, 31),
        'KEY_DESCRIPTION': (c.description || '').substring(0, 31),
        'KEY_DATA': renderedData.substring(0, 1023),
        'KEY_FORMAT': parseInt(c.format) || 0
    };

    console.log('Sending card ' + index + ': ' + c.name + ' (data=' + renderedData.length + ' chars)');

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
