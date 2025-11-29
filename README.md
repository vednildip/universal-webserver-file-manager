# universal-webserver-file-manager
A universal ESP32 webserver and SDMMC file manager with full-screen PNG/JPEG viewer, slideshow, touch control, and support for LovyanGFX, TFT_eSPI, and Arduino_GFX displays.

A clean, structured, universal image viewer and webserver for ESP32-based TFT setups. touch, PNG/JPEG images, SD_MMc, and a WiFi-accessible web interface.

## Features

- **Universal TFT support**: LovyanGFX, TFT_eSPI, Arduino_GFX selection
- **Image formats**: PNG + JPEG (with automatic scaling and color correctness)
- **Touch interface**: XPT2046 controller (with debounce, auto-coordinates)
- **WiFi**: Auto-connects, auto-reconnects, mDNS, WiFi symbol on TFT, local IP display
- **SDMMC Card**: Loads image list from SD, randomizes slideshow, directory browsing
- **Webserver**: Browse, upload/delete/rename files, display images directly on your TFT, JSON directory listing, supports file download
- **Slideshow**: Automatic, periodically changes displayed image; touch swaps next image

## Usage

1. **Wiring & Setup**

   - ESP32S3-LILYGO THMI ST7789 Parrel 8 bit or  powered with appropriate TFT and XPT2046 touch wiring
   - MicroSD wired for SD_MMC (see `clk`, `cmd`, `d0` pin assignments in code)
   - Optionally update WiFi credentials (`ssid`, `password` in code)

2. **Install Required Libraries**  
   Use Arduino IDE's Library Manager or PlatformIO:

   - LovyanGFX, TFT_eSPI, Arduino_GFX_Library
   - XPT2046_Touchscreen
   - JPEGDecoder, PNGdec
   - ESPmDNS, WiFi, WebServer, SD_MMC

3. **Prepare SD Card**
   - Create `/pictures/` directory and put some `.jpg` or `.png` images inside

4. **Upload Code**
   - Place provided code in `src/main.ino` or directly into Arduino IDE
   - Add `Lilylogo.h` and `index_html.cpp` (which you reference for the TFT splash and web interface)

5. **Build & Flash**
   - Flash ESP32 using Arduino IDE, PlatformIO, or esptool
   - Open serial monitor for debug logs (baud 115200)
   - Connect device; TFT will show splash, WiFi status, and images
  
6. **Access Webserver**
   - From a browser on your WiFi network, open `http://esp32.local` or check the TFT for your ESP32’s IP
  
## Example Web Interface

- List images and directories (JSON)
- Direct image display (`/display?path=...`)
- Upload, rename, delete, create directory
- Download files

## Pin & Library Selection

Library selection is at compile-time via:
```cpp
#define TFT_LIBRARY 0  // 0 = LovyanGFX, 1 = TFT_eSPI, 2 = Arduino_GFX
```
Change to match your hardware and installed library.

## Licensing

MIT License (see LICENSE file).

## Credits

By NILESH DARAWADE — see comments for contributors.  
Inspired by ESP32 universal TFT projects.

---

 


