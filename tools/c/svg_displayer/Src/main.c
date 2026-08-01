/**
 * @file main.c
 * @author Paul Clauss (plclauss@gmail.com)
 * @brief Tool to display images to an SSD1351 OLED.
 *        Usage: ./SVGDisplayer <path-to-bin>
 * @warning For simplicity, this tool does no input checks.
 * @date 2026-07-26
 */

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "BSP/OLED/oled.h"

/* ************************* CUSTOM VARS / DEFINES  ************************* */

typedef enum {
  RETVAL_SUCCESS = 0,
  RETVAL_ERROR = 1,
} ReturnValues;

/* **************************** ENTRY/EXIT POINT **************************** */

int32_t main(void) {
  printf("Initializing OLED...\r\n");
  if (!oledInit()) {
    printf("Failed to initialize OLED.\r\n");
    return RETVAL_ERROR;
  }

  printf("Drawing SVG...\r\n");
  {
    ImageParameters imageParams = {
      .image = NULL,
      .width = SSD1351_DISP_WIDTH,                              /* Change me! */
      .height = SSD1351_DISP_HEIGHT,                            /* Change me! */
      .x = 0, .y = 0,                                           /* Change me! */
    };

    const size_t imageSz = ((size_t)(
      imageParams.width * imageParams.height * sizeof(uint16_t)
    ));
    imageParams.image = (uint8_t *)malloc(imageSz);
    if (!imageParams.image) {
      printf("Failed to read SVG into memory. SVG too large?\r\n");
      return RETVAL_ERROR;
    }

    FILE *f = fopen(".bin", "rb");                              /* Change me! */
    if (!f) {
      printf(
        "Failed to open file. "
        "Forgot to update configuration?\r\n"
      );
      free(imageParams.image);
      return RETVAL_ERROR;
    }

    const int32_t seekRes = fseek(f, 0, SEEK_END);
    const long expecSz = ftell(f);
    if (seekRes || expecSz == -1L || (size_t)expecSz != imageSz) {
      printf(
        "File size does not match expected width/height. "
        "Forgot to update configuration?\r\n"
      );
      fclose(f);
      free(imageParams.image);
      return RETVAL_ERROR;
    }

    rewind(f);
    const size_t bytesRead = fread(
      imageParams.image, sizeof(uint8_t), imageSz, f
    );
    if (bytesRead != imageSz) {
      printf(
        "Failed to read SVG into memory. "
        "SVG too large?\r\n"
      );
      fclose(f);
      free(imageParams.image);
      return RETVAL_ERROR;
    }

    fclose(f);
    if (
      !oledDrawImage(imageParams) ||
      !oledUpdateDisplay()
    ) {
      printf("Failed to draw SVG.\r\n");
      free(imageParams.image);
      return RETVAL_ERROR;
    }

    free(imageParams.image);
  }

  printf("Success!\r\n");
  return RETVAL_SUCCESS;
}
