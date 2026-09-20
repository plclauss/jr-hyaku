/**
 * @file oled.c
 * @author Paul Clauss (plclauss@gmail.com)
 * @brief Implementations of functions declared in `oled.h`.
 * @date 2025-11-23
 */

#include "oled.h"

#include <string.h>
#include <unistd.h>

#include "bsp/gpio/gpio.h"
#include "bsp/oled/oled_cmds.h"
#include "bsp/spi/spi.h"

/* ************************* DEFINES / CUSTOM VARS ************************** */

/**
 * @brief The OLED's non-SPI related GPIO pins that we care about.
 */
#define SSD1351_RST_BCM_PIN (17)
#define SSD1351_DC_BCM_PIN (27)

/**
 * @brief Metadata regarding the OLED.
 */
#define LINUX_DFLT_OLED_DEVFILE_FP ("/dev/spidev0.0")  // SPI0, CS0
#define LINUX_DFLT_OLED_SPEED_HZ (8000000)             // 8MHz
static SPIConfig OLEDSPIConfig = {
    .fd = LINUX_INVALID_FILE_DESCRIPTOR_VAL,
    .devfile = LINUX_DFLT_OLED_DEVFILE_FP,
    .mode = LINUX_DFLT_SPI_MODE,
    .bitsPerWord = LINUX_DFLT_BITS_PER_WORD,
    .speedHz = LINUX_DFLT_OLED_SPEED_HZ,
};

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
const Coordinate INVALID_COORDINATE = { UINT8_MAX, UINT8_MAX };

/* ************************ STATIC FUNCTION PROTOS. ************************* */

// Low-Level Functions
static bool oledSendCommand(const uint8_t cmd);
static bool oledSendData(const uint8_t *data, const int32_t dataSize);

// Drawing Helpers
static bool oledSetDrawingRegion(const uint8_t x1, const uint8_t y1,
                                 const uint8_t w, const uint8_t h);

static bool oledStringIsValid(const char *str, const GFXfont *font);
static const char *oledTokenizeString(const char *str, const char delim);

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
static bool oledSetDrawingRegion(const uint8_t x1, const uint8_t y1,
                                 const uint8_t w, const uint8_t h) {
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
  // Initialize the OLED's SPI interface.
  if (!spiInit(&OLEDSPIConfig)) return false;

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
 * @brief Turns the OLED screen off.
 *
 * @return True, if the OLED's screen was turned off successfully, OR if the
 * OLED wasn't initialized at all; false, otherwise.
 */
bool oledDeinit(void) {
  if (!oledIsInitialized) return true;

  const bool deinitStatus = (
    oledClearScreen() &&
    oledUpdateDisplay() &&
    oledSendCommand(SSD1351_CMD_DISP_OFF)
    /* TODO: Close SPI file descriptor. */
  );

  if (deinitStatus) oledIsInitialized = false;
  return deinitStatus;
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
 * @brief Draws a point (cross, '+'), centered at the (x, y) coordinate.
 * 
 * @param x The desired x-coordinate center of the point.
 * 
 * @param y The desired y-coordinate center of the point.
 * 
 * @param color The desired color of the point.
 * 
 * @return True, if the point was drawn successfully; false, otherwise.
 * 
 * @note Changes will not be visible until oledUpdateDisplay() is invoked.
 */
bool oledDrawPoint(uint8_t x, uint8_t y, const uint16_t color) {
  // Ensure the display is initialized before drawing to it.
  if (!oledIsInitialized) return false;

  // If the (x, y) coordinate is out-of-bounds, clamp it to the display's size.
  if (x >= SSD1351_DISP_WIDTH) x = (SSD1351_DISP_WIDTH - 1);
  if (y >= SSD1351_DISP_HEIGHT) y = (SSD1351_DISP_HEIGHT - 1);

  // Draw the point -- a simple cross ('+'), centered at (x, y).
  const bool left = (x > 0);
  const bool right = (x < (SSD1351_DISP_WIDTH - 1));
  const bool up = (y > 0);
  const bool down = (y < (SSD1351_DISP_HEIGHT - 1));

  oledDrawPixel(x, y, color);
  // '+'
  // if (left)   if (!oledDrawPixel(x - 1, y, color)) return false;
  // if (right)  if (!oledDrawPixel(x + 1, y, color)) return false;
  // if (up)     if (!oledDrawPixel(x, y - 1, color)) return false;
  // if (down)   if (!oledDrawPixel(x, y + 1, color)) return false;

  // 'X'
  if (left && up)    if (!oledDrawPixel(x - 1, y - 1, color)) return false;
  if (right && up)   if (!oledDrawPixel(x + 1, y - 1, color)) return false;
  if (left && down)  if (!oledDrawPixel(x - 1, y + 1, color)) return false;
  if (right && down) if (!oledDrawPixel(x + 1, y + 1, color)) return false;


  return true;
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
 * @param[out] metrics Used to retrieve the bbox's ascent/descent, which is
 * useful for proper vertical centering in oledDrawText(...).
 *
 * @return An (X, Y) coordinate, defining the bottom-right corner of the
 * bounding box; (UINT8_MAX, UINT8_MAX), otherwise.
 *
 * @note This function assumes that the top-left corner of the box is at (0, 0).
 */
Coordinate oledCalcTextBounds(
  const char *str,
  const uint8_t len,
  const GFXfont *font,
  TextMetrics_t *metrics
) {
  // Input Validation
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

  if (metrics) {
    metrics->ascent = -topmostPx;
    metrics->descent = bottommostPx;
  }

  Coordinate end = {rightmostPx, bottommostPx};
  if (leftmostPx < 0) end.x += (-leftmostPx);
  if (topmostPx < 0) end.y += (-topmostPx);
  return end;
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
  /* X-Coordinates */
  if (textParams.x < 0) textParams.x = 0;
  else if (textParams.x >= SSD1351_DISP_WIDTH) {
    textParams.x = (SSD1351_DISP_WIDTH - 1);
  }

  /* Y-Coordinates */
  if (textParams.y1 < 0) textParams.y1 = 0;
  else if (textParams.y1 >= SSD1351_DISP_HEIGHT) {
    textParams.y1 = (SSD1351_DISP_HEIGHT - 1);
  }

  if (textParams.y2 < 0) textParams.y2 = 0;
  else if (textParams.y2 >= SSD1351_DISP_HEIGHT) {
    textParams.y2 = (SSD1351_DISP_HEIGHT - 1);
  }

  if (textParams.y1 > textParams.y2) SWAP_VALUE(textParams.y1, textParams.y2);

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
  int16_t textWidth = 0, textHeight = 0;

  const char *text = textParams.text;

  // Ensure all characters are supported before drawing.
  if (!oledStringIsValid(text, font)) return false;

  // Determine exact width / height of text to draw.
  TextMetrics_t metrics;
  Coordinate boundingBox =
    oledCalcTextBounds(text, strlen(text), font, &metrics);

  textWidth = (boundingBox.x + 1);
  textHeight = (boundingBox.y + 1);

  // Determine whether text can fit w/in bounding box:
  // This really only applies to the vertical constraints, as we clip text if
  // it cannot fit horizontally.
  /* Horizontal Bounds Check */
  ;

  /* Vertical Bounds Check */
  const int16_t boxHeight = textParams.y2 - textParams.y1;
  if (textHeight > boxHeight) return false;

  // Draw the text to the GDDRAM buffer, tokenizing against newline chars.
  uint8_t tokenIdx = 0;
  bool finishedParsing = false;
  cursor.y = (textParams.y1 + ((boxHeight - textHeight) / 2) + metrics.ascent);
  while (!finishedParsing) {
    // Extract the next token, and ...
    const char *tokenStart = text + tokenIdx;
    const char *tokenEnd = oledTokenizeString(tokenStart, '\n');
    uint8_t tokenLen = tokenEnd - tokenStart;

    if ((*tokenEnd) == '\0') finishedParsing = true;

    // ... Draw token to GDDRAM buffer (if not empty).
    if (!tokenLen) { // Implies newline found, tokenLen == 0; skip.
      tokenIdx += 1;
      cursor.y += font->yAdvance;
      continue; // To next iteration of while-loop.
    }

    /* Calculate token's dimensions; useful for text-clipping + centering. */
    boundingBox = oledCalcTextBounds(tokenStart, tokenLen, font, NULL);
    if (oledCoordinateIsInvalid(boundingBox)) return false;

    int16_t tokenWidth = (boundingBox.x + 1);
    int16_t tokenHeight = (boundingBox.y + 1);

    /* Determine cursor's starting x-coord. */
    if (!textParams.center) {
      cursor.x = textParams.x;
    } else {
      const int16_t effectiveWidth = MIN_VALUE(tokenWidth, SSD1351_DISP_WIDTH);
      cursor.x = ((SSD1351_DISP_WIDTH - effectiveWidth) / 2);
    }

    /* Determine whether clipping is necessary. */
    int16_t hyphenWidth = 0;
    const int16_t rightEdge = SSD1351_DISP_WIDTH;
    const bool needsHyphen = ((cursor.x + tokenWidth) > rightEdge);
    if (needsHyphen) {
      if ('-' < font->first || '-' > font->last) return false;
      const char *hyphen = "-";
      boundingBox = oledCalcTextBounds(hyphen, 1, font, NULL);
      if (oledCoordinateIsInvalid(boundingBox)) return false;

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
        const uint8_t xo = glyph->xOffset;
        const uint8_t yo = glyph->yOffset;

        uint8_t bit = 0, bits = 0;
        for (uint8_t yy = 0; yy < h; yy++) {
          for (uint8_t xx = 0; xx < w; xx++) {
            if (!(bit++ & 7)) bits = bitmap[bo++];

            if (
                (bits & 0x80) &&
                !oledDrawPixel(
                  cursor.x + xo + xx,
                  cursor.y + yo + yy,
                  textParams.color
                )
              ) { return false; }

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
        const uint8_t xo = glyph->xOffset;
        const uint8_t yo = glyph->yOffset;

        uint8_t bit = 0, bits = 0;
        for (uint8_t yy = 0; yy < h; yy++) {
          for (uint8_t xx = 0; xx < w; xx++) {
            if (!(bit++ & 7)) bits = bitmap[bo++];

            if (
                (bits & 0x80) &&
                !oledDrawPixel(
                  cursor.x + xo + xx,
                  cursor.y + yo + yy,
                  textParams.color
                )
          ) { return false; }

            bits <<= 1;
          }
        }
      }

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
 * @param imageParams A struct containing information about how the image should
 * be drawn to the OLED:
 *    -  image - A pointer to the heap memory containing the RGB565 image data;
 *    -  width - The width of `image`, in bytes;
 *    - height - The height of `image`, in bytes;
 *    -      x - The starting x-coordinate of the image to draw; and,
 *    -      y - The starting y-coordinate of the image to draw.
 *
 * @return True, if the image was drawn successfully; false, otherwise.
 *
 * @note If the image's size exceeds the dimensions of the OLED, it will still
 * be drawn, but will be clipped.
 */
bool oledDrawImage(ImageParameters imageParams) {
  // Input Validation
  if (!imageParams.image) {
    return false;
  } else if (imageParams.x >= SSD1351_DISP_WIDTH ||
             imageParams.y >= SSD1351_DISP_HEIGHT) {
    return false;
  }

  // Draw the image to the buffer.
  const uint32_t x2 =
      MIN_VALUE(imageParams.x + imageParams.width, SSD1351_DISP_WIDTH);
  const uint32_t y2 =
      MIN_VALUE(imageParams.y + imageParams.height, SSD1351_DISP_HEIGHT);

  for (uint32_t dispY = imageParams.y; dispY < y2; dispY++) {
    const uint32_t srcRow = dispY - imageParams.y;
    for (uint32_t dispX = imageParams.x; dispX < x2; dispX++) {
      const uint32_t srcCol = dispX - imageParams.x;
      const uint32_t srcByteIdx = (srcRow * imageParams.width + srcCol) * 2;
      const uint16_t pixel = (((uint16_t)imageParams.image[srcByteIdx] << 8) |
                              (imageParams.image[srcByteIdx + 1]));

      uint16_t *dest = &dispBuffer[SSD1351_ACCESS_PIXEL(dispX, dispY)];
      ((uint8_t *)dest)[0] = (pixel >> 8) & 0xFF;
      ((uint8_t *)dest)[1] = pixel & 0xFF;
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
  if (!oledSetDrawingRegion(0, 0, SSD1351_DISP_WIDTH, SSD1351_DISP_HEIGHT) ||
      !oledSendCommand(SSD1351_CMD_WRITE_RAM) ||
      !oledSendData((uint8_t *)dispBuffer, SSD1351_MAX_RW_SIZE_B)) {
    return false;
  }

  return true;
}

/**
 * @brief Determines whether a Coordinate is invalid. This function is generally
 * useful after a call to oledCalcTextBounds(...).
 * 
 * @param coord The Coordinate whose validity is in question.
 * 
 * @return True, if `coord` is invalid; false, otherwise.
 */
bool oledCoordinateIsInvalid(const Coordinate coord) {
  return (coord.x == INVALID_COORDINATE.x || coord.y == INVALID_COORDINATE.y);
}
