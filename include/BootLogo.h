#ifndef BOOT_LOGO_H
#define BOOT_LOGO_H

#include <TFT_eSPI.h>
#include "logo.h"

/**
 * Draws the user's custom 2-bit indexed logo image on the TFT screen.
 * Dimensions: 240x80
 */
inline void drawBootLogo(TFT_eSPI &tft, uint16_t screenWidth, uint16_t screenHeight) {
  // Extract index colors from the first 16 bytes of logo_map (RGBA format, ignore Alpha for TFT)
  // and swap bytes for SPI big-endian
  
  uint16_t color0 = tft.color565(logo_map[0], logo_map[1], logo_map[2]);
  color0 = (color0 >> 8) | (color0 << 8);
  
  uint16_t color1 = tft.color565(logo_map[4], logo_map[5], logo_map[6]);
  color1 = (color1 >> 8) | (color1 << 8);
  
  uint16_t color2 = tft.color565(logo_map[8], logo_map[9], logo_map[10]);
  color2 = (color2 >> 8) | (color2 << 8);
  
  uint16_t color3 = tft.color565(logo_map[12], logo_map[13], logo_map[14]);
  color3 = (color3 >> 8) | (color3 << 8);

  // Clear the screen using the background color (Index 3)
  tft.fillScreen(color3);

  // Position logo in the center of the screen
  int16_t startX = (screenWidth - 240) / 2;
  int16_t startY = (screenHeight - 80) / 2;

  // The actual bitmap starts after the 16-byte palette (logo_map + 16)
  const uint8_t *bitmap = logo_map + 16;

  // Temporary buffer to hold one row of pixels (240 pixels)
  uint16_t rowBuffer[240];

  tft.startWrite();
  for (int16_t y = 0; y < 80; y++) {
    for (int16_t x = 0; x < 240; x++) {
      // Find the byte offset in the 2-bit image data (60 bytes per row of 240 pixels)
      int32_t byteIdx = y * 60 + (x / 4);
      uint8_t bitShift = 6 - 2 * (x % 4);
      
      // Extract the 2-bit value (0 to 3)
      uint8_t pixelVal = (bitmap[byteIdx] >> bitShift) & 0x03;
      
      // Assign the corresponding color
      switch (pixelVal) {
        case 0: rowBuffer[x] = color0; break;
        case 1: rowBuffer[x] = color1; break;
        case 2: rowBuffer[x] = color2; break;
        case 3: rowBuffer[x] = color3; break;
      }
    }
    // Write the row buffer directly to the TFT controller for high performance
    tft.pushImage(startX, startY + y, 240, 1, rowBuffer);
  }
  tft.endWrite();
}

#endif // BOOT_LOGO_H
