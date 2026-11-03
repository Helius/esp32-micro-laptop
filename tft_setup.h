// tft_setup.h — per-sketch TFT_eSPI configuration for the ESP32-C6
// microlaptop display (T240B7-C10-25, ST7789, 2.4" 240x320, 4-wire SPI).
//
// TFT_eSPI reads this file via its __has_include(<tft_setup.h>) mechanism.
// Defining USER_SETUP_LOADED stops TFT_eSPI from loading its own default
// User_Setup.h, so the values below are the ones that get compiled in.
//
// Wiring (matches esp32-micro-laptop.ino):
//   DC=GPIO2  CS=GPIO3  SCL=GPIO6  SDA=GPIO5  RESET=GPIO1
//
// ESP32-C6 support in TFT_eSPI comes from PR #3399, which routes C6
// through the C3 processor files (TFT_eSPI_ESP32_C3.*).  The C3 header
// defines SPI_PORT=2 for register access and VSPI=FSPI for the Arduino
// SPI layer, both resolving to the single GPSPI2 peripheral on C6.

#ifndef TFT_SETUP_H
#define TFT_SETUP_H

#define USER_SETUP_LOADED

// ---- Display driver ---------------------------------------------------------
#define ST7789_DRIVER

// ---- Colour order -----------------------------------------------------------
// This panel wires the ST7789 colour order as BGR: with the default (RGB) red
// and blue come out swapped. TFT_RGB_ORDER picks the MADCTL colour-order bit
// at init; it is not changeable at runtime through the public API.
#define TFT_RGB_ORDER TFT_BGR

// ---- Panel size (native portrait) ------------------------------------------
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ---- Pins ------------------------------------------------------------------
#define TFT_MOSI 5
#define TFT_SCLK 6
#define TFT_CS   3
#define TFT_DC   2
#define TFT_RST  1
#define TFT_MISO -1          // not connected (write-only panel)

// ---- SPI host (mandatory on ESP32-C6: route to SPI2 / FSPI) ----------------
#define USE_FSPI_PORT

// ---- Smooth (anti-aliased) fonts -------------------------------------------
// Required by tft.loadFont(IBMPlexMono16) and by SimpleTui's TftDisplay.
#define SMOOTH_FONT

// ---- SPI clock -------------------------------------------------------------
// 16 MHz is the value confirmed reliable on this wiring (matches the
// previous Adafruit ST7789 setup).  The panel/ST7789 also handle 40000000
// (40 MHz) on short wires for a higher frame rate — raise it here if the
// image is stable.
#define SPI_FREQUENCY 16000000

#endif // TFT_SETUP_H