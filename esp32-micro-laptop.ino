/*
 * ESP32-C6 + ST7789 2.4" 240x320 IPS
 * simple-tui dashboard (landscape 320x240)
 *
 * Display: T240B7-C10-25 (ST7789, 4-wire SPI)
 * Library: TFT_eSPI (configured via tft_setup.h in this sketch folder)
 * UI:      simple-tui (simple_tui.hpp, copied from ../simple-tui/include)
 * Font:    IBM Plex Mono 16px smooth font (IBMPlexMono16.h)
 *
 * Wiring:
 *   1  GND   -> GND
 *   2  DC    -> GPIO 2
 *   3  CS    -> GPIO 3
 *   4  SCL   -> GPIO 6
 *   5  SDA   -> GPIO 5
 *   6  RESET -> GPIO 1
 *   7  VCC   -> 3.3V
 *   8  GND   -> GND
 *   9  LEDA  -> 3.3V (4.7 ohm recommended)
 *   10 LEDK  -> GND
 */

// TFT_eSPI per-sketch setup (driver, pins, SPI clock, FSPI host, SMOOTH_FONT).
// Must be included before <TFT_eSPI.h> so the macros take effect.
#include "tft_setup.h"
#include <TFT_eSPI.h>

#include <string.h>

#include "simple_tui.hpp"
#include "IBMPlexMono16.h"

// ---- Panel size (native, portrait) -----------------------------------------
#define PANEL_W 240
#define PANEL_H 320

TFT_eSPI tft = TFT_eSPI();

bool displayReady = false;

// ---- simple-tui Display backend --------------------------------------------
// simple-tui works on a character grid of fixed-size cells. The smooth font is
// monospace: every glyph advances by the same width and the line height is
// gFont.yAdvance, so both values are read from the loaded font in setup().
int16_t cellW = 10;   // replaced by the font's advance width after loadFont()
int16_t cellH = 22;   // replaced by tft.fontHeight() after loadFont()

// 32 columns x 10 rows: 320 / 10px and 240 / (16px ascent + 6px descent).
static constexpr uint16_t kCols = 320 / 10;
static constexpr uint16_t kRows = 240 / 22;

uint16_t rgb565(const stui::Color& c) {
  return tft.color565(c.r, c.g, c.b);
}

// One character cell: the final text/colors that should be visible there.
// 8 bytes is enough for any single glyph (UTF-8 box-drawing is 3 bytes + NUL).
struct Cell {
  char text[8] = {};
  uint16_t fg = 0;
  uint16_t bg = 0;
  bool valid = false;
};

// simple-tui renders the whole tree on every redraw and repaints each Text
// (it clears the previous cells with spaces, then writes the new content).
// Painting that straight to the panel costs ~190 ms per frame at 16 MHz and
// shows the erase/redraw as flicker. So `print()` only fills a shadow grid;
// `flush()` diffs it against what is already on screen and pushes the few
// cells that actually changed.
class TftDisplay : public stui::Display {
 public:
  void clear() const override {
    if (!displayReady) return;  // SimpleTui clears from its constructor,
    tft.fillScreen(bg_);        // which runs before tft.init()
    for (uint16_t y = 0; y < kRows; ++y)
      for (uint16_t x = 0; x < kCols; ++x) {
        pending_[y][x] = Cell{};
        shown_[y][x] = Cell{};
      }
    dirty_ = false;
  }

  void setColors(const stui::Colors& colors) const override {
    fg_ = rgb565(colors.fg);
    bg_ = rgb565(colors.bg);
  }

  void print(uint16_t x, uint16_t y, const char* str) const override {
    if (!displayReady || str == nullptr || *str == 0 || x >= kCols || y >= kRows)
      return;

    Cell& cell = pending_[y][x];
    if (cell.valid && cell.fg == fg_ && cell.bg == bg_ &&
        strcmp(cell.text, str) == 0)
      return;  // logically unchanged

    strncpy(cell.text, str, sizeof(cell.text) - 1);
    cell.text[sizeof(cell.text) - 1] = 0;
    cell.fg = fg_;
    cell.bg = bg_;
    cell.valid = true;
    dirty_ = true;
  }

  // Push the changed cells to the panel. Call once after a render pass.
  void flush() const {
    if (!displayReady || !dirty_) return;
    dirty_ = false;

    for (uint16_t y = 0; y < kRows; ++y) {
      for (uint16_t x = 0; x < kCols; ++x) {
        Cell& next = pending_[y][x];
        if (!next.valid) continue;
        Cell& prev = shown_[y][x];
        if (prev.valid && prev.fg == next.fg && prev.bg == next.bg &&
            strcmp(prev.text, next.text) == 0)
          continue;

        const int32_t px = static_cast<int32_t>(x) * cellW;
        const int32_t py = static_cast<int32_t>(y) * cellH;
        tft.fillRect(px, py, cellW, cellH, next.bg);
        if (next.text[0] != ' ') {  // space = background only
          tft.setTextColor(next.fg, next.bg);
          tft.setCursor(px, py);
          tft.print(next.text);
        }
        prev = next;
      }
    }
  }

 private:
  mutable uint16_t fg_ = TFT_WHITE;
  mutable uint16_t bg_ = TFT_BLACK;
  mutable Cell pending_[kRows][kCols];
  mutable Cell shown_[kRows][kCols];
  mutable bool dirty_ = false;
};

TftDisplay display;

stui::SimpleTui<kCols, kRows> tui(display);

// ---- Dashboard screen (mirrors simple-tui/src/main.cpp) --------------------
char distance[12] = "0";
uint32_t counter = 0;

stui::Text pageIndicator{"[ *  ]", {.colors = {.fg = stui::color_blue}}};
stui::Text batteryIndicator{"[bat:78%]", {.colors = {.fg = stui::color_blue}}};
stui::Text wifiIndicator{"[wifi:-66 dBm]",
                         {.colors = {.fg = stui::color_blue}}};

stui::Row header{
    {.spacing = 1,
    },
    pageIndicator,
    batteryIndicator,
    wifiIndicator,
};

stui::Text spacer{""};
stui::Text batteryVoltage{"battery: 3.9V"};
stui::Text wifiLevel{"WIFI: -66 dBm"};
stui::Text coordinates{"x,y: 0,0", {.colors = {.bg = stui::color_blue}, .border = true}};

stui::Column leftColumn{
    {.spacing = 0},
    spacer,
    batteryVoltage,
    wifiLevel,
    coordinates,
};

stui::Text distLabel{"dist: ", {.colors = {.fg = stui::color_green}}};
stui::Text distValue{distance,
                     {.colors = {.fg = stui::color_red}}};
stui::Text distUnit{" m", {.colors = {.fg = stui::color_green}}};

stui::Row distanceRow{
    {.spacing = 0},
    distLabel,
    distValue,
    distUnit,
};

stui::Text temperature{"tempr: 10"};
stui::Text pressure{"press: 1013 hPa"};
stui::Text humidity{"humidity: 50%"};
stui::Text uv{"UV: normal"};
stui::Text uptime{"uptime: 1234 s"};

stui::Column rightColumn{
    {.border = true,
     .color = {.fg = stui::color_green}},
    distanceRow,
    temperature,
    pressure,
    humidity,
    uv,
    uptime,
};

stui::Row body{
    {.spacing = 1},
    leftColumn,
    rightColumn,
};

stui::Column page0{
    {.spacing = 0},
    header,
    body,
};

// ---- Built-in RGB LED (WS2812 on GPIO 8) -----------------------------------
void updateLed() {
  static unsigned long lastLedMs = 0;
  if (millis() - lastLedMs < 20) return;
  lastLedMs = millis();

  uint8_t hue = (uint8_t)((millis() / 8) & 0xFF);  // ~3s full cycle
  uint8_t r, g, b;
  if (hue < 85) {
    r = 25 - hue * 25 / 85;  g = 0;                       b = hue * 25 / 85;
  } else if (hue < 170) {
    uint8_t h2 = hue - 85;
    r = 0;                   g = h2 * 25 / 85;            b = 25 - h2 * 25 / 85;
  } else {
    uint8_t h2 = hue - 170;
    r = h2 * 25 / 85;        g = 25 - h2 * 25 / 85;       b = 0;
  }
  rgbLedWrite(RGB_BUILTIN, r, g, b);
}

void setup() {
  Serial.begin(115200);

  delay(300);
  Serial.println();
  Serial.println("ST7789 simple-tui dashboard starting...");

  // Built-in RGB LED at ~10% brightness (max value 25/255).
  pinMode(RGB_BUILTIN, OUTPUT);
  rgbLedWrite(RGB_BUILTIN, 0, 0, 0);

  // TFT_eSPI handles SPI init itself; pins, clock and the FSPI host come
  // from tft_setup.h.
  tft.init();
  tft.setRotation(3);  // landscape (320 x 240)
  Serial.printf("Display: %d x %d (landscape)\n", tft.width(), tft.height());

  tft.fillScreen(TFT_BLACK);

  // Smooth (anti-aliased) font from flash. Needs SMOOTH_FONT in tft_setup.h.
  tft.loadFont(IBMPlexMono16);
  if (!tft.fontLoaded) {
    Serial.println("ERROR: IBMPlexMono16 loadFont() failed");
  }
  // textWidth("M") measures the ink, not the advance; every monospace glyph
  // advances by gxAdvance, which is the real cell width.
  uint16_t glyphIndex = 0;
  if (tft.getUnicodeIndex('M', &glyphIndex)) cellW = tft.gxAdvance[glyphIndex];
  cellH = tft.fontHeight();
  Serial.printf("Font: cell %d x %d px -> %d x %d cells\n", cellW, cellH,
                tft.width() / cellW, tft.height() / cellH);

  displayReady = true;
  tui.setScreen(page0);  // clears, measures, lays out and renders once
  display.flush();
  Serial.println("Dashboard rendered.");
}

void loop() {
  static unsigned long lastUpdateMs = 0;
  const unsigned long now = millis();

  // Refresh the live value a little faster than the 500 ms redraw interval.
  if (now - lastUpdateMs >= 500) {
    lastUpdateMs = now;
    snprintf(distance, sizeof(distance), "%lu", (unsigned long)counter);
    counter++;
  }

  tui.loop(now);   // fills the shadow grid every 500 ms
  display.flush(); // pushes only the cells that actually changed
  updateLed();
}
