# Pebble Wallet

Display your loyalty cards and passes as scannable barcodes on your Pebble smartwatch.

## Features

- Store up to 10 loyalty cards
- Display Code 128, Code 39, and EAN-13 barcodes
- Works on all Pebble models (Original, Time, Time Round, and new 2025+ models)
- Easy configuration via phone settings
- Barcodes are stored locally - no internet required after setup

## Supported Barcode Formats

| Format | Support | Common Uses |
|--------|---------|-------------|
| Code 128 | Full | Most loyalty cards, Starbucks, many retailers |
| Code 39 | Full | Older systems, some membership cards |
| EAN-13 | Full | Retail barcodes |
| QR Code | Limited | Not recommended due to display resolution |

## Building

### Prerequisites
- Pebble SDK installed (via Rebble: https://developer.rebble.io/sdk/)
- Python 3.13 (not yet compatible with 3.14)

### Build Commands

```bash
# Build for all platforms
pebble build

# Install to emulator
pebble install --emulator aplite    # Pebble Original (B&W)
pebble install --emulator basalt    # Pebble Time (color)
pebble install --emulator chalk     # Pebble Time Round

# Install to physical watch
pebble install --phone <ip_address>
```

### Cloud Development
Use Rebble's browser-based IDE: https://cloud.repebble.com

## Usage

1. **Install the app** on your Pebble watch
2. **Open Settings** on the Pebble phone app, select Pebble Wallet
3. **Add your cards**:
   - Enter the card name (e.g., "Starbucks")
   - Enter the barcode number (found on your physical card)
   - Select the barcode format (Code 128 is most common)
   - Click "Add Card"
4. **Save & Sync** to transfer cards to your watch
5. **On your watch**: Select a card to display its barcode

## How to Find Your Barcode Number

1. **Physical card**: Look for the number printed below the barcode
2. **Store app**: Many stores show the barcode number in their mobile app
3. **Barcode scanner app**: Use a barcode scanner app on your phone to scan your card

## Architecture

```
PebbleWallet/
├── package.json          # App metadata, message keys
├── wscript               # Build configuration
├── src/
│   ├── app.c             # Watch app (C) - UI and barcode rendering
│   └── pkjs/
│       └── index.js      # Phone app (JavaScript) - config and storage
└── resources/            # Images, fonts (if needed)
```

## Technical Notes

### Barcode Rendering
The app renders Code 128 barcodes directly on the watch using the Canvas-like graphics API. Barcode width is automatically calculated to fit the screen.

### Storage
Cards are stored in the phone's localStorage and sent to the watch via Bluetooth. The watch requests cards on app launch.

### Platform Differences
- **aplite** (Pebble Original/Steel): B&W display, smaller memory
- **basalt** (Pebble Time/Steel): Color display
- **chalk** (Pebble Time Round): Round display, slightly different layout

## Limitations

- **No NFC**: Pebble watches don't have NFC, so tap-to-pay is not possible
- **No Google Wallet integration**: Google Wallet API doesn't allow reading existing passes
- **QR codes**: Limited support due to display resolution constraints
- **Screen brightness**: E-paper displays may not scan well in low light

## Troubleshooting

### Barcode won't scan
- Increase screen brightness
- Hold watch steady at scanner
- Try adjusting distance from scanner
- Verify the barcode number is correct

### Cards not syncing
- Ensure Bluetooth is connected
- Open and close the app settings
- Restart the Pebble app on your phone

## Future Improvements

- [ ] Add card icons/colors
- [ ] Implement EAN-8 format
- [ ] Add search/filter for many cards
- [ ] Persistent storage on watch (survive app restart)
- [ ] Import from CSV file

## License

MIT License
