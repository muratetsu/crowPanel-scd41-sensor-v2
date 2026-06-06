#ifndef BOOT_LOGO_H
#define BOOT_LOGO_H

#include <TFT_eSPI.h>
#include "logo.h"

/**
 * Draws the user's custom 1-bit indexed logo image on the TFT screen.
 * Dimensions: 240x80
 */
inline void drawBootLogo(TFT_eSPI &tft, uint16_t screenWidth, uint16_t screenHeight) {
  // Set the logo foreground color (Index 0) to custom RGB(255, 220, 80)
  uint16_t color0 = tft.color565(255, 220, 80);
  color0 = (color0 >> 8) | (color0 << 8); // Swap bytes for SPI big-endian
  
  // Extract background color (Index 1) from the image data palette
  uint16_t color1 = tft.color565(logo_map[4], logo_map[5], logo_map[6]);
  color1 = (color1 >> 8) | (color1 << 8); // Swap bytes for SPI big-endian

  // Clear the screen using the background color (Color 1 is the dark color in this image)
  tft.fillScreen(color1);

  // Position logo in the center of the screen
  int16_t startX = (screenWidth - 240) / 2;
  int16_t startY = (screenHeight - 80) / 2;

  // The actual bitmap starts after the 8-byte palette (logo_map + 8)
  const uint8_t *bitmap = logo_map + 8;

  // Temporary buffer to hold one row of pixels (240 pixels, 16-bit RGB565 color per pixel)
  uint16_t rowBuffer[240];

  tft.startWrite();
  for (int16_t y = 0; y < 80; y++) {
    for (int16_t x = 0; x < 240; x++) {
      // Find the byte offset in the 1-bit image data (30 bytes per row of 240 pixels)
      int32_t byteIdx = y * 30 + (x / 8);
      uint8_t bitShift = 7 - (x % 8);
      
      // Extract the bit (0 or 1)
      uint8_t pixelBit = (bitmap[byteIdx] >> bitShift) & 0x01;
      
      // Assign the corresponding color: 0 -> color0, 1 -> color1
      rowBuffer[x] = (pixelBit == 1) ? color1 : color0;
    }
    // Write the compiled row buffer directly to the TFT controller for high performance
    tft.pushImage(startX, startY + y, 240, 1, rowBuffer);
  }
  tft.endWrite();
}

#endif // BOOT_LOGO_H
