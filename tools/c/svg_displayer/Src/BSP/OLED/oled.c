/**
 * @file oled.c
 * @author Paul Clauss (plclauss@gmail.com)
 * @brief Implementations of functions declared in `oled.h`.
 * @date 2025-11-23
 */

#include "oled.h"

#include <string.h>
#include <unistd.h>

#include "BSP/GPIO/gpio.h"
#include "BSP/SPI/spi.h"
#include "Fonts/Inconsolata_Expanded-Regular5pt7b.h"
#include "Fonts/Inconsolata_ExtraExpanded-Black6pt7b.h"
#include "Fonts/Inconsolata_SemiCondensed-Bold4pt7b.h"
#include "oled_cmds.h"

/* ************************* DEFINES / CUSTOM VARS ************************** */

/**
 * @brief The OLED's non-SPI related GPIO pins that we care about.
 */
#ifdef LINUX
#define SSD1351_RST_BCM_PIN (17)
#define SSD1351_DC_BCM_PIN (27)
#endif

/**
 * @brief Metadata regarding the OLED.
 */
#ifdef LINUX
#define LINUX_DFLT_OLED_DEVFILE_FP ("/dev/spidev0.0")  // SPI0, CS0
#define LINUX_DFLT_OLED_SPEED_HZ (8000000)             // 800MHz
static LinuxSPIConfig OLEDSPIConfig = {
  .fd = LINUX_INVALID_FILE_DESCRIPTOR_VAL,
  .devfile = LINUX_DFLT_OLED_DEVFILE_FP,
  .mode = LINUX_DFLT_SPI_MODE,
  .bitsPerWord = LINUX_DFLT_BITS_PER_WORD,
  .speedHz = LINUX_DFLT_OLED_SPEED_HZ,
};
#endif

/**
 * @brief The time to wait before certain signals should become stable; see the
 * "Power ON sequence" of the SSD1351 datasheet.
 *
 * @cite https://newhavendisplay.com/content/app_notes/SSD1351.pdf.
 *
 * @note The documentation recommends a max wait time of 300ms, but we place an
 * upper-bound on it, just in case.
 */
#define SSD1351_POWER_ON_WAIT_TIME_US (500000)  // 500ms

/**
 * @brief Variable(s) to track the state of the OLED.
 */
static bool oledIsInitialized = false;

/**
 * @brief A buffer, representing the GDDRAM of the OLED.
 *
 * Since our OLED is 128x128 pixels, w/ 16-bit color, we speicy 128x128 buffer
 * w/ 16-bit values.
 *
 * @note Communicating w/ the OLED occurs via SPI. We use an 8-bit standard for
 * SPI, so any call to `oledSendData(...)` must ensure to convert the
 * `dispBuffer` to 8-bit.
 *
 * That is to say, if you're writing a single pixel value, for instance, you
 * must send something like this, instead:
 *
 * const uint8_t arr[] = {dispBuffer[pixel] & 0xFF00 >> 8, dispBuffer[pixel]}.
 */
#define SSD1351_BUFFER_SIZE_B (SSD1351_DISP_WIDTH * SSD1351_DISP_HEIGHT)
static uint16_t dispBuffer[SSD1351_BUFFER_SIZE_B] = {0};
#define SSD1351_ACCESS_PIXEL(x, y) ((y)*SSD1351_DISP_WIDTH + (x))

/**
 * @brief Misc. defines to help w/ communication b/w the OLED via SPI.
 */
#define SSD1351_MAX_RW_SIZE_B (SSD1351_BUFFER_SIZE_B * sizeof(uint16_t))

#define GET_HIGH_BYTE(byte) ((uint8_t)((byte & 0xFF00) >> 8))
#define GET_LOW_BYTE(byte) ((uint8_t)(byte & 0x00FF))

#define MIN_VALUE(a, b) ((a) > (b) ? (b) : (a))
#define MAX_VALUE(a, b) ((a) > (b) ? (a) : (b))
#define SWAP_VALUE(a, b)  \
  {                       \
    const uint16_t t = a; \
    a = b;                \
    b = t;                \
  }

/**
 * @brief Misc. to help w/ drawing.
 */

/**
 * @brief Used to represent an arbitrary (X, Y) coordinate within the OLED's
 * coordinate system.
 *
 * A value of (UINT8_MAX, UINT8_MAX) is used to indicate errors / an invalid
 * coordinate.
 */
typedef struct Coordinate {
  int16_t x, y;
} Coordinate;

// Padding from the borders of the OLED, to create cleaner displays.
// Note: This value must be applied to all sides; e.g., the actual drawable with
// is (SSD1351_DISP_WIDTH - (2 * SSD1351_DISP_PADDING_PX)).
#define SSD1351_DISP_PADDING_PX (5)

/* ************************ STATIC FUNCTION PROTOS. ************************* */

// Low-Level Functions
static bool oledSendCommand(const uint8_t cmd);
static bool oledSendData(const uint8_t *data, const int32_t dataSize);

// Drawing Helpers
static bool oledSetDrawingRegion(
  const uint8_t x1,
  const uint8_t y1,
  const uint8_t w,
  const uint8_t h);

static bool oledStringIsValid(const char *str, const GFXfont *font);
static const char *oledTokenizeString(const char *str, const char delim);
static Coordinate
oledCalcStringBounds(const char *str, const uint8_t len, const GFXfont *font);

/* ********************** STATIC FUNCTION DEFINITIONS *********************** */

/**
 * @brief Writes a command to the OLED.
 *
 * @param cmd The command to write to the OLED.
 *
 * @return True, if the command was successfully sent; false, otherwise.
 *
 * @note This function sets the OLED's D/C pin low, and does NOT set it high
 * afterwards.
 */
static bool oledSendCommand(const uint8_t cmd) {
  // Inform OLED that next byte is a command byte.
  if (!gpioWritePin(SSD1351_DC_BCM_PIN, PIN_LOW)) return false;

  // Send command byte.
  return spiTransferData(OLEDSPIConfig, &cmd, NULL, sizeof(cmd), NULL);
}

/**
 * @brief Writes data to the OLED.
 *
 * @param[in] data The data to write to the OLED.
 *
 * @param dataSize The size, in bytes, of the data provided.
 *
 * @return True, if the data was successfully sent; false, otherwise.
 *
 * @note This function sets the OLED's D/C pin high, and does NOT set it low
 * afterwards.
 */
static bool oledSendData(const uint8_t *data, const int32_t dataSize) {
  // Inform OLED that next byte(s) is/are data byte(s).
  if (!gpioWritePin(SSD1351_DC_BCM_PIN, PIN_HIGH)) return false;

  // Send data byte(s).
  return spiTransferData(OLEDSPIConfig, data, NULL, dataSize, NULL);
}

/**
 * @brief Sets the OLED's drawing region.
 *
 * This function takes advantage of a unique feature of the OLED, which allows
 * us to write only to the area we desire. This prevents us from writing data to
 * the entire drawing area of the OLED.
 *
 * @param x1 The starting x-coordinate of the drawing region.
 *
 * @param y1 The starting y-coordinate of the drawing region.
 *
 * @param w The width of the drawing region (effectively delta_x).
 *
 * @param h The height of the drawing region (effectively delta_y).
 *
 * @return True, if the drawing region was established (, and so we may begin
 * drawing data via oledSendData(...)); false, otherwise.
 */
static bool oledSetDrawingRegion(
  const uint8_t x1,
  const uint8_t y1,
  const uint8_t w,
  const uint8_t h) {
  // If the drawing region is out-of-bounds, clamp it to the display's size.
  // Note: We -1, since the draw-able region is [0, 127].
  uint8_t x2, y2;
  if (((uint16_t)(x1 + w)) >= SSD1351_DISP_WIDTH) {
    x2 = SSD1351_DISP_WIDTH - 1;
  } else {
    x2 = (x1 + (w - 1));
  }

  if (((uint16_t)(y1 + h)) >= SSD1351_DISP_HEIGHT) {
    y2 = SSD1351_DISP_HEIGHT - 1;
  } else {
    y2 = (y1 + (h - 1));
  }

  // Set drawing region, and return.
  const uint8_t xCoords[] = {x1, x2};
  const uint8_t yCoords[] = {y1, y2};
  if (!oledSendCommand(OLED_CMD_SET_COL_ADDR)) return false;
  if (!oledSendData(xCoords, sizeof(xCoords))) return false;
  if (!oledSendCommand(OLED_CMD_SET_ROW_ADDR)) return false;
  return oledSendData(yCoords, sizeof(yCoords));
}

/**
 * @brief Determines whether all characters within a string are drawable.
 *
 * @param[in] str The desired string to draw to the OLED.
 *
 * @param[in] font The font the string will be drawn in, if valid.
 *
 * @return True, if all characters within the string are supported by the
 * requested font; false, otherwise.
 */
static bool oledStringIsValid(const char *str, const GFXfont *font) {
  // Input Validation
  if (!str || !font) return false;

  for (uint8_t idx = 0; str[idx] != '\0'; idx += 1) {
    const char c = str[idx];
    if (c != '\n' && (c < font->first || c > font->last)) return false;
  }
  return true;
}

/**
 * @brief A simplified / combined version of strtok(...) and strchr(...).
 *
 * Used in oledDrawString(...) to find the starting and ending indices of a
 * substring. This is useful when interpreting strings with newline characters
 * for proper drawing.
 *
 * @param[in] str The string to tokenize.
 *
 * @param delim The character to search for in the string.
 *
 * @return A pointer to the first character in the string which is equal to the
 * delimeter value; OR to the end of the string, if no such character was found.
 *
 * In the event of an error, NULL is returned.
 */
static const char *oledTokenizeString(const char *str, const char delim) {
  // Input Validation
  if (!str) return NULL;

  char c;
  uint8_t idx = 0;
  while ((c = str[idx]) != '\0') {
    if (c == delim) break;
    idx += 1;
  }
  return (str + idx);
}

/**
 * @brief Calculates the bounding box of a string by calculating its
 * pixel-width and -height.
 *
 * @param[in] str The string whose pixel-width and -height will be calculated.
 *
 * @param len The length, in bytes, of the input string.
 *
 * @param[in] font The font that the string will be drawn in.
 *
 * The font may change the bounding box, so it's relevant in this context.
 *
 * @return An (X, Y) coordinate, defining the bottom-right corner of the
 * bounding box; (UINT8_MAX, UINT8_MAX), otherwise.
 *
 * @note This function assumes that the top-left corner of the box is at (0, 0).
 */
static Coordinate
oledCalcStringBounds(const char *str, const uint8_t len, const GFXfont *font) {
  // Input Validation
  const Coordinate INVALID_COORDINATE = {UINT8_MAX, UINT8_MAX};
  if (!str || !len || !font) return INVALID_COORDINATE;

  Coordinate coord = {0, 0};
  int16_t leftmostPx = 0, rightmostPx = 0;
  int16_t topmostPx = 0, bottommostPx = 0;
  for (uint8_t idx = 0; idx < len; idx += 1) {
    const char c = str[idx];
    const GFXglyph *glyph = (c != '\n') ? &font->glyph[c - font->first] : NULL;

    if (glyph) {
      /* X-Coordinate */
      const int16_t charXULCorner = (coord.x + glyph->xOffset);
      const int16_t charXURCorner = (charXULCorner + glyph->width);
      leftmostPx = MIN_VALUE(leftmostPx, charXULCorner);
      rightmostPx = MAX_VALUE(rightmostPx, charXURCorner);

      coord.x += glyph->xAdvance;

      /* Y-Coordinate */
      const int16_t charYULCorner = (coord.y + glyph->yOffset);
      const int16_t charYBLCorner = (charYULCorner + glyph->height);
      topmostPx = MIN_VALUE(topmostPx, charYULCorner);
      bottommostPx = MAX_VALUE(bottommostPx, charYBLCorner);
    } else {
      coord.x = 0;
      coord.y += font->yAdvance;
    }
  }

  Coordinate end = {rightmostPx, bottommostPx};
  if (leftmostPx < 0) end.x += (-leftmostPx);
  if (topmostPx < 0) end.y += (-topmostPx);
  return end;
}

/* ************************** FUNCTION DEFINITIONS ************************** */

/**
 * @brief Lazily initialies the OLED, including all GPIO pins required to
 * interface it.
 *
 * @return True, if initialization succeeded; false, otherwise.
 *
 * @note The display is lazily-initialized, and is cleared (to a black screen)
 * on a successful initialization.
 */
bool oledInit(void) {
  // TODO: Check if OLED is already initialized.
  //    - Simple check against oledIsInitialized...
  //    - But, need to add a check for initialization in spiInit(...).

  // Initialize the OLED's SPI interface.
#ifdef LINUX
  if (!spiInit(&OLEDSPIConfig)) return false;
#else
  return false;
#endif

  // Initialize all GPIOs.
  if (!gpioInitPin(SSD1351_RST_BCM_PIN, PINCONF_OUTPUT)) return false;
  if (!gpioInitPin(SSD1351_DC_BCM_PIN, PINCONF_OUTPUT)) return false;

  // Perform Power On sequence (according to SSD1351 docs).
  if (!gpioWritePin(SSD1351_RST_BCM_PIN, PIN_HIGH)) return false;
  usleep(SSD1351_POWER_ON_WAIT_TIME_US);
  if (!gpioWritePin(SSD1351_RST_BCM_PIN, PIN_LOW)) return false;
  usleep(SSD1351_POWER_ON_WAIT_TIME_US);
  if (!gpioWritePin(SSD1351_RST_BCM_PIN, PIN_HIGH)) return false;
  usleep(SSD1351_POWER_ON_WAIT_TIME_US);

  // Apply customizations to the OLED's configuration.
  // Optional Additional Commands:
  //    - Clock Divider / Oscillator Freq. - For power consumption/refresh rate.
  //    - Precharge Period -  Changes OLED capacitance for long/short discharge.
  //    - Contract - If OLED is too dim/bright.
  //    - Command Lock - For re-locking OLED's receptiveness to commands.
  const uint8_t oledSettings[] = {
    SSD1351_CMD_DISP_OFF,
    0,
    SSD1351_CMD_CMDLOCK,
    1,
    0x12,  // Unlock OLED driver from entering commands.
    SSD1351_CMD_CMDLOCK,
    1,
    0xB1,  // Force certain commands to be accessible in unlock state.
    SSD1351_CMD_SET_DISP_OFFSET,
    1,
    0x00,
    SSD1351_CMD_SET_DISP_GPIOS,
    1,
    0x00,
    SSD1351_CMD_SET_REMAP,
    1,
    0x74,  // Swap Color Sequence & Invert COM scan.
    SSD1351_CMD_DISP_ON,
    0,
  };

  uint8_t byte = 0;
  const uint8_t numSettings = sizeof(oledSettings);
  while (byte < numSettings) {
    // Extract relevant cmd information.
    const uint8_t cmd = oledSettings[byte++];
    const uint8_t numCmdParams = oledSettings[byte++];
    const uint8_t *params = oledSettings + byte;

    // Send the command byte first...
    if (!oledSendCommand(cmd)) return false;

    // ...Followed by the parameter data, if any.
    if (numCmdParams && !oledSendData(params, numCmdParams)) return false;

    // Update parsing variables.
    byte += numCmdParams;
  }

  // Clear the display, and return accordingly.
  // Note: To clear the display, we artificially set the initialization status
  // of the OLED to true. It is reset on failure, as necessary.
  oledIsInitialized = true;
  if (!oledClearScreen() || !oledUpdateDisplay()) {
    oledIsInitialized = false;
    return false;
  }
  return true;
}

/**
 * @brief Clears the OLED (to all BLACK).
 *
 * @return True, if the buffer was cleared successfully; false, otherwise.
 *
 * @note Changes will not be visible until oledUpdateDisplay() is invoked.
 */
bool oledClearScreen(void) {
  // Ensure the display is initialized before drawing to it.
  if (!oledIsInitialized) return false;

  // Clear the display buffer, and return.
  memset(dispBuffer, BLACK, sizeof(dispBuffer));
  return true;
}

/**
 * @brief Draws a singular pixel to the GDDRAM buffer.
 *
 * @param x The desired x-coordinate of the pixel.
 *
 * @param y The desired y-coordinate of the pixel.
 *
 * @param color The desired color of the pixel.
 *
 * @return True, if the pixel was drawn successfully; false, otherwise.
 *
 * @note Changes will not be visible until oledUpdateDisplay() is invoked.
 */
bool oledDrawPixel(uint8_t x, uint8_t y, const uint16_t color) {
  // Ensure the display is initialized before drawing to it.
  if (!oledIsInitialized) return false;

  // If the (x, y) coordinate is out-of-bounds, clamp it to the display's size.
  if (x >= SSD1351_DISP_WIDTH) x = (SSD1351_DISP_WIDTH - 1);
  if (y >= SSD1351_DISP_HEIGHT) y = (SSD1351_DISP_HEIGHT - 1);

  // Draw the pixel to the framebuffer, and return.
  dispBuffer[SSD1351_ACCESS_PIXEL(x, y)] = color;
  return true;
}

/**
 * @brief Draws a horizontal line to the GDDRAM buffer.
 *
 * @param x The starting x-coordinate of the line.
 *
 * @param y The starting y-coordinate of the line.
 *
 * @param w The desired length of the line, in px.
 *
 * @param color The desired color of the line.
 *
 * @return True, if the line was drawn successfully; false, otherwise.
 */
bool oledDrawHLine(uint8_t x, uint8_t y, uint8_t w, const uint16_t color) {
  // Input Validation
  if (w == 0) return false;  // Nothing to draw.

  // Ensure the display is initialized before drawing to it.
  if (!oledIsInitialized) return false;

  // If the coordinates are out-of-bounds, clamp them to the display's size.
  if (x >= SSD1351_DISP_WIDTH) x = (SSD1351_DISP_WIDTH - 1);
  if (y >= SSD1351_DISP_HEIGHT) y = (SSD1351_DISP_HEIGHT - 1);
  if (w >= (SSD1351_DISP_WIDTH - x)) w = ((SSD1351_DISP_WIDTH - x) - 1);

  // Draw the line to the framebuffer/display.
  for (; w > 0; x++, w--) {
    if (!oledDrawPixel(x, y, color)) return false;
  }
  return true;
}

/**
 * @brief Draws a vertical line to the GDDRAM buffer.
 *
 * @param x The starting x-coordinate of the line.
 *
 * @param y The starting y-coordinate of the line.
 *
 * @param h The desired length of the line, in px.
 *
 * @param color The desired color of the line.
 *
 * @return True, if the line was drawn successfully; false, otherwise.
 */
bool oledDrawVLine(uint8_t x, uint8_t y, uint8_t h, const uint16_t color) {
  // Input Validation
  if (h == 0) return false;  // Nothing to draw.

  // Ensure the display is initialized before drawing to it.
  if (!oledIsInitialized) return false;

  // If the coordinates are out-of-bounds, clamp them to the display's size.
  if (x >= SSD1351_DISP_WIDTH) x = (SSD1351_DISP_WIDTH - 1);
  if (y >= SSD1351_DISP_HEIGHT) y = (SSD1351_DISP_HEIGHT - 1);
  if (h >= (SSD1351_DISP_HEIGHT - y)) h = ((SSD1351_DISP_WIDTH - y) - 1);

  // Draw the line to the framebuffer/display.
  for (; h > 0; y++, h--) {
    if (!oledDrawPixel(x, y, color)) return false;
  }
  return true;
}

/**
 * @brief Draws a square to the GDDRAM buffer.
 *
 * @param x1 The x-coordinate of the top-left corner of the square.
 *
 * @param y1 The y-coordinate of the top-left corner of the square.
 *
 * @param x2 The x-coordinate of the bottom-right corner of the square.
 *
 * @param y2 The y-coordinate of the bottom-right corner of the square.
 *
 * @param color The desired color of the square.
 *
 * @param fill A boolean, which determines whether the square is filled.
 *
 * @return True, if the square was drawn successfully; false, otherwise.
 */
bool oledDrawSquare(
  uint8_t x1,
  uint8_t y1,
  uint8_t x2,
  uint8_t y2,
  const uint16_t color,
  const bool fill) {
  // Ensure the display is initialized before drawing to it.
  if (!oledIsInitialized) return false;

  // If the coordinates are out-of-bounds, clamp it to the display's size.
  if (x1 >= SSD1351_DISP_WIDTH) x1 = (SSD1351_DISP_WIDTH - 1);
  if (x2 >= SSD1351_DISP_WIDTH) x2 = (SSD1351_DISP_WIDTH - 1);
  if (y1 >= SSD1351_DISP_HEIGHT) y1 = (SSD1351_DISP_HEIGHT - 1);
  if (y2 >= SSD1351_DISP_HEIGHT) y2 = (SSD1351_DISP_HEIGHT - 1);

  // Swap coordinates, if necessary.
  if (x2 < x1) SWAP_VALUE(x1, x2);
  if (y2 < y1) SWAP_VALUE(y1, y2);

  // Draw the square to the framebuffer, and to the display.
  const uint8_t w = ((x2 - x1) + 1);
  if (!oledDrawHLine(x1, y1, w, color)) return false;
  if (!oledDrawHLine(x1, y2, w, color)) return false;

  const uint8_t h = ((y2 - y1) + 1);
  if (!oledDrawVLine(x1, y1, h, color)) return false;
  if (!oledDrawVLine(x2, y1, h, color)) return false;

  // If the square is to be filled, draw those pixels, as well.
  if (fill) {
    for (uint8_t x = x1 + 1; x < x2; x += 1) {
      for (uint8_t y = y1 + 1; y < y2; y += 1) {
        if (!oledDrawPixel(x, y, color)) return false;
      }
    }
  }

  return true;
}

/**
 * @brief Draws a string to the GDDRAM buffer.
 *
 * @param textParams A struct, containing information about how the text should
 * be drawn to the OLED:
 *    -   font - An enum representing which of the supported fonts to use;
 *    -      x - The starting x-value of the text to draw;
 *    -     y1 - The y-value of the top of the bounding box for the text;
 *    -     y2 - The y-value of the bottom of the bounding box for the text; and
 *    - center - A boolean, deciding whether to center the text horizontally.
 *
 * Note: If .center is set to true, the .x value is effectively ignored.
 *
 * @return True, if the text was drawn to the OLED successfully; false,
 * otherwise.
 *
 * @note If the text to draw would've exceeded the right-side border of the
 * OLED, the string is clipped to fit (w/ an ellipsis).
 *
 * If the text exceeded any other border, this function fails to draw the text.
 *
 * @note All text is padded by some amount; see the function details below.
 */
bool oledDrawString(TextParameters textParams) {
  // Input Validation
  if (!textParams.text) return false;

  // Clamp the text coordinates to w/in the display's border.
  const uint8_t _DISP_WIDTH =
    (SSD1351_DISP_WIDTH - (2 * SSD1351_DISP_PADDING_PX));
  const uint8_t _DISP_HEIGHT =
    (SSD1351_DISP_HEIGHT - (2 * SSD1351_DISP_PADDING_PX));

  /* X-Coordinates */
  if (textParams.x < SSD1351_DISP_PADDING_PX) {
    textParams.x = (SSD1351_DISP_PADDING_PX + 1);
  } else if (textParams.x >= _DISP_WIDTH) {
    textParams.x = (_DISP_WIDTH - 1);
  }

  /* Y-Coordinates */
  if (textParams.y1 < SSD1351_DISP_PADDING_PX) {
    textParams.y1 = (SSD1351_DISP_PADDING_PX + 1);
  } else if (textParams.y1 >= _DISP_HEIGHT) {
    textParams.y1 = (_DISP_HEIGHT - 1);
  }

  if (textParams.y2 < SSD1351_DISP_PADDING_PX) {
    textParams.y2 = (SSD1351_DISP_PADDING_PX + 1);
  } else if (textParams.y2 >= _DISP_HEIGHT) {
    textParams.y2 = (_DISP_HEIGHT - 1);
  }

  if (textParams.y1 == textParams.y2) {
    return false;
  } else if (textParams.y2 < textParams.y1) {
    SWAP_VALUE(textParams.y1, textParams.y2);
  }

  // Ensure the display is initialized before drawing to it.
  if (!oledIsInitialized) return false;

  // Initialize some variables to help w/ drawing the string.
  const GFXfont *font;
  switch (textParams.font) {
    case SEMI_CONDENSED_4PTBOLD: {
      font = &Inconsolata_SemiCondensed_Bold4pt7b;
      break;
    }
    case EXPANDED_5PTREGULAR: {
      font = &Inconsolata_Expanded_Regular5pt7b;
      break;
    }
    case EXTRA_EXPANDED_6PTBLACK: {
      font = &Inconsolata_ExtraExpanded_Black6pt7b;
      break;
    }
    default: {
      return false;
    }
  }

  Coordinate cursor = {0, 0};
  int16_t textWidth = 0;
  int16_t textHeight = 0;

  const char *text = textParams.text;

  // Ensure all characters are supported before drawing.
  if (!oledStringIsValid(text, font)) return false;

  // Determine exact width / height of text to draw.
  // Note: Assumes non-centered starting cursor of (0, 0) to make calculation.
  Coordinate boundingBox = oledCalcStringBounds(text, strlen(text), font);
  if (boundingBox.x == UINT8_MAX || boundingBox.y == UINT8_MAX) return false;

  textWidth = (boundingBox.x + 1);
  textHeight = (boundingBox.y + 1);

  // Determine whether text can fit w/in bounding box:
  // This really only applies to the vertical constraints, as we clip text if
  // it cannot fit horizontally.
  /* Horizontal Bounds Check */
  ;

  /* Vertical Bounds Check */
  const uint8_t boxHeight = textParams.y2 - textParams.y1;
  if (textHeight > boxHeight) return false;

  // Draw the text to the GDDRAM buffer, tokenizing against newline chars.
  uint8_t tokenIdx = 0;
  bool finishedParsing = false;
  cursor.y = (textParams.y1 + ((boxHeight - textHeight) / 2));
  while (!finishedParsing) {
    // Extract the next token, and ...
    const char *tokenStart = text + tokenIdx;
    const char *tokenEnd = oledTokenizeString(tokenStart, '\n');
    uint8_t tokenLen = tokenEnd - tokenStart;

    if ((*tokenEnd) == '\0') finishedParsing = true;

    // ...Draw token to GDDRAM buffer.
    /* Calculate token's dimensions; useful for text-clipping and centering. */
    boundingBox = oledCalcStringBounds(tokenStart, tokenLen, font);
    if (boundingBox.x == UINT8_MAX || boundingBox.y == UINT8_MAX) return false;

    int16_t tokenWidth = (boundingBox.x + 1);
    int16_t tokenHeight = (boundingBox.y + 1);

    /* Determine cursor's starting x-coord. */
    if (!textParams.center) {
      cursor.x = textParams.x;
    } else {
      const int16_t effectiveWidth = MIN_VALUE(tokenWidth, _DISP_WIDTH);
      cursor.x = ((_DISP_WIDTH - effectiveWidth) / 2) + SSD1351_DISP_PADDING_PX;
    }

    /* Determine whether clipping is necessary. */
    int16_t hyphenWidth = 0;
    const int16_t rightEdge = (SSD1351_DISP_WIDTH - SSD1351_DISP_PADDING_PX);
    const bool needsHyphen = ((cursor.x + tokenWidth) > rightEdge);
    if (needsHyphen) {
      if ('-' < font->first || '-' > font->last) return false;
      const char *hyphen = "-";
      boundingBox = oledCalcStringBounds(hyphen, 1, font);
      if (boundingBox.x == UINT8_MAX || boundingBox.y == UINT8_MAX) {
        return false;
      }

      hyphenWidth = (boundingBox.x + 1);
    }

    /* Draw token. */
    for (; tokenLen > 0; tokenIdx += 1, tokenLen -= 1) {
      // Load charset info.
      const char c = text[tokenIdx];
      const uint8_t *bitmap = font->bitmap;
      const GFXglyph *glyph = &font->glyph[c - font->first];

      // Draw hyphen, if there's not enough room for the current character.
      const int16_t remWidth = (rightEdge - cursor.x);
      const int16_t remWidthAfterCurr = (remWidth - glyph->xAdvance);
      if (needsHyphen && remWidthAfterCurr <= hyphenWidth) {
        glyph = &font->glyph['-' - font->first];

        uint16_t bo = glyph->bitmapOffset;
        const uint8_t w = glyph->width;
        const uint8_t h = glyph->height;
        const int8_t xo = glyph->xOffset;
        const int8_t yo = glyph->yOffset;

        uint8_t bit = 0, bits = 0;
        for (uint8_t yy = 0; yy < h; yy++) {
          for (uint8_t xx = 0; xx < w; xx++) {
            if (!(bit++ & 7)) bits = bitmap[bo++];

            if (
              (bits & 0x80) &&
              !oledDrawPixel(
                cursor.x + xo + xx,
                cursor.y + yo + yy,
                textParams.color)
            ) {
              return false;
            }

            bits <<= 1;
          }
        }

        break; // Out of for-loop.
      }
      // Otherwise, draw the current character.
      else {
        uint16_t bo = glyph->bitmapOffset;
        const uint8_t w = glyph->width;
        const uint8_t h = glyph->height;
        const int8_t xo = glyph->xOffset;
        const int8_t yo = glyph->yOffset;

        uint8_t bit = 0, bits = 0;
        for (uint8_t yy = 0; yy < h; yy++) {
          for (uint8_t xx = 0; xx < w; xx++) {
            if (!(bit++ & 7)) bits = bitmap[bo++];

            if (
              (bits & 0x80) &&
              !oledDrawPixel(
                cursor.x + xo + xx,
                cursor.y + yo + yy,
                textParams.color)
              ) {
              return false;
            }

            bits <<= 1;
          }
        }
      }

      // Advance the cursor by the character's advance value.
      cursor.x += glyph->xAdvance;
    }

    // Update variables.
    tokenIdx += 1;
    cursor.y += font->yAdvance;
  }

  return true;
}

/**
 * @brief Draws an image to the GDDRAM buffer.
 *
 * It is expected that the image is a square _EXPEC_IMG_DIMS_PX image. The image
 * is always centered horizontally, and is drawn ..._DISP_PADDING_PX from top.
 *
 * It is hard-coded this way, since this is only ever really used for drawing
 * the song art when the music is playing. Since the design is the same, we
 * impose these restrictions.
 *
 * @param[in] image The RGB565 image data to draw to the screen.
 *
 * Any image will work, so long as it's ((...IMG_DIMS_PX^2) *2), but it may look
 * funky, depending on the encoding/if there's compression.
 *
 * @param imageSize The size of the image, in bytes.
 *
 * This should be equiv. to ((...IMG_DIMS_PX^2) * 2).
 *
 * @return True, if the image was drawn successfully; false, otherwise.
 */
bool oledDrawImage(const uint8_t *image, const int32_t imageSize) {
  // Input Validation
  static const int32_t _EXPEC_IMG_DIMS_PX = 81;
  static const int32_t _EXPEC_IMG_SIZE =
    (_EXPEC_IMG_DIMS_PX * _EXPEC_IMG_DIMS_PX * 2);
  if (!image || imageSize != _EXPEC_IMG_SIZE) return false;

  // Draw the image to the buffer, and to the OLED.
  static const uint8_t _IMG_XOFFSET_PX =
    ((SSD1351_DISP_WIDTH - _EXPEC_IMG_DIMS_PX) / 2);
  static const uint8_t _IMG_YOFFSET_PX = SSD1351_DISP_PADDING_PX;
  for (int32_t y = 0; y < _EXPEC_IMG_DIMS_PX; y += 1) {
    for (int32_t x = 0; x < _EXPEC_IMG_DIMS_PX; x += 1) {
      const int32_t imgIdx = (2 * (y * _EXPEC_IMG_DIMS_PX + x));
      const uint16_t imgPx = (image[imgIdx] << 8) | (image[imgIdx + 1]);
      dispBuffer[SSD1351_ACCESS_PIXEL(
        x + _IMG_XOFFSET_PX, y + _IMG_YOFFSET_PX)] = imgPx;
    }
  }

  return true;
}

/**
 * @brief Updates the OLED with the content in the static, global GDDRAM buffer.
 *
 * This function updates the entire display. To update a smaller area, call:
 * oledUpdateDisplayArea(...);
 *
 * @return True, if the OLED was updated successfully; false, otherwise.
 */
bool oledUpdateDisplay(void) {
  // Ensure the display is initialized before drawing to it.
  if (!oledIsInitialized) return false;

  // Update the entire display.
  if (
    !oledSetDrawingRegion(0, 0, SSD1351_DISP_WIDTH, SSD1351_DISP_HEIGHT) ||
    !oledSendCommand(SSD1351_CMD_WRITE_RAM) ||
    !oledSendData((uint8_t *)dispBuffer, SSD1351_MAX_RW_SIZE_B)) {
    return false;
  }

  return true;
}

/**
 * @brief Turns the OLED screen off.
 *
 * @return True, if the OLED's screen was turned off successfully, OR if the
 * OLED wasn't initialized at all; false, otherwise.
 */
bool oledDeinit(void) {
  if (!oledIsInitialized) return true;

  const bool deinitStatus =
    (oledClearScreen() && oledUpdateDisplay() &&
     oledSendCommand(SSD1351_CMD_DISP_OFF));
  if (deinitStatus) oledIsInitialized = false;
  return deinitStatus;
}
