#ifndef FONT_TEXT_5X7_H
#define FONT_TEXT_5X7_H

#include <Arduino.h>

// Simple 5-wide x 7-tall bitmap font for scrolling status text — distinct
// from the compact 3x7 digit-only font used on the clock face itself (see
// Font3x7.h). Same bit convention: each byte is one column, bit0 = top
// row, bit6 = bottom row (bit7 unused). Every glyph occupies exactly 5
// columns (narrow marks like '.' or ':' just use mostly-blank columns),
// which keeps the scrolling code simple — every character advances by a
// fixed FONT_TEXT_WIDTH + 1 (inter-character gap) pixels.
//
// Uppercase letters only, to keep the table a manageable size — lowercase
// input (e.g. from an arbitrary user-set SSID) is uppercased before
// lookup. Any character with no glyph here (accented letters, unusual
// symbols, etc.) falls back to a blank space rather than showing garbage,
// so scrolling an arbitrary SSID can never render something nonsensical.

#define FONT_TEXT_WIDTH  5
#define FONT_TEXT_HEIGHT 7

static const uint8_t GLYPH_SPACE[5]       PROGMEM = {0x00, 0x00, 0x00, 0x00, 0x00};

static const uint8_t GLYPH_A[5] PROGMEM = {0x7C, 0x0A, 0x09, 0x0A, 0x7C};
static const uint8_t GLYPH_B[5] PROGMEM = {0x7F, 0x49, 0x49, 0x49, 0x36};
static const uint8_t GLYPH_C[5] PROGMEM = {0x3E, 0x41, 0x41, 0x41, 0x41};
static const uint8_t GLYPH_D[5] PROGMEM = {0x7F, 0x41, 0x41, 0x41, 0x3E};
static const uint8_t GLYPH_E[5] PROGMEM = {0x7F, 0x49, 0x49, 0x49, 0x41};
static const uint8_t GLYPH_F[5] PROGMEM = {0x7F, 0x09, 0x09, 0x09, 0x01};
static const uint8_t GLYPH_G[5] PROGMEM = {0x3E, 0x41, 0x41, 0x49, 0x79};
static const uint8_t GLYPH_H[5] PROGMEM = {0x7F, 0x08, 0x08, 0x08, 0x7F};
static const uint8_t GLYPH_I[5] PROGMEM = {0x41, 0x41, 0x7F, 0x41, 0x41};
static const uint8_t GLYPH_J[5] PROGMEM = {0x30, 0x40, 0x40, 0x40, 0x3F};
static const uint8_t GLYPH_K[5] PROGMEM = {0x7F, 0x08, 0x14, 0x22, 0x41};
static const uint8_t GLYPH_L[5] PROGMEM = {0x7F, 0x40, 0x40, 0x40, 0x40};
static const uint8_t GLYPH_M[5] PROGMEM = {0x7F, 0x02, 0x04, 0x02, 0x7F};
static const uint8_t GLYPH_N[5] PROGMEM = {0x7F, 0x02, 0x04, 0x08, 0x7F};
static const uint8_t GLYPH_O[5] PROGMEM = {0x3E, 0x41, 0x41, 0x41, 0x3E};
static const uint8_t GLYPH_P[5] PROGMEM = {0x7F, 0x09, 0x09, 0x09, 0x0F};
static const uint8_t GLYPH_Q[5] PROGMEM = {0x3E, 0x41, 0x41, 0x41, 0x7E};
static const uint8_t GLYPH_R[5] PROGMEM = {0x7F, 0x09, 0x19, 0x29, 0x4F};
static const uint8_t GLYPH_S[5] PROGMEM = {0x46, 0x49, 0x49, 0x49, 0x31};
static const uint8_t GLYPH_T[5] PROGMEM = {0x01, 0x01, 0x7F, 0x01, 0x01};
static const uint8_t GLYPH_U[5] PROGMEM = {0x3F, 0x40, 0x40, 0x40, 0x3F};
static const uint8_t GLYPH_V[5] PROGMEM = {0x0F, 0x30, 0x40, 0x30, 0x0F};
static const uint8_t GLYPH_W[5] PROGMEM = {0x7F, 0x20, 0x18, 0x20, 0x7F};
static const uint8_t GLYPH_X[5] PROGMEM = {0x41, 0x22, 0x1C, 0x22, 0x41};
static const uint8_t GLYPH_Y[5] PROGMEM = {0x01, 0x02, 0x7C, 0x02, 0x01};
static const uint8_t GLYPH_Z[5] PROGMEM = {0x61, 0x51, 0x49, 0x45, 0x43};

static const uint8_t GLYPH_0[5] PROGMEM = {0x3E, 0x41, 0x41, 0x41, 0x3E};
static const uint8_t GLYPH_1[5] PROGMEM = {0x44, 0x42, 0x7F, 0x40, 0x40};
static const uint8_t GLYPH_2[5] PROGMEM = {0x40, 0x61, 0x51, 0x49, 0x46};
static const uint8_t GLYPH_3[5] PROGMEM = {0x21, 0x41, 0x49, 0x4D, 0x33};
static const uint8_t GLYPH_4[5] PROGMEM = {0x18, 0x14, 0x12, 0x7F, 0x10};
static const uint8_t GLYPH_5[5] PROGMEM = {0x4F, 0x49, 0x49, 0x49, 0x31};
static const uint8_t GLYPH_6[5] PROGMEM = {0x3E, 0x49, 0x49, 0x79, 0x30};
static const uint8_t GLYPH_7[5] PROGMEM = {0x60, 0x10, 0x09, 0x05, 0x03};
static const uint8_t GLYPH_8[5] PROGMEM = {0x36, 0x49, 0x49, 0x49, 0x36};
static const uint8_t GLYPH_9[5] PROGMEM = {0x06, 0x49, 0x49, 0x49, 0x3E};

static const uint8_t GLYPH_PERIOD[5]      PROGMEM = {0x00, 0x00, 0x40, 0x00, 0x00};
static const uint8_t GLYPH_APOSTROPHE[5]  PROGMEM = {0x00, 0x00, 0x03, 0x00, 0x00};
static const uint8_t GLYPH_QUOTE[5]       PROGMEM = {0x00, 0x03, 0x00, 0x03, 0x00};
static const uint8_t GLYPH_COLON[5]       PROGMEM = {0x00, 0x00, 0x14, 0x00, 0x00};
static const uint8_t GLYPH_SLASH[5]       PROGMEM = {0x40, 0x30, 0x08, 0x06, 0x01};
static const uint8_t GLYPH_UNDERSCORE[5]  PROGMEM = {0x40, 0x40, 0x40, 0x40, 0x40};
static const uint8_t GLYPH_HYPHEN[5]      PROGMEM = {0x08, 0x08, 0x08, 0x08, 0x08};
static const uint8_t GLYPH_COMMA[5]       PROGMEM = {0x00, 0x00, 0x60, 0x00, 0x00};

// Returns a pointer to the 5-byte PROGMEM column array for the given
// character (letters are uppercased before lookup). Falls back to a
// blank space glyph for anything unsupported, so this can never render
// garbage — safe to call with arbitrary user-supplied text (e.g. an SSID).
static inline const uint8_t* getTextGlyph(char c) {
  if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';

  switch (c) {
    case 'A': return GLYPH_A;
    case 'B': return GLYPH_B;
    case 'C': return GLYPH_C;
    case 'D': return GLYPH_D;
    case 'E': return GLYPH_E;
    case 'F': return GLYPH_F;
    case 'G': return GLYPH_G;
    case 'H': return GLYPH_H;
    case 'I': return GLYPH_I;
    case 'J': return GLYPH_J;
    case 'K': return GLYPH_K;
    case 'L': return GLYPH_L;
    case 'M': return GLYPH_M;
    case 'N': return GLYPH_N;
    case 'O': return GLYPH_O;
    case 'P': return GLYPH_P;
    case 'Q': return GLYPH_Q;
    case 'R': return GLYPH_R;
    case 'S': return GLYPH_S;
    case 'T': return GLYPH_T;
    case 'U': return GLYPH_U;
    case 'V': return GLYPH_V;
    case 'W': return GLYPH_W;
    case 'X': return GLYPH_X;
    case 'Y': return GLYPH_Y;
    case 'Z': return GLYPH_Z;
    case '0': return GLYPH_0;
    case '1': return GLYPH_1;
    case '2': return GLYPH_2;
    case '3': return GLYPH_3;
    case '4': return GLYPH_4;
    case '5': return GLYPH_5;
    case '6': return GLYPH_6;
    case '7': return GLYPH_7;
    case '8': return GLYPH_8;
    case '9': return GLYPH_9;
    case '.': return GLYPH_PERIOD;
    case '\'': return GLYPH_APOSTROPHE;
    case '"': return GLYPH_QUOTE;
    case ':': return GLYPH_COLON;
    case '/': return GLYPH_SLASH;
    case '_': return GLYPH_UNDERSCORE;
    case '-': return GLYPH_HYPHEN;
    case ',': return GLYPH_COMMA;
    case ' ': return GLYPH_SPACE;
    default:  return GLYPH_SPACE;
  }
}

#endif // FONT_TEXT_5X7_H
