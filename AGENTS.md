# AGENTS.md — microlaptop firmware

## Project

ESP32-C6 firmware for a small "microlaptop" with a 2.4" SPI TFT.
The current sketch is `firmware.ino` (Arduino sketch folder name **must** match
the `.ino` name, so keep it as `firmware/firmware.ino`).

## Hardware

- **MCU:** ESP32-C6 (Arduino core `esp32:esp32:esp32c6`, core 3.3.12)
- **Display:** T240B7-C10-25 — ShenZhen GoldenMorning, 2.4" IPS TFT
  - 240 × 320 pixels, ST7789 controller, 4-wire SPI
  - 262K colors, normally black, 10-pin FPC
  - Datasheet: https://www.compel.ru/item-pdf/f3a5393f33faf9c615d816b0b70a37cf/pn/gldmrn~t240b7-c10-25.pdf
- **Backlight:** LEDA / LEDK, VF ≈ 3.1 V, IF ≈ 80 mA
- **Built-in LED:** RGB LED on GPIO 8 (`LED_BUILTIN`), driven white by
  `digitalWrite(LED_BUILTIN, HIGH/LOW)`; used as a 1 Hz heartbeat.

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

- Adafruit GFX Library 1.13.0
- Adafruit ST7735 and ST7789 Library 1.12.0
- Adafruit BusIO 1.17.4

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
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

#define TFT_CS    3
#define TFT_DC    2
#define TFT_RST   1
#define TFT_SCLK  6
#define TFT_MOSI  5

Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);

// In setup():
SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);  // ESP32 SPI2/FSPI, custom pins
tft.init(240, 320);                          // native portrait resolution
tft.setRotation(3);                          // landscape, 320 x 240, upright
tft.fillScreen(ST77XX_BLACK);
```

- On ESP32-C6, Arduino's `SPI` object is **SPI2 (FSPI)**, not SPI0/SPI1.
  `FSPI` being defined as `0` in the core is just an enum value; SPI0/SPI1 are
  the flash/cache controllers.
- `setRotation(3)` gives the current landscape orientation (320 × 240) with the
  panel mounted as it is. `setRotation(1)` is the opposite 180° landscape.
- If the image is offset, adjust the panel offsets after `init()`.

## Graphics performance / flicker

- Default hardware SPI speed is 16 MHz. It is reliable with this wiring.
- Do **not** clear/redraw large areas line-by-line; it causes visible flicker.
- Current approach: render the animation into an off-screen `GFXcanvas16`
  and push it in one SPI transfer:

```cpp
GFXcanvas16 canvas(200, 200);   // ~80 KB in RAM

canvas.fillScreen(ST77XX_BLACK);
// draw with canvas.drawLine / canvas.fillCircle / canvas.print ...
tft.drawRGBBitmap(areaX, areaY, (uint16_t *)canvas.getBuffer(),
                  canvas.width(), canvas.height());
```

- A full 320×240 canvas is 153,600 bytes and also fits in the ~310 KB free
  heap, but the 200×200 cube region is faster.
- Frame rate is dominated by the SPI transfer: 200×200 at 16 MHz ≈ 40 ms
  (~20–25 fps). Raise `tft.setSPISpeed(24000000)` or `40000000` if needed.
- Use time-based animation (`dt` from `millis()`) so speed does not depend on
  frame rate.
- Avoid `delay()` in the animation loop unless necessary.

## Current animation

`firmware.ino` shows a rotating 3D wireframe cube:

- 8 corners, 12 edges, perspective projection
- Rainbow edges, dimmed by depth; white corner dots
- 200×200 off-screen canvas, centered below the title
- Cube corners are at radius √3, so the max projected radius is about
  `1.44 × CUBE_SCALE`. With a 200×200 canvas, keep `CUBE_SCALE` ≤ ~63;
  the current value is 62 (max radius ≈ 89 px) to avoid clipping corners.
- Boot color self-test: red → green → blue → white (300 ms each)
- Built-in LED heartbeat at 1 Hz

## Useful defaults

- Landscape size: `screenW = tft.width()` = 320, `screenH = tft.height()` = 240
  after `setRotation(3)`.
- Colors are RGB565; `tft.color565(r, g, b)` is available.
- `ST77XX_BLACK`, `ST77XX_WHITE`, `ST77XX_RED`, `ST77XX_GREEN`,
  `ST77XX_BLUE`, `ST77XX_CYAN` are defined by Adafruit_GFX.
- Text: `setTextSize(2)`, 6×8 font → 12×16 px per character.
