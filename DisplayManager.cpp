#include "DisplayManager.h"
#include "Font3x7.h"
#include "FontText5x7.h"
#include <SPI.h>

// MAX7219 registers
#define REG_NOOP        0x00
#define REG_DIGIT0      0x01  // digit0..7 = row0..7 when decode mode is off
#define REG_DECODE_MODE 0x09
#define REG_INTENSITY   0x0A
#define REG_SCAN_LIMIT  0x0B
#define REG_SHUTDOWN    0x0C
#define REG_DISPLAYTEST 0x0F

DisplayManager::DisplayManager(SettingsManager &settings)
  : _settings(settings) {
  memset(_backBuffer, 0, sizeof(_backBuffer));
  memset(_frontBuffer, 0, sizeof(_frontBuffer));
}

void DisplayManager::begin() {
  pinMode(MAX7219_CS_PIN, OUTPUT);
  digitalWrite(MAX7219_CS_PIN, HIGH);

  SPI.begin();
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);
  SPI.setFrequency(10000000); // 10MHz, well within MAX7219 spec (~10MHz max)

  sendCommandAll(REG_DISPLAYTEST, 0x00);   // ensure test mode is off
  sendCommandAll(REG_SCAN_LIMIT, 0x07);    // show all 8 digits/rows
  sendCommandAll(REG_DECODE_MODE, 0x00);   // no BCD decode — raw pixel mode
  sendCommandAll(REG_SHUTDOWN, 0x01);      // normal operation (not shutdown)
  refreshBrightnessFromSettings();

  clear();
  flip();
}

void DisplayManager::refreshBrightnessFromSettings() {
  setBrightness(_settings.matrixBrightness());
}

void DisplayManager::sendCommandAll(uint8_t reg, uint8_t value) {
  // Send the same 16-bit word to every chained device — order doesn't
  // matter here since all devices receive identical data.
  digitalWrite(MAX7219_CS_PIN, LOW);
  for (uint8_t i = 0; i < NUM_MODULES; i++) {
    SPI.transfer(reg);
    SPI.transfer(value);
  }
  digitalWrite(MAX7219_CS_PIN, HIGH);
}

void DisplayManager::setBrightness(uint8_t level) {
  if (level > 15) level = 15;
  sendCommandAll(REG_INTENSITY, level);
}

void DisplayManager::clear() {
  memset(_backBuffer, 0, sizeof(_backBuffer));
}

// Reverses the column order within the lowest MATRIX_WIDTH bits of a
// bitmask — used by the display-flip transform below, since a 180-degree
// rotation mirrors columns as well as rows.
static uint32_t reverseColumnBits(uint32_t mask) {
  uint32_t result = 0;
  for (uint8_t i = 0; i < MATRIX_WIDTH; i++) {
    if (mask & (1UL << i)) {
      result |= (1UL << (MATRIX_WIDTH - 1 - i));
    }
  }
  return result;
}

void DisplayManager::setPixel(uint8_t x, uint8_t y, bool on) {
  if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT) return;
  // Display-flip: a full 180-degree rotation of the whole rendered image,
  // independent of MODULE_CHAIN_REVERSED/MODULE_MIRROR_BITS (which fix
  // wiring-order artifacts, not physical mounting orientation). Applied
  // once here so every draw call (digits, colon, dash, status pixels,
  // seconds bar, error screens) inherits it automatically.
  if (_settings.displayFlipped()) {
    x = MATRIX_WIDTH - 1 - x;
    y = MATRIX_HEIGHT - 1 - y;
  }
  if (on) {
    _backBuffer[y] |= (1UL << x);
  } else {
    _backBuffer[y] &= ~(1UL << x);
  }
}

void DisplayManager::setRow(uint8_t row, uint32_t colBitmask) {
  if (row >= MATRIX_HEIGHT) return;
  if (_settings.displayFlipped()) {
    row = MATRIX_HEIGHT - 1 - row;
    colBitmask = reverseColumnBits(colBitmask);
  }
  _backBuffer[row] = colBitmask;
}

void DisplayManager::orRow(uint8_t row, uint32_t colBitmask) {
  if (row >= MATRIX_HEIGHT) return;
  if (_settings.displayFlipped()) {
    row = MATRIX_HEIGHT - 1 - row;
    colBitmask = reverseColumnBits(colBitmask);
  }
  _backBuffer[row] |= colBitmask;
}

uint8_t DisplayManager::drawDigit(uint8_t x, uint8_t y, uint8_t digit) {
  if (digit > 9) digit = 0;
  for (uint8_t col = 0; col < FONT_DIGIT_WIDTH; col++) {
    uint8_t colBits = pgm_read_byte(&FONT_3X7_DIGITS[digit][col]);
    for (uint8_t row = 0; row < FONT_DIGIT_HEIGHT; row++) {
      bool on = (colBits >> row) & 0x01;
      setPixel(x + col, y + row, on);
    }
  }
  return x + FONT_DIGIT_WIDTH;
}

uint8_t DisplayManager::drawLargeDigit(uint8_t x, uint8_t y, uint8_t digit) {
  if (digit > 9) digit = 0;
  for (uint8_t col = 0; col < FONT_LARGE_DIGIT_WIDTH; col++) {
    uint8_t colBits = pgm_read_byte(&FONT_4X7_DIGITS[digit][col]);
    for (uint8_t row = 0; row < FONT_DIGIT_HEIGHT; row++) {
      setPixel(x + col, y + row, (colBits >> row) & 0x01);
    }
  }
  return x + FONT_LARGE_DIGIT_WIDTH;
}

uint8_t DisplayManager::drawColon(uint8_t x, uint8_t y) {
  uint8_t colBits = pgm_read_byte(&FONT_3X7_COLON[0]);
  for (uint8_t row = 0; row < FONT_DIGIT_HEIGHT; row++) {
    bool on = (colBits >> row) & 0x01;
    setPixel(x, y + row, on);
  }
  return x + FONT_COLON_WIDTH;
}

uint8_t DisplayManager::drawDash(uint8_t x, uint8_t y) {
  for (uint8_t col = 0; col < FONT_DASH_WIDTH; col++) {
    uint8_t colBits = pgm_read_byte(&FONT_3X7_DASH[col]);
    for (uint8_t row = 0; row < FONT_DIGIT_HEIGHT; row++) {
      bool on = (colBits >> row) & 0x01;
      setPixel(x + col, y + row, on);
    }
  }
  return x + FONT_DASH_WIDTH;
}

int16_t DisplayManager::drawTextGlyph(int16_t x, uint8_t y, char c) {
  const uint8_t* glyph = getTextGlyph(c);
  for (uint8_t col = 0; col < FONT_TEXT_WIDTH; col++) {
    int16_t px = x + col;
    // Only touch columns that actually land within the visible matrix —
    // this is what makes it safe to call with a glyph that's partially
    // or fully off-screen (mid-scroll), without needing the caller to
    // clip anything itself.
    if (px >= 0 && px < MATRIX_WIDTH) {
      uint8_t colBits = pgm_read_byte(&glyph[col]);
      for (uint8_t row = 0; row < FONT_TEXT_HEIGHT; row++) {
        bool on = (colBits >> row) & 0x01;
        setPixel((uint8_t)px, y + row, on);
      }
    }
  }
  return x + FONT_TEXT_WIDTH + 1; // +1 for the inter-character gap
}

void DisplayManager::flip() {
  memcpy(_frontBuffer, _backBuffer, sizeof(_backBuffer));
  pushFrontBufferToHardware();
}

uint8_t DisplayManager::rowByteForModule(uint8_t row, uint8_t moduleIndex) const {
  // Extract the 8 columns belonging to moduleIndex (0 = leftmost module)
  // from the full-width row bitmask.
  uint8_t shift = moduleIndex * 8;
  uint8_t raw = (uint8_t)((_frontBuffer[row] >> shift) & 0xFF);

  if (!_settings.moduleMirrorBits()) {
    return raw;
  }

  // Reverse the bit order within the byte — needed because MAX7219 modules
  // commonly wire column 0 to the MSB or LSB depending on the board's
  // physical orientation in the chain. Flip the "Mirror bits" setting in /settings
  // if digits render as horizontal mirror images of themselves.
  uint8_t mirrored = 0;
  for (uint8_t b = 0; b < 8; b++) {
    if (raw & (1 << b)) {
      mirrored |= (1 << (7 - b));
    }
  }
  return mirrored;
}

void DisplayManager::pushFrontBufferToHardware() {
  // For each of the 8 rows, send one 16-bit (register, data) word per
  // module in the chain, ordered so each physical module ends up with the
  // correct byte after the final CS latch. Data shifted in first ends up
  // in the device farthest from the MCU, so we must send the *last*
  // physical module's byte first, and the *first* module's byte last.
  for (uint8_t row = 0; row < MATRIX_HEIGHT; row++) {
    digitalWrite(MAX7219_CS_PIN, LOW);
    for (int8_t chainPos = NUM_MODULES - 1; chainPos >= 0; chainPos--) {
      // chainPos counts down from the far end of the chain to the near end.
      // Map that physical chain position to a logical left-to-right module
      // index, honoring the "Chain reversed" setting if the chain runs right-to-left.
      uint8_t moduleIndex = _settings.moduleChainReversed()
                               ? (NUM_MODULES - 1 - chainPos)
                               : chainPos;
      SPI.transfer(REG_DIGIT0 + row);
      SPI.transfer(rowByteForModule(row, moduleIndex));
    }
    digitalWrite(MAX7219_CS_PIN, HIGH);
  }
}
