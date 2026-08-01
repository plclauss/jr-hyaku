/**
 * @file oled.h
 * @author Paul Clauss (plclauss@gmail.com)
 * @brief Function declarations related to drawing visual data to the OLED.
 * @date 2025-11-23
 */

#ifndef __OLED_H__
#define __OLED_H__

#include <inttypes.h>
#include <stdbool.h>

#define SSD1351_DISP_WIDTH (128)
#define SSD1351_DISP_HEIGHT (128)

#define WHITE (0xFFFF)
#define BLACK (0x0000)

// Initializers / Deinitializers
bool oledInit(void);
bool oledDeinit(void);

// Drawing Functions
bool oledClearScreen(void);

bool oledDrawPixel(uint8_t x, uint8_t y, const uint16_t color);
bool oledDrawHLine(uint8_t x, uint8_t y, uint8_t w, const uint16_t color);
bool oledDrawVLine(uint8_t x, uint8_t y, uint8_t h, const uint16_t color);
bool oledDrawSquare(
  uint8_t x1,
  uint8_t y1,
  uint8_t x2,
  uint8_t y2,
  const uint16_t color,
  const bool fill);

typedef enum {
  SEMI_CONDENSED_4PTBOLD,
  EXPANDED_5PTREGULAR,
  EXTRA_EXPANDED_6PTBLACK,
} FontOptions;
typedef struct TextParameters {
  FontOptions font;
  uint16_t color;

  char *text;
  uint8_t x;
  uint8_t y1, y2;
  bool center;
} TextParameters;
bool oledDrawString(const TextParameters textParams);

typedef struct ImageParameters {
  uint8_t *image;
  uint32_t width, height;
  uint32_t x, y;
} ImageParameters;
bool oledDrawImage(ImageParameters imageParams);

bool oledUpdateDisplay(void);

#endif  // __OLED_H__
