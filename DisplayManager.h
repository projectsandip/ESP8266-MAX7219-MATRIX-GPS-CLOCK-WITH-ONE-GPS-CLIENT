#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "SettingsManager.h"

// Raw MAX7219 driver (no external matrix library) using hardware SPI, so we
// have full control over per-module bit/chain order — needed for the
// horizontal-mirror fix from the earlier TM1637/TFT clock builds.
//
// Framebuffer model: 8 rows x MATRIX_WIDTH columns, stored as one uint32_t
// per row (bit X set = pixel at column X is lit, bit0 = leftmost column).
// Drawing happens into a back buffer; flip() copies it to the front buffer
// and pushes the whole frame to hardware in one pass, so a partially-drawn
// frame is never visible (double-buffered, no flicker/tearing).
//
// Brightness and orientation (MODULE_CHAIN_REVERSED/MODULE_MIRROR_BITS)
// now come from SettingsManager, read live on each call, so they can be
// changed from /settings without reflashing.
class DisplayManager {
public:
  DisplayManager(SettingsManager &settings);

  void begin();

  // --- Drawing API (draws into the back buffer) ---
  void clear();
  void setPixel(uint8_t x, uint8_t y, bool on);
  // Draws a 3x7 digit (0-9) with its top-left corner at (x,y). Returns the
  // x position immediately after the glyph (for easy chaining).
  uint8_t drawDigit(uint8_t x, uint8_t y, uint8_t digit);
  uint8_t drawLargeDigit(uint8_t x, uint8_t y, uint8_t digit);
  uint8_t drawColon(uint8_t x, uint8_t y);
  uint8_t drawDash(uint8_t x, uint8_t y);

  // Draws one glyph from the 5x7 scrolling-text font (see FontText5x7.h)
  // at a *signed* x position, which may be partially or fully off-screen
  // (e.g. mid-scroll) — only pixels that land within the visible matrix
  // are actually set. Returns the x position immediately after this
  // glyph (including the 1px inter-character gap), for chaining, same
  // convention as drawDigit() etc.
  int16_t drawTextGlyph(int16_t x, uint8_t y, char c);

  // Push the back buffer to hardware (and to the front buffer)
  void flip();

  // Direct row access for effects that don't fit the digit-drawing API.
  // row must be 0-7.
  void setRow(uint8_t row, uint32_t colBitmask);
  void orRow(uint8_t row, uint32_t colBitmask);

  void setBrightness(uint8_t level); // 0-15, applies immediately + independent of settings

  // Re-reads settings.matrixBrightness() and applies it — call after a
  // settings save if you want the new brightness to take effect without
  // a reboot (orientation flags are read fresh every flip() already, so
  // no equivalent call is needed for those).
  void refreshBrightnessFromSettings();

private:
  SettingsManager &_settings;
  uint32_t _backBuffer[MATRIX_HEIGHT];
  uint32_t _frontBuffer[MATRIX_HEIGHT];

  void sendCommandAll(uint8_t reg, uint8_t value);
  void pushFrontBufferToHardware();
  uint8_t rowByteForModule(uint8_t row, uint8_t moduleIndex) const;
};

#endif // DISPLAY_MANAGER_H
