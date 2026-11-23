#ifndef FONT_3X7_H
#define FONT_3X7_H

#include <Arduino.h>

// Each digit is 3 columns wide x 7 rows tall.
// Storage: digit[col] = bitmask of lit rows, bit0 = top row, bit6 = bottom row.
// This leaves 1px of vertical margin on an 8-row matrix (row 7 reserved for
// the seconds progress bar / status pixels).

#define FONT_DIGIT_WIDTH  3
#define FONT_DIGIT_HEIGHT 7

// Wider numerals for the hour and minute fields of the clock.
#define FONT_LARGE_DIGIT_WIDTH 4
static const uint8_t FONT_4X7_DIGITS[10][FONT_LARGE_DIGIT_WIDTH] PROGMEM = {
  { 0b1111111, 0b1000001, 0b1000001, 0b1111111 }, // 0
  { 0b0000000, 0b0000000, 0b1111111, 0b0000000 }, // 1
  { 0b1111001, 0b1001001, 0b1001001, 0b1001111 }, // 2
  { 0b1001001, 0b1001001, 0b1001001, 0b1111111 }, // 3
  { 0b0001111, 0b0001000, 0b0001000, 0b1111111 }, // 4
  { 0b1001111, 0b1001001, 0b1001001, 0b1111001 }, // 5
  { 0b1111111, 0b1001001, 0b1001001, 0b1111001 }, // 6
  { 0b0000001, 0b0000001, 0b0000001, 0b1111111 }, // 7
  { 0b1111111, 0b1001001, 0b1001001, 0b1111111 }, // 8
  { 0b1001111, 0b1001001, 0b1001001, 0b1111111 }, // 9
};

static const uint8_t FONT_3X7_DIGITS[10][FONT_DIGIT_WIDTH] PROGMEM = {
  // 0
  { 0b1111111, 0b1000001, 0b1111111 },
  // 1
  { 0b0000000, 0b1111111, 0b0000000 },
  // 2 -- top curve, middle-to-bottom diagonal-ish, bottom flat
  { 0b1111001, 0b1001001, 0b1001111 },
  // 3
  { 0b1001001, 0b1001001, 0b1111111 },
  // 4
  { 0b0001111, 0b0001000, 0b1111111 },
  // 5
  { 0b1001111, 0b1001001, 0b1111001 },
  // 6
  { 0b1111111, 0b1001001, 0b1111001 },
  // 7
  { 0b0000001, 0b0000001, 0b1111111 },
  // 8
  { 0b1111111, 0b1001001, 0b1111111 },
  // 9
  { 0b1001111, 0b1001001, 0b1111111 },
};

// Colon: 1 column wide, two dots at rows 2 and 4 (0-indexed from top)
#define FONT_COLON_WIDTH 1
static const uint8_t FONT_3X7_COLON[FONT_COLON_WIDTH] PROGMEM = {
  0b0010100
};

// Small dash, used e.g. in the date-peek display separator ("DD-MM")
#define FONT_DASH_WIDTH 2
static const uint8_t FONT_3X7_DASH[FONT_DASH_WIDTH] PROGMEM = {
  0b0001000, 0b0001000
};

#endif // FONT_3X7_H
