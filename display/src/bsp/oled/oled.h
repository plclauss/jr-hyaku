/**
 * @file oled.h
 * @author Paul Clauss (plclauss@gmail.com)
 * @brief Function declarations related to drawing visual data to the OLED.
 * @date 2025-11-23
 */

#ifndef __OLED_H__
#define __OLED_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdbool.h>

#include "bsp/oled/Fonts/Inconsolata_Expanded-Regular5pt7b.h"
#include "bsp/oled/Fonts/Inconsolata_ExtraExpanded-Black6pt7b.h"
#include "bsp/oled/Fonts/Inconsolata_SemiCondensed-Bold4pt7b.h"

/**
 * @brief OLED metadata.
 */
#define SSD1351_DISP_WIDTH (128)
#define SSD1351_DISP_HEIGHT (128)

/**
 * @brief Basic draw-able colors.
 * 
 * Any 16-bit value will work, but these are often useful.
 */
#define WHITE (0xFFFF)
#define BLACK (0x0000)

/**
 * @brief OLED init and de-init functions.
 */
bool oledInit(void);
bool oledDeinit(void);

/**
 * @brief OLED drawing functions.
 */
bool oledClearScreen(void);
bool oledDrawPixel(uint8_t x, uint8_t y, const uint16_t color);
bool oledDrawPoint(uint8_t x, uint8_t y, const uint16_t color);

typedef struct Coordinate {
  int16_t x, y;
} Coordinate;
extern const Coordinate INVALID_COORDINATE;

typedef struct TextMetrics {
  int16_t ascent, descent;
} TextMetrics_t;
Coordinate oledCalcTextBounds(
  const char *str,
  const uint8_t len,
  const GFXfont *font,
  TextMetrics_t *metrics
);

typedef enum {
  SEMI_CONDENSED_4PTBOLD,
  EXPANDED_5PTREGULAR,
  EXTRA_EXPANDED_6PTBLACK,
} FontOptions;
typedef struct TextParameters {
  FontOptions font;
  uint16_t color;

  const char *text;
  int16_t x;
  int16_t y1, y2;
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

/**
 * @brief Misc. helpers
 */
bool oledCoordinateIsInvalid(const Coordinate coord);

bool oledSetMasterContrast(uint8_t contrast);

#ifdef __cplusplus
}
#endif

#endif  // __OLED_H__
