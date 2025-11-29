/***********************************************************
 *  UNIVERSAL IMAGE VIEWER – CLEAN STRUCTURED VERSION (A2)
 *  Supports: LovyanGFX, TFT_eSPI, Arduino_GFX
 *  Supports PNG + JPEG (fixed colors)
 *  Common XPT2046 touch
 **********************************************************/
#define TFT_LIBRARY 0  // 0 = LovyanGFX, 1 = TFT_eSPI, 2 = Arduino_GFX

#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SD_MMC.h>
#include <JPEGDecoder.h>
#include <PNGdec.h>
#include <vector>
#include <algorithm>
#include <random>
#include "Lilylogo.h"
#include "index_html.cpp"
const char* webHTML = index_html;
bool webHTMLIsFile = false;
// === Touch debounce ===
unsigned long lastTouchTime = 0;
const unsigned long debounceDelay = 300;
bool wasPressed = false;
// Pin configuration
#define TOUCH_XPT2046_CS 2
#define TOUCH_XPT2046_INT 9
#define TOUCH_XPT2046_SCK 1
#define TOUCH_XPT2046_MISO 4
#define TOUCH_XPT2046_MOSI 3
#define TOUCH_XPT2046_ROTATION 0
#define TOUCH_XPT2046_SAMPLES 50
/***********************************************************
 *  GLOBALS
 ***********************************************************/
PNG png;
#define FS_SD SD_MMC
#define DBG_OUTPUT_PORT Serial
// ----------------------------------------------------------
// TFT LIBRARIES
// ----------------------------------------------------------
#define PWR_PIN 10
#define TFT_BL 38
#if TFT_LIBRARY == 1
#include <TFT_eSPI.h>
TFT_eSPI* tft = new TFT_eSPI();
#elif TFT_LIBRARY == 0
#include <LovyanGFX.hpp>
LGFX tft_real;
LGFX* tft = &tft_real;
#else
#include <Arduino_GFX_Library.h>
Arduino_DataBus* bus = new Arduino_ESP32PAR8(
  7 /* DC */, 6 /* CS */, 8 /* WR */, GFX_NOT_DEFINED /* RD */,
  48 /* D0 */, 47 /* D1 */, 39 /* D2 */, 40 /* D3 */, 41 /* D4 */, 42 /* D5 */, 45 /* D6 */, 46 /* D7 */);
Arduino_GFX* tft = new Arduino_ST7789(bus, NOT_A_PIN /*TFT_RST*/, 1, false, 240, 320);
// Shared color set (all libraries now match)
#define TFT_BLACK RGB565(0, 0, 0)
#define TFT_WHITE RGB565(255, 255, 255)
#define TFT_RED RGB565(255, 0, 0)
#define TFT_GREEN RGB565(0, 255, 0)
#define TFT_BLUE RGB565(0, 0, 255)
#define TFT_YELLOW RGB565(255, 255, 0)
#define TFT_CYAN RGB565(0, 255, 255)
#define TFT_MAGENTA RGB565(255, 0, 255)
#define TFT_GREY RGB565(128, 128, 128)
#define TFT_DARKGREY RGB565(50, 50, 50)
#endif
/***********************************************************
 *  TOUCH (shared for TFT_eSPI + Arduino_GFX)
 ***********************************************************/
#if TFT_LIBRARY != 0
#include <XPT2046_Touchscreen.h>
#define TOUCH_XPT2046_CS 2
#define TOUCH_XPT2046_INT 9
#define TOUCH_XPT2046_SCK 1
#define TOUCH_XPT2046_MISO 4
#define TOUCH_XPT2046_MOSI 3
XPT2046_Touchscreen ts(TOUCH_XPT2046_CS, TOUCH_XPT2046_INT);
int16_t touch_max_x = 319;
int16_t touch_max_y = 239;
bool touch_swap_xy = true;
int16_t touch_last_x = 0;
int16_t touch_last_y = 0;
void translate_touch_raw(int16_t raw_x, int16_t raw_y) {
  int16_t map_x1 = 200, map_x2 = 3800;
  int16_t map_y1 = 200, map_y2 = 3800;
  if (touch_swap_xy) {
    touch_last_x = map(raw_y, map_x1, map_x2, 0, touch_max_x);
    touch_last_y = map(raw_x, map_y1, map_y2, 0, touch_max_y);
  } else {
    touch_last_x = map(raw_x, map_x1, map_x2, 0, touch_max_x);
    touch_last_y = map(raw_y, map_y1, map_y2, 0, touch_max_y);
  }
}
#endif
/***********************************************************
 *  WIFI
 ***********************************************************/
const char* ssid = "XXXXXXXX";
const char* password = "XXXXXXXXXX";
const char* host = "esp32";
WebServer server(80);
File uploadFile;  // MUST be global
/***********************************************************
 *  IMAGE & SD GLOBALS
 ***********************************************************/
std::vector<String> imageList;
int currentImageIndex = 0;
unsigned long lastImageChange = 0;
bool hasSD = false;
/***********************************************************
 *  UNIVERSAL pushImage WRAPPER
 ***********************************************************/
inline void swapRGB565(uint16_t* buf, int pixels) {
  for (int i = 0; i < pixels; i++) {
    uint16_t c = buf[i];
    buf[i] = (c >> 8) | (c << 8);
  }
}
void tftPushImage(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* data) {
#if TFT_LIBRARY == 2
  tft->draw16bitRGBBitmap(x, y, data, w, h);
#elif TFT_LIBRARY == 1
  tft->pushImage(x, y, w, h, data);
#else
  tft->pushImage(x, y, w, h, data, false);
#endif
}
/***********************************************************
 *  DISPLAY INITIALIZATION
 ***********************************************************/

void powerOn() {
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, HIGH);
  delay(150);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
}
void tftStart_THMI() {
  // Library-specific begin/init
  powerOn();
#if TFT_LIBRARY != 2
  tft->begin();
  //tft->setBrightness(12);  // for  1
#else
  tft->begin(800000);
#endif
  // Somewhere after tft->begin() and display ready
  tft->setTextColor(TFT_WHITE, TFT_BLACK);  // White text on black background
  tft->setCursor(35, 5);
  tft->setTextSize(1);  // small offset from top-left corner
  // --- Show splash/logo ---
  uint16_t* logoBuf = (uint16_t*)malloc(240 * 320 * sizeof(uint16_t));
  if (!logoBuf) return;
  memcpy(logoBuf, gImage_logo, 240 * 320 * sizeof(uint16_t));
#if TFT_LIBRARY == 1
  swapRGB565(logoBuf, 240 * 320);  // TFT_eSPI needs byte swap
#elif TFT_LIBRARY != 2
  // All other libraries except #2 need byte swap
  for (size_t i = 0; i < 240 * 320; i++) {
    uint16_t v = logoBuf[i];
    logoBuf[i] = (v << 8) | (v >> 8);
  }
#endif
#if TFT_LIBRARY == 2
  tft->setRotation(0);  // 0 = portrait
  tftPushImage(0, 0, 240, 320, (uint16_t*)gImage_logo);
  tft->print("Arduino_GFX");
#else
  tftPushImage(0, 0, 240, 320, logoBuf);
#if TFT_LIBRARY == 0
  tft->print("LovyanGFX");
#elif TFT_LIBRARY == 1
  tft->print("TFT_eSPI");
#else
  tft->print("Unknown TFT");
#endif
#endif
  free(logoBuf);
  delay(1200);
}
/***********************************************************
 *  WIFI SYMBOL DRAWING (common for all libraries)
 ***********************************************************/
void drawWiFiSymbol(bool connected) {
  int cx = 15, cy = 25;
  uint32_t fg = connected ? TFT_GREEN : TFT_DARKGREY;
  uint32_t bg = TFT_BLACK;

  // Draw arcs / WiFi signal
#if TFT_LIBRARY == 1
  // TFT_eSPI: smooth arcs
  tft->drawSmoothArc(cx, cy, 21, 20, 45, 225, fg, bg);
  tft->drawSmoothArc(cx, cy, 15, 14, 135, 225, fg, bg);
  tft->drawSmoothArc(cx, cy, 9, 8, 135, 225, fg, bg);
#else
  // LovyanGFX or Arduino_GFX: simple arcs
  tft->drawArc(cx, cy, 21, 20, 225, 315, fg);
  tft->drawArc(cx, cy, 15, 14, 225, 315, fg);
  tft->drawArc(cx, cy, 9, 8, 225, 315, fg);
#endif
  // Draw central dot
  tft->fillCircle(cx, cy, 2, fg);
  // Red X when offline
  if (!connected) {
    tft->drawLine(cx - 10, cy - 10, cx + 10, cy + 10, TFT_RED);
    tft->drawLine(cx - 10, cy + 10, cx + 10, cy - 10, TFT_RED);
  }
  // Display IP or "No WiFi" at bottom-right
  //tft->setTextSize(1);
  tft->setTextColor(TFT_WHITE, TFT_BLACK);
#if TFT_LIBRARY == 2
  tft->setFont();  // Arduino_GFX default font
#else
  tft->setTextFont(1);  // TFT_eSPI / LovyanGFX
#endif
  int16_t sw = tft->width();
  int16_t sh = tft->height();
  tft->fillRect(sw - 120, sh - 16, 120, 16, TFT_BLACK);
  tft->setCursor(sw - (connected ? 110 : 90), sh - 12);
  if (connected) tft->print(WiFi.localIP());
  else tft->print("No WiFi");
  tft->setCursor(35, 5);
#if TFT_LIBRARY == 0
  tft->print("LovyanGFX");
#elif TFT_LIBRARY == 1
  tft->print("TFT_eSPI");
#elif TFT_LIBRARY == 2
  tft->print("Arduino_GFX");
#else
tft->print("Unknown TFT");
#endif
}
/***********************************************************
 *  WIFI CONNECT
 ***********************************************************/
void WIFI_CONNECT() {
  Serial.println("Connecting WiFi...");
  drawWiFiSymbol(false);
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  const unsigned long timeout = 15000;
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeout) {
    delay(200);
    Serial.print(".");
    yield();
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected: " + WiFi.localIP().toString());
    drawWiFiSymbol(true);
    if (MDNS.begin(host)) {
      Serial.println("mDNS responder started");
    }
  } else {
    Serial.println("\nWiFi NOT available — offline");
    drawWiFiSymbol(false);
  }
}
/***********************************************************
 *  SDMMC INITIALIZATION
 ***********************************************************/
int clk = 12, cmd = 11, d0 = 13, d1 = -1, d2 = -1, d3 = -1;
void initSDMMC() {
  if (!SD_MMC.setPins(clk, cmd, d0, d1, d2, d3)) {
    Serial.println("SDMMC pin config failed!");
    return;
  }
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("SDMMC init FAILED");
  } else {
    Serial.println("SDMMC init OK");
    float gb = SD_MMC.cardSize() / 1024.0 / 1024.0 / 1024.0;
    Serial.printf("Card Size: %.2f GB\n", gb);
    hasSD = true;
  }
}
/***********************************************************
 *  LOAD IMAGE LIST
 ***********************************************************/
void loadImageList() {
  imageList.clear();
  File dir = SD_MMC.open("/pictures");
  if (!dir || !dir.isDirectory()) {
    Serial.println("No /pictures directory");
    return;
  }
  File e;
  while ((e = dir.openNextFile())) {
    if (!e.isDirectory()) {
      String name = e.name();
      name.toLowerCase();
      if (
        name.endsWith(".jpg") || name.endsWith(".jpeg") || name.endsWith(".png")) {
        imageList.push_back("/pictures/" + name);
      }
    }
    e.close();
  }
  if (!imageList.empty()) {
    std::shuffle(imageList.begin(), imageList.end(), std::mt19937(std::random_device{}()));
    Serial.println("Images randomized");
  }
}
/***********************************************************
 *  PNG DECODER CALLBACKS
 ***********************************************************/
#define MAX_IMAGE_WIDTH 480  // safe for all displays
int xpos = 0, ypos = 0;
void* pngOpen(const char* filename, int32_t* size) {
  File* f = new File();
  *f = SD_MMC.open(filename, FILE_READ);
  if (!(*f)) return nullptr;
  *size = f->size();
  return (void*)f;
}
void pngClose(void* handle) {
  File* f = (File*)handle;
  if (f) {
    f->close();
    delete f;
    yield();
  }
}
int32_t pngRead(PNGFILE* handle, uint8_t* buffer, int32_t length) {
  File* f = (File*)handle->fHandle;
  return f->read(buffer, length);
}
int32_t pngSeek(PNGFILE* handle, int32_t position) {
  File* f = (File*)handle->fHandle;
  f->seek(position);
  return position;
}
int pngDraw(PNGDRAW* pDraw) {
  uint16_t lineBuffer[MAX_IMAGE_WIDTH];
  png.getLineAsRGB565(
    pDraw,
    lineBuffer,
    PNG_RGB565_BIG_ENDIAN,
    0xFFFFFFFF);
  tftPushImage(
    xpos,
    ypos + pDraw->y,
    pDraw->iWidth,
    1,
    lineBuffer);
  return 1;
}
/***********************************************************
 *  JPEG RENDERER WITH PERFECT COLOR & SCALE
 ***********************************************************/
static inline uint16_t swap565(uint16_t c) {
  return (c << 8) | (c >> 8);
}
void jpegRenderAutoScaled() {
  int imgW = JpegDec.width;
  int imgH = JpegDec.height;
  // Rotate display depending on image orientation
  bool portrait = imgH > imgW;
  tft->setRotation(portrait ? 0 : 1);
  int tftW = tft->width();
  int tftH = tft->height();
  // Preserve aspect ratio
  float scale = min((float)tftW / imgW, (float)tftH / imgH);
  int newW = imgW * scale;
  int newH = imgH * scale;
  int offsetX = (tftW - newW) / 2;
  int offsetY = (tftH - newH) / 2;
  bool useMCU = false;
  uint32_t neededMemory = (uint32_t)imgW * imgH * sizeof(uint16_t);
  const uint32_t maxMem = 4UL * 1024UL * 1024UL;
#if TFT_LIBRARY == 0
  // LovyanGFX: use full buffer if enough RAM
  if (neededMemory <= maxMem) {
    uint16_t* fullImg = (uint16_t*)malloc(imgW * imgH * sizeof(uint16_t));
    if (fullImg) {
      while (JpegDec.read()) {
        uint16_t* block = JpegDec.pImage;
        int mcuX = JpegDec.MCUx * JpegDec.MCUWidth;
        int mcuY = JpegDec.MCUy * JpegDec.MCUHeight;
        // Swap bytes for LovyanGFX
        for (int yy = 0; yy < JpegDec.MCUHeight; yy++) {
          for (int xx = 0; xx < JpegDec.MCUWidth; xx++) {
            int px = mcuX + xx;
            int py = mcuY + yy;
            if (px < imgW && py < imgH) {
              uint16_t v = block[yy * JpegDec.MCUWidth + xx];
              fullImg[py * imgW + px] = (v << 8) | (v >> 8);
            }
          }
        }
      }
      tft->pushImageRotateZoom(offsetX, offsetY, 0, 0, 0, scale, scale, imgW, imgH, fullImg, true);
      free(fullImg);
      return;
    } else {
      useMCU = true;  // fallback to MCU loop
    }
  } else {
    useMCU = true;
  }
#else
  useMCU = true;        // all other libraries use MCU scaling
#endif
  // MCU-by-MCU scaled rendering
  if (useMCU) {
    while (JpegDec.read()) {
      uint16_t* block = JpegDec.pImage;
      int mcuX = JpegDec.MCUx * JpegDec.MCUWidth;
      int mcuY = JpegDec.MCUy * JpegDec.MCUHeight;
#if TFT_LIBRARY == 0
      // LovyanGFX byte swap per MCU
      for (int i = 0; i < JpegDec.MCUWidth * JpegDec.MCUHeight; i++) {
        block[i] = (block[i] << 8) | (block[i] >> 8);
      }
#endif
      // Compute scaled block bounding box
      int sx0 = offsetX + round(mcuX * scale);
      int sy0 = offsetY + round(mcuY * scale);
      int sx1 = offsetX + round((mcuX + JpegDec.MCUWidth) * scale);
      int sy1 = offsetY + round((mcuY + JpegDec.MCUHeight) * scale);
      int sw = sx1 - sx0;
      int sh = sy1 - sy0;
      if (sw <= 0 || sh <= 0) continue;
#if TFT_LIBRARY == 1
      // TFT_eSPI: allocate small scaled buffer per MCU to handle large images
      uint16_t* scaledBlock = (uint16_t*)malloc(sw * sh * 2);
      if (scaledBlock) {
        for (int y = 0; y < sh; y++) {
          int srcY = y / scale;
          if (srcY >= JpegDec.MCUHeight) srcY = JpegDec.MCUHeight - 1;
          for (int x = 0; x < sw; x++) {
            int srcX = x / scale;
            if (srcX >= JpegDec.MCUWidth) srcX = JpegDec.MCUWidth - 1;
            uint16_t c = block[srcY * JpegDec.MCUWidth + srcX];
            scaledBlock[y * sw + x] = (c << 8) | (c >> 8);
          }
        }
        tft->pushImage(sx0, sy0, sw, sh, scaledBlock);
        free(scaledBlock);
      }
#elif TFT_LIBRARY == 2
      // Library 2: scaled MCU block
      uint16_t* scaledBlock = (uint16_t*)malloc(sw * sh * sizeof(uint16_t));
      if (scaledBlock) {
        for (int y = 0; y < sh; y++) {
          int srcY = y / scale;
          if (srcY >= JpegDec.MCUHeight) srcY = JpegDec.MCUHeight - 1;
          for (int x = 0; x < sw; x++) {
            int srcX = x / scale;
            if (srcX >= JpegDec.MCUWidth) srcX = JpegDec.MCUWidth - 1;
            scaledBlock[y * sw + x] = block[srcY * JpegDec.MCUWidth + srcX];
          }
        }
        tftPushImage(sx0, sy0, sw, sh, scaledBlock);
        free(scaledBlock);
      }
#else
      // Other libraries: direct MCU draw
      tftPushImage(sx0, sy0, sw, sh, block);
#endif
    }
  }
}
/***********************************************************
 *  UNIVERSAL IMAGE DRAWER (PNG + JPEG)
 ***********************************************************/
void drawImage(const char* filename, int x, int y) {
  xpos = x;
  ypos = y;
  String fn = filename;
  fn.toLowerCase();
  Serial.printf("Drawing image: %s\n", filename);
  /***********************  PNG  ***************************/
  if (fn.endsWith(".png")) {
    int rc = png.open(filename, pngOpen, pngClose, pngRead, pngSeek, pngDraw);
    if (rc == PNG_SUCCESS) {
      tft->startWrite();
      png.decode(NULL, 0);
      png.close();
      tft->endWrite();
    } else {
      Serial.println("PNG decode failed!");
    }
    return;
  }
  /***********************  JPEG  *************************/
  if (fn.endsWith(".jpg") || fn.endsWith(".jpeg")) {
    File f = SD_MMC.open(filename);
    if (!f) {
      Serial.println("JPEG open failed!");
      return;
    }
    if (!JpegDec.decodeSdFile(f)) {
      Serial.println("JPEG decode failed!");
      f.close();
      yield();
      return;
    }
    tft->startWrite();
    jpegRenderAutoScaled();
    tft->endWrite();
    f.close();
    yield();
    return;
  }
  /***********************  OTHER  ************************/
  Serial.println("Unsupported format.");
}
/***********************************************************
 *  MIME HELPER
 ***********************************************************/
String getContentType(String fn) {
  fn.toLowerCase();
  if (fn.endsWith(".png")) return "image/png";
  if (fn.endsWith(".jpg")) return "image/jpeg";
  if (fn.endsWith(".jpeg")) return "image/jpeg";
  if (fn.endsWith(".html") || fn.endsWith(".htm")) return "text/html";
  return "text/plain";
}
void serveWebHtml() {
  if (hasSD && webHTMLIsFile) {
    File f = SD_MMC.open("/" + String(webHTML));
    if (f) {
      server.streamFile(f, "text/html");
      f.close();
      yield();
      return;
    }
    server.send(404, "text/plain", "File Not Found");
  } else {
    server.send(200, "text/html", webHTML);
  }
}
void handleImage(String path) {
  File f = SD_MMC.open(path);
  if (!f) {
    server.send(404, "text/plain", "File Not Found");
    return;
  }
  server.streamFile(f, getContentType(path));
  f.close();
  yield();
}
void handleDisplayImage() {
  if (!server.hasArg("path")) {
    server.send(400, "text/plain", "Missing 'path'");
    return;
  }
  String img = server.arg("path");
  if (!img.startsWith("/"))
    img = "/" + img;
  if (!SD_MMC.exists(img)) {
    server.send(404, "text/plain", "Image not found");
    return;
  }
  tft->fillScreen(TFT_BLACK);  // instead of TFT_BLACK
  drawImage(img.c_str(), 0, 0);
  server.send(200, "text/plain", "Displayed");
}
/***********************************************************
 *  START WEB SERVER
 ***********************************************************/
void handleFileUpload() {
  HTTPUpload& upload = server.upload();
  static File uploadFile;
  if (upload.status == UPLOAD_FILE_START) {
    String filename = "/" + upload.filename;
    Serial.printf("Upload start: %s\n", filename.c_str());
    uploadFile = SD_MMC.open(filename, FILE_WRITE);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) uploadFile.close();
    Serial.println("Upload finished");
  }
}
void Server_ON() {
  // === ROUTES ===
  server.on("/", HTTP_GET, serveWebHtml);
  server.on("/image", HTTP_GET, []() {
    if (!server.hasArg("path"))
      return server.send(400, "text/plain", "Missing path");
    handleImage(server.arg("path"));
  });
  server.on("/display", HTTP_GET, handleDisplayImage);
  // === LIST FILES ===
  server.on("/list", HTTP_GET, []() {
    if (!server.hasArg("dir"))
      return server.send(400, "text/plain", "Missing dir");
    String dirPath = server.arg("dir");
    File dir = SD_MMC.open(dirPath);
    if (!dir || !dir.isDirectory())
      return server.send(400, "text/plain", "Not a directory");
    String json = "[";
    File file = dir.openNextFile();
    bool first = true;
    while (file) {
      if (!first) json += ",";
      json += "{\"type\":\"";
      json += (file.isDirectory() ? "dir" : "file");
      json += "\",\"name\":\"";
      json += file.name();
      json += "\"}";
      first = false;
      file.close();
      file = dir.openNextFile();
    }
    json += "]";
    server.send(200, "application/json", json);
  });
  // === UPLOAD ===
  server.on(
    "/upload",
    HTTP_POST,
    []() {
      server.send(200, "text/plain", "Upload OK");
    },
    []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        String filename = "/" + upload.filename;
        Serial.printf("Upload start: %s\n", filename.c_str());
        uploadFile = SD_MMC.open(filename, FILE_WRITE);
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile)
          uploadFile.write(upload.buf, upload.currentSize);
      } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) uploadFile.close();
        Serial.println("Upload finished");
      }
    });
  // === DELETE ===
  server.on("/delete", HTTP_GET, []() {
    if (!server.hasArg("path"))
      return server.send(400, "text/plain", "Missing path");
    bool ok = SD_MMC.remove(server.arg("path"));
    server.send(200, "text/plain", ok ? "Deleted" : "Delete failed");
  });
  // === RENAME ===
  server.on("/rename", HTTP_GET, []() {
    if (!server.hasArg("oldPath") || !server.hasArg("newPath"))
      return server.send(400, "text/plain", "Missing");
    bool ok = SD_MMC.rename(server.arg("oldPath"), server.arg("newPath"));
    server.send(200, "text/plain", ok ? "Renamed" : "Rename failed");
  });
  // === CREATE DIR ===
  server.on("/create", HTTP_POST, []() {
    if (!server.hasArg("path"))
      return server.send(400, "text/plain", "Missing path");
    bool ok = SD_MMC.mkdir(server.arg("path"));
    server.send(200, "text/plain", ok ? "Created" : "Create failed");
  });
  // === DOWNLOAD ===
  server.on("/download", HTTP_GET, []() {
    if (!server.hasArg("path"))
      return server.send(400, "text/plain", "Missing path");
    String path = server.arg("path");
    if (!SD_MMC.exists(path))
      return server.send(404, "text/plain", "File not found");
    File file = SD_MMC.open(path, FILE_READ);
    if (!file)
      return server.send(500, "text/plain", "Open failed");
    String filename = path.substring(path.lastIndexOf('/') + 1);
    server.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
    server.sendHeader("Content-Type", "application/octet-stream");
    server.sendHeader("Connection", "close");
    server.streamFile(file, "application/octet-stream");
    file.close();
  });
  // === START SERVER (even offline) ===
  server.begin();
  Serial.println("HTTP server ready.");
}
/***********************************************************
 *  SETUP
 ***********************************************************/
void setup() {
  Serial.begin(115200);
   //selectTFTLibrary();  // Ask user which library to use
  tftStart_THMI();
   //TFT_LIBRARY = selectTFTLibraryOnTFT();
    Serial.printf("TFT Library selected: %d\n", TFT_LIBRARY);
  // TOUCH INIT
#if TFT_LIBRARY == 0
  // LovyanGFX self-calibration table
  uint16_t calData[] = { 366, 201, 322, 3694, 3783, 225, 3780, 3620 };
  tft->setTouchCalibrate(calData);
#else
  // XPT2046 Shared for TFT_eSPI + Arduino_GFX
  SPI.begin(TOUCH_XPT2046_SCK, TOUCH_XPT2046_MISO, TOUCH_XPT2046_MOSI, TOUCH_XPT2046_CS);
  ts.begin();
  ts.setRotation(TOUCH_XPT2046_ROTATION);
  pinMode(TOUCH_XPT2046_INT, INPUT_PULLUP);
  touch_max_x = tft->width() - 1;
  touch_max_y = tft->height() - 1;
#endif
  // WiFi
  WIFI_CONNECT();
  // SD & Images
  initSDMMC();
  drawImage("/pictures/girl.jpg", 0, 0);
  loadImageList();
  // Web server
  Server_ON();
}
/***********************************************************
 *  LOOP
 ***********************************************************/
void loop() {
  /**************** WIFI AUTO RECONNECT ****************/
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 5000) {
    lastWiFiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
      drawWiFiSymbol(false);
    } else {
      drawWiFiSymbol(true);
    }
  }
  /**************** TOUCH HANDLING ****************/
  bool pressed = false;
  uint16_t x = 0, y = 0;
#if TFT_LIBRARY != 0
  if (ts.tirqTouched() && ts.touched()) {
    TS_Point p = ts.getPoint();
    if (p.z > 50) {
      pressed = true;
      translate_touch_raw(p.x, p.y);
      x = touch_last_x;
      y = touch_last_y;
    }
  }
#else
  pressed = tft->getTouch(&x, &y);
#endif
  /**************** DISPLAY TOUCH COORDS ****************/
  static uint16_t prevX = 0xFFFF, prevY = 0xFFFF;
  if (pressed && !wasPressed && millis() - lastTouchTime > debounceDelay) {
    lastTouchTime = millis();
    wasPressed = true;
    // --- ERASE previous coordinates ---
    if (prevX != 0xFFFF && prevY != 0xFFFF) {
      tft->fillRect(0, tft->height() - 16, 80, 16, TFT_WHITE);
    }
    /**************** IMAGE CHANGE ON TOUCH ****************/
    if (imageList.size() > 0) {
      currentImageIndex = (currentImageIndex + 1) % imageList.size();
      tft->fillScreen(TFT_BLACK);
      drawImage(imageList[currentImageIndex].c_str(), 0, 0);
    }
    // --- SHOW new coordinates ---
    tft->setCursor(0, tft->height() - 16);
    tft->setTextColor(TFT_BLACK);
    tft->setTextSize(1);
    tft->printf("X:%3d Y:%3d", x, y);
    // --- SERIAL DEBUG ---
    Serial.printf("Touch: X=%d Y=%d\n", x, y);
    prevX = x;
    prevY = y;
  }
  if (!pressed)
    wasPressed = false;
  /**************** SLIDESHOW (15s) ****************/
  if (millis() - lastImageChange > 15000 && imageList.size() > 0) {
    lastImageChange = millis();
    currentImageIndex = (currentImageIndex + 1) % imageList.size();
    tft->fillScreen(TFT_BLACK);
    drawImage(imageList[currentImageIndex].c_str(), 0, 0);
  }
  /**************** HANDLE WEB CLIENTS ****************/
  server.handleClient();
}
