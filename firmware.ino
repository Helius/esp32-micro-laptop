/*
 * ESP32-C6 + ST7789 2.4" 240x320 IPS
 * Rotating wireframe cube (landscape 320x240)
 *
 * Display: T240B7-C10-25 (ST7789, 4-wire SPI)
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
 *
 * Libraries: Adafruit GFX, Adafruit ST7735 and ST7789
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <math.h>

// ---- Pin mapping -----------------------------------------------------------
#define TFT_CS    3
#define TFT_DC    2
#define TFT_RST   1
#define TFT_SCLK  6
#define TFT_MOSI  5

// ---- Panel size (native, portrait) -----------------------------------------
#define PANEL_W   240
#define PANEL_H   320

// Off-screen canvas for the cube. Only this square is pushed to the display,
// so the cube never disappears/reappears between frames.
#define CUBE_AREA 200

Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 cubeCanvas(CUBE_AREA, CUBE_AREA);

int16_t screenW = PANEL_W;
int16_t screenH = PANEL_H;
int16_t areaX, areaY;

// Cube corners in normalized space (-1..1)
const float CUBE_V[8][3] = {
  {-1, -1, -1}, { 1, -1, -1}, { 1,  1, -1}, {-1,  1, -1},
  {-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1},
};

// 12 edges: back face, front face, connecting struts
const uint8_t CUBE_E[12][2] = {
  {0, 1}, {1, 2}, {2, 3}, {3, 0},
  {4, 5}, {5, 6}, {6, 7}, {7, 4},
  {0, 4}, {1, 5}, {2, 6}, {3, 7},
};

// Projection
// Cube corners are at radius sqrt(3), so the maximum projected radius is
// about 1.44 * CUBE_SCALE. The canvas half-size is 100 px, so keep
// CUBE_SCALE below ~64 to leave a margin and avoid clipping the corners.
const float CUBE_SCALE = 62.0f;   // pixels
const float CAM_DIST   = 4.0f;    // camera distance in cube units
const float FOCAL      = 3.0f;

// Rotation angles (radians) and angular speeds (rad/s)
float ax = 0.4f, ay = 0.7f, az = 0.15f;
const float SPEED_X = 0.50f;
const float SPEED_Y = 0.85f;
const float SPEED_Z = 0.30f;

unsigned long lastFrameMs = 0;

float px[8], py[8], pz[8];

// Simple HSV-ish rainbow wheel -> RGB565
uint16_t wheel(uint8_t pos) {
  pos = 255 - pos;
  if (pos < 85)  return tft.color565(255 - pos * 3, 0, pos * 3);
  if (pos < 170) { pos -= 85; return tft.color565(0, pos * 3, 255 - pos * 3); }
  pos -= 170;
  return tft.color565(pos * 3, 255 - pos * 3, 0);
}

// Scale an RGB565 color by 0..255.
uint16_t dimColor(uint16_t c, uint8_t scale) {
  uint16_t r = (c >> 11) & 0x1F;
  uint16_t g = (c >> 5) & 0x3F;
  uint16_t b = c & 0x1F;
  r = (r * scale) / 255;
  g = (g * scale) / 255;
  b = (b * scale) / 255;
  return (r << 11) | (g << 5) | b;
}

void drawTitle() {
  const char *title = "ESP32-C6";
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  int16_t tw = strlen(title) * 6 * 2;
  tft.setCursor((screenW - tw) / 2, 10);
  tft.print(title);
}

void projectCube() {
  float cx = cosf(ax), sx = sinf(ax);
  float cy = cosf(ay), sy = sinf(ay);
  float cz = cosf(az), sz = sinf(az);

  for (int i = 0; i < 8; i++) {
    float vx = CUBE_V[i][0];
    float vy = CUBE_V[i][1];
    float vz = CUBE_V[i][2];

    // Rotate around Y
    float x1 =  vx * cy + vz * sy;
    float z1 = -vx * sy + vz * cy;

    // Rotate around X
    float y1 = vy * cx - z1 * sx;
    float z2 = vy * sx + z1 * cx;

    // Rotate around Z
    float x2 = x1 * cz - y1 * sz;
    float y2 = x1 * sz + y1 * cz;

    // Perspective projection into canvas coordinates
    float s = FOCAL / (CAM_DIST - z2) * CUBE_SCALE;
    px[i] = CUBE_AREA / 2 + x2 * s;
    py[i] = CUBE_AREA / 2 + y2 * s;
    pz[i] = z2;                    // depth, -1 (far) .. +1 (near)
  }
}

void drawCube(GFXcanvas16 &g) {
  for (int i = 0; i < 12; i++) {
    uint8_t a = CUBE_E[i][0];
    uint8_t b = CUBE_E[i][1];

    // Brighter for edges closer to the camera.
    float zAvg = (pz[a] + pz[b]) * 0.5f;
    uint8_t bright = (uint8_t)(90 + (zAvg + 1.0f) * 82.5f);

    uint16_t c = dimColor(wheel((uint8_t)(millis() / 10 + i * 14)), bright);
    g.drawLine((int16_t)px[a], (int16_t)py[a],
               (int16_t)px[b], (int16_t)py[b], c);
  }

  // Small dots at the corners
  for (int i = 0; i < 8; i++) {
    g.fillCircle((int16_t)px[i], (int16_t)py[i], 2, ST77XX_WHITE);
  }
}

void setup() {
  Serial.begin(115200);

  // Built-in LED: quick triple blink to show the sketch is running.
  pinMode(LED_BUILTIN, OUTPUT);
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(120);
    digitalWrite(LED_BUILTIN, LOW);
    delay(120);
  }

  delay(300);
  Serial.println();
  Serial.println("ST7789 wireframe cube starting...");

  // Hardware SPI on SPI2 (FSPI) with our custom pins.
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);

  tft.init(PANEL_W, PANEL_H);
  tft.setRotation(3);              // landscape, 180 deg (320 x 240)
  screenW = tft.width();
  screenH = tft.height();
  Serial.printf("Display: %d x %d (landscape)\n", screenW, screenH);

  // Quick color self-test.
  const uint16_t testColors[] = {ST77XX_RED, ST77XX_GREEN, ST77XX_BLUE,
                                 ST77XX_WHITE};
  for (uint8_t i = 0; i < 4; i++) {
    tft.fillScreen(testColors[i]);
    delay(300);
  }

  tft.fillScreen(ST77XX_BLACK);
  drawTitle();

  // The cube is drawn into a 200x200 off-screen canvas, then pushed in one
  // continuous SPI transfer. This removes the visible frame-to-frame flicker.
  areaX = screenW / 2 - CUBE_AREA / 2;
  areaY = screenH / 2 - CUBE_AREA / 2 + 10;
  cubeCanvas.fillScreen(ST77XX_BLACK);

  lastFrameMs = millis();
  Serial.println("Animation running.");
}

void loop() {
  // Time-based rotation so the speed stays constant regardless of frame rate.
  unsigned long now = millis();
  float dt = (now - lastFrameMs) / 1000.0f;
  lastFrameMs = now;
  if (dt > 0.1f) dt = 0.1f;

  ax += SPEED_X * dt;
  ay += SPEED_Y * dt;
  az += SPEED_Z * dt;

  projectCube();

  cubeCanvas.fillScreen(ST77XX_BLACK);
  drawCube(cubeCanvas);
  tft.drawRGBBitmap(areaX, areaY, (uint16_t *)cubeCanvas.getBuffer(),
                    CUBE_AREA, CUBE_AREA);

  // Built-in LED heartbeat: 1 Hz, non-blocking.
  static unsigned long lastBlink = 0;
  static bool ledOn = false;
  if (millis() - lastBlink >= 500) {
    lastBlink = millis();
    ledOn = !ledOn;
    digitalWrite(LED_BUILTIN, ledOn ? HIGH : LOW);
  }
}
