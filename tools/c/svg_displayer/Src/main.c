#include <inttypes.h>
#include <stdio.h>

#include "BSP/OLED/oled.h"

typedef enum {
  RETVAL_SUCCESS = 0,
  RETVAL_ERROR = 1,
} ReturnValues;

int32_t main(void) {
  printf("Initializing OLED...\r\n");
  if (!oledInit()) {
    printf("Failed to initialize OLED.\r\n");
    return RETVAL_ERROR;
  }

  printf("Drawing SVG (as PNG)...\r\n");
  uint8_t *svg = NULL;          /* TODO */
  if (!oledDrawImage(svg, 0)) { /* TODO */
    printf("Failed to draw SVG.\r\n");
    return RETVAL_ERROR;
  }

  oledClearScreen();
  oledDeinit();
  return RETVAL_SUCCESS;
}
