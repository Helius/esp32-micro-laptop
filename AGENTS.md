# AGENTS.md — microlaptop firmware

## Project

ESP32-C6 firmware for a small "microlaptop" with a 2.4" SPI TFT.
The current sketch is `esp32-micro-laptop.ino` (Arduino sketch folder name
**must** match the `.ino` name).

## Hardware

- **MCU:** ESP32-C6 (Arduino core `esp32:esp32:esp32c6`, core 3.3.12)
- **Display:** T240B7-C10-25 — ShenZhen GoldenMorning, 2.4" IPS TFT
  - 240 × 320 pixels, ST7789 controller, 4-wire SPI
  - 262K colors, normally black, 10-pin FPC
  - Datasheet: https://www.compel.ru/item-pdf/f3a5393f33faf9c615d816b0b70a37cf/pn/gldmrn~t240b7-c10-25.pdf
- **Backlight:** LEDA / LEDK, VF ≈ 3.1 V, IF ≈ 80 mA
- **Built-in LED:** addressable RGB LED (WS2812) on GPIO 8, driven via
  `rgbLedWrite(RGB_BUILTIN, r, g, b)` (0..255 per channel). Used as a slow
  rainbow cycle at ~10% brightness (max value 25/255).

## Wiring (confirmed working)

| Display pin | Name  | ESP32-C6 |
|-------------|-------|----------|
| 1  | GND   | GND |
| 2  | DC    | GPIO 2 |
| 3  | CS    | GPIO 3 |
| 4  | SCL   | GPIO 6 |
| 5  | SDA   | GPIO 5 |
| 6  | RESET | GPIO 1 |
| 7  | VCC   | 3.3 V |
| 8  | GND   | GND |
| 9  | LEDA  | 3.3 V (4.7 Ω recommended) |
| 10 | LEDK  | GND |

Important:

- **VCC (pin 7) powers the controller.** The backlight is a separate LED
  circuit (LEDA/LEDK), so a lit backlight with a black screen usually means the
  controller is unpowered or GND is missing.
- Connect **both** GND pins (1 and 8).
- Do not confuse pin 1 (GND) with VCC. A short between 3.3 V and GND was the
  original cause of a dead display.
- Backlight: direct 3.3 V works but overdrives it. 22 Ω was too dim; use
  **~4.7 Ω 1/4 W** in series with LEDA for full brightness.
- GPIOs 1, 2, 3, 5, 6 are used by the display. GPIO 8 is the onboard RGB LED.

## Software setup

Installed libraries:

- **TFT_eSPI 2.5.43** — with ESP32-C6 support from PR #3399
  (https://github.com/Bodmer/TFT_eSPI/pull/3399). The PR routes C6 through
  the C3 processor files (`TFT_eSPI_ESP32_C3.*`). Two fixes were applied to
  `Processors/TFT_eSPI_ESP32_C3.h`:
  - Removed a duplicate `CONFIG_IDF_TARGET` guard with a lowercase `ESP32c6`
    typo that misidentified C6 as plain ESP32.
  - Removed C6 from the `TFT_MISO = TFT_MOSI` condition. The PR aliased
    `TFT_MISO` to `TFT_MOSI` (GPIO 5) on C6 when `TFT_MISO == -1`, but this
    display is write-only (MISO not connected). The alias made `spi.begin()`
    configure GPIO 5 as an input (MISO) conflicting with MOSI output — no data
    reached the display (black screen).
- TFT_eSPI is configured per-sketch via `tft_setup.h` (sits next to the
  `.ino` and is auto-included by TFT_eSPI's `__has_include` mechanism).
- **SimpleTui** (repo `../SimpleTui`) — header-only text UI installed the
  standard Arduino way: `~/Arduino/libraries/SimpleTui` is a symlink to the
  repo, so `#include <SimpleTui.h>` resolves through the library manager. No
  copy lives in the sketch folder. It is driven by a `stui::Display` backend
  (`TftDisplay` in the sketch).
- **IBM Plex Mono 16px smooth font** — `IBMPlexMono16.h` (174 glyphs: ASCII,
  Cyrillic, punctuation and box-drawing `─│┌┐└┘├┤┬┴┼`). Loaded with
  `tft.loadFont(IBMPlexMono16)`; this requires `#define SMOOTH_FONT` in
  `tft_setup.h`. The font has no `…` glyph and no space glyph (spaces use
  `gFont.spaceWidth` = 5 px, narrower than a cell).

Build / flash / monitor:

```sh
./brun.sh
```

or manually:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32c6:CDCOnBoot=cdc --build-path ./build
arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32c6 --input-dir ./build
arduino-cli monitor -p /dev/ttyACM0 --config baudrate=115200
```

Note: the serial port is `/dev/ttyACM0`. Setup-time serial prints often happen
before the monitor attaches; a `stty -F /dev/ttyACM0 115200 raw -echo` +
`cat /dev/ttyACM0` is useful for checking boot output.

## Display initialization (known good)

```cpp
#include "tft_setup.h"          // per-sketch config (pins, driver, SPI clock)
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

// In setup():
tft.init();
tft.setRotation(3);            // landscape, 320 x 240, upright
tft.fillScreen(TFT_BLACK);
```

- TFT_eSPI is configured via `tft_setup.h` in the sketch folder (pins, driver,
  SPI clock, FSPI host). No `SPI.begin()` call is needed — TFT_eSPI handles SPI
  init itself from those defines.
- On ESP32-C6 the only general-purpose SPI controller is **SPI2 (FSPI/GPSPI)**.
  TFT_eSPI's C3/C6 backend defines `SPI_PORT=2` and `VSPI=FSPI` for register
  and Arduino-SPI access, both resolving to GPSPI2.
- `setRotation(3)` gives the current landscape orientation (320 × 240) with the
  panel mounted as it is. `setRotation(1)` is the opposite 180° landscape.
- If the image is offset, adjust the panel offsets after `init()`.

## Graphics performance / flicker

- SPI clock is set in `tft_setup.h` (`SPI_FREQUENCY`, 16 MHz by default). It is
  reliable with this wiring.
- The UI is a character grid: each `stui::Display::print(x, y, str)` call
  fills one cell with the current background color and draws one glyph.
  `SimpleTui` redraws the whole screen every 500 ms (`redraw_interval_ms`),
  and `Text` clears its previous cells with spaces before repainting, so a
  naive per-cell `fillRect` + glyph costs ~190 ms per frame and flickers.
- `TftDisplay` therefore double-buffers on the cell level: `print()` only
  writes into a `pending_` grid, and `flush()` (called right after every
  `tui.loop()` and after `setScreen`) compares it with `shown_` and paints
  only the changed cells. A normal update touches 1–2 cells (~1.5 ms);
  static content is not repainted at all.
- `fillRect` + glyph per changed cell (rather than the font's background fill)
  keeps every cell exactly `cellW × cellH`, which matters because the smoothed
  space glyph only advances 5 px.
- For a future full-screen animation, an off-screen `TFT_eSprite` pushed in
  one SPI transfer is still the flicker-free option for large redraws.

## Current screen

`esp32-micro-laptop.ino` shows the SimpleTui dashboard from
`../SimpleTui/src/main.cpp`:

- Header row: `[*   ]  [bat:78%] [wifi:-66dBm]` (spacing 1 to fit 32 cols)
- Left column: battery / Wi-Fi / boxed `x,y: 0,0`
- Right column (green border on blue): live `dist: N m`, temperature,
  pressure, humidity, UV, uptime
- The `dist` value is a `char distance[12]` buffer refreshed every 500 ms;
  `Row::render` re-measures so the text shifts as the number grows
- Cell grid: `cellW = 10 px` (`gxAdvance` of the monospace font),
  `cellH = 22 px` (`fontHeight()` = 16 ascent + 6 max descent) →
  **32 × 10 cells** (`SimpleTui<32, 10>`)
- Boot color self-test: red → green → blue → white (300 ms each)
- Built-in RGB LED: slow rainbow cycle at ~10% brightness

## Useful defaults

- Landscape size: `screenW = tft.width()` = 320, `screenH = tft.height()` = 240
  after `setRotation(3)`.
- Colors are RGB565; `tft.color565(r, g, b)` is available.
- `TFT_BLACK`, `TFT_WHITE`, `TFT_RED`, `TFT_GREEN`, `TFT_BLUE`, `TFT_CYAN`
  are defined by TFT_eSPI.
- Text: `setTextSize(2)`, 6×8 font → 12×16 px per character.
