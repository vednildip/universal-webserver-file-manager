#pragma once
/***********************************************************
 *  LilyGO T-HMI (THMI) — Pin Configuration Header
 *  Parallel TFT + XPT2046 Touch + Power & Backlight Pins
 ***********************************************************/

/***********************
 *  TFT DISPLAY (PARALLEL)
 ***********************/
#define TFT_DC    7
#define TFT_CS    6
#define TFT_WR    8
#define TFT_RST   -1   // No reset pin on this board

// Parallel 8-bit data bus D0–D7
#define TFT_D0   48
#define TFT_D1   47
#define TFT_D2   39
#define TFT_D3   40
#define TFT_D4   41
#define TFT_D5   42
#define TFT_D6   45
#define TFT_D7   46

/***********************
 *  POWER & BACKLIGHT
 ***********************/
#define PWR_PIN  10
#define TFT_BL   38

/***********************
 *  TOUCH (XPT2046)
 ***********************/
#define TOUCH_XPT2046_CS     2
#define TOUCH_XPT2046_INT    9
#define TOUCH_XPT2046_SCK    1
#define TOUCH_XPT2046_MISO   4
#define TOUCH_XPT2046_MOSI   3
#define TOUCH_XPT2046_ROTATION   0
#define TOUCH_XPT2046_SAMPLES    50

/***********************
 *  COMMON COLORS
 ***********************/
#define WHITE 0xFFFF
#define BLACK 0x0000

/***********************
 *  END OF FILE
 ***********************/
