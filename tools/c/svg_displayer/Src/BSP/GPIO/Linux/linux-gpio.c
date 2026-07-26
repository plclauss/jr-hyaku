/**
 * @file linux-gpio.c
 * @author Paul Clauss (plclauss@gmail.com)
 * @brief Implementations of functions declared in `linux-gpio.h`.
 * @date 2025-11-23
 */

#include "linux-gpio.h"

#include <gpiod.h>  // TODO: Document installation (libgpiod-2.2.tar.xz).

/* ************************* DEFINES / CUSTOM VARS ************************** */

/**
 * @brief Metadata about the GPIO interface.
 */
#ifdef LINUX
#define GPIO_DEFAULT_GPIOCHIP_NAME ("/dev/gpiochip0")
#define GPIO_TOTAL_NUM_GPIOS (40)
#endif

/**
 * @brief Variables regarding the initialization of the GPIO interface, and of
 * the current pin configurations.
 */
static struct gpiod_chip *gpioChip = NULL;

typedef struct PinConfiguration {
  PinDirections dir;
  struct gpiod_line_request *lineReq;
} PinConfiguration;
static PinConfiguration pinConfigs[GPIO_TOTAL_NUM_GPIOS] = {
  [0 ...(GPIO_TOTAL_NUM_GPIOS - 1)] = {.dir = NUM_PINCONFS, .lineReq = NULL}};

/* ************************ STATIC FUNCTION PROTOS. ************************* */

static bool gpioInit(void);

/* ********************** STATIC FUNCTION DEFINITIONS *********************** */

/**
 * @brief Lazily initializes the GPIO interface.
 *
 * @return True, if initialization succeeds; false, otherwise.
 */
static bool gpioInit(void) {
  return ((gpioChip = gpiod_chip_open(GPIO_DEFAULT_GPIOCHIP_NAME)) != NULL);
}

/* ************************** FUNCTION DEFINITIONS ************************** */

/**
 * @brief Initializes a pin as an input/output pin.
 *
 * @param pin The pin to initialize as an input/output.
 *
 * @param dir The direction (input/output) to assign the `pin`.
 *
 * @note All pins configured as inputs are configured, by default, w/ a pull-up
 * resistor.
 *
 * @return True, if `pin` was initialized as `dir` successfully; false,
 * otherwise.
 */
bool gpioInitPin(const uint8_t pin, const PinDirections dir) {
  // Input Validation
  if (pin >= GPIO_TOTAL_NUM_GPIOS || dir < 0 || dir >= NUM_PINCONFS) {
    return false;
  }

  // Ensure GPIO Chip is initialized before initializing a pin.
  if (!gpioChip && !gpioInit()) return false;

  // Ensure the requested pin isn't already configured.
  if (pinConfigs[pin].dir != dir && pinConfigs[pin].dir != NUM_PINCONFS) {
    return false;
  }

  // Initialize variables to hold the settings of the line (pin).
  struct gpiod_line_config *conf = gpiod_line_config_new();
  struct gpiod_line_settings *settings = gpiod_line_settings_new();
  if (!conf || !settings) {
    if (conf) gpiod_line_config_free(conf);
    return false;
  }

  // Set the direction of the pin to be dir.
  int32_t res, dirAsEnum;
  if (dir == PINCONF_INPUT) {
    dirAsEnum = ((int32_t)(GPIOD_LINE_DIRECTION_INPUT));
  } else {
    dirAsEnum = ((int32_t)(GPIOD_LINE_DIRECTION_OUTPUT));
  }

  if (gpiod_line_settings_set_direction(settings, dirAsEnum) < 0) {
    gpiod_line_config_free(conf);
    gpiod_line_settings_free(settings);
    return false;
  }

  // If the requested dir is an input, configure the pin w/ a pull-up resistor.
  if (
    dir == PINCONF_INPUT &&
    gpiod_line_settings_set_bias(settings, GPIOD_LINE_BIAS_PULL_UP) < 0) {
    gpiod_line_config_free(conf);
    gpiod_line_settings_free(settings);
    return false;
  }

  // Add the pin direction to the settings.
  const uint32_t offsets[] = {pin};
  if (gpiod_line_config_add_line_settings(conf, offsets, 1, settings) < 0) {
    gpiod_line_config_free(conf);
    gpiod_line_settings_free(settings);
    return false;
  }

  // Obtain access to the line w/ the requested settings.
  struct gpiod_request_config *req = gpiod_request_config_new();
  if (!req) {
    gpiod_line_config_free(conf);
    gpiod_line_settings_free(settings);
    return false;
  }

  const char *consumer = (dir == PINCONF_INPUT) ? "INPUT" : "OUTPUT";
  gpiod_request_config_set_consumer(req, consumer);
  pinConfigs[pin].lineReq = gpiod_chip_request_lines(gpioChip, req, conf);
  if (!pinConfigs[pin].lineReq) {
    gpiod_line_config_free(conf);
    gpiod_line_settings_free(settings);
    gpiod_request_config_free(req);
    return false;
  }

  // If successful, mark the pin as initialized by writing the direction to the
  // configuration array, clean all resources, and return true.
  pinConfigs[pin].dir = dir;
  gpiod_line_config_free(conf);
  gpiod_line_settings_free(settings);
  gpiod_request_config_free(req);
  return true;
}

/**
 * @brief Reads the logic level of a pin.
 *
 * @param pin The pin to read from.
 *
 * @return PIN_HIGH/PIN_LOW, depending on the logic value of the pin at the time
 * of reading; NUM_PIN_VALUES on error.
 */
PinValues gpioReadPin(const uint8_t pin) {
  // TODO: Add validation for pin input.

  // Ensure GPIO Chip is initialized before writing.
  if (!gpioChip && !gpioInit()) return NUM_PIN_VALUES;

  // Ensure pin to write to is configured as an output (PINCONF_OUTPUT).
  if (pinConfigs[pin].dir != PINCONF_INPUT) return NUM_PIN_VALUES;

  // Read the pin's current val.
  const uint32_t offset = (uint32_t)pin;
  struct gpiod_line_request *line = pinConfigs[pin].lineReq;
  const int32_t lineVal = gpiod_line_request_get_value(line, offset);
  if (lineVal < 0) return NUM_PIN_VALUES;
  return ((PinValues)(lineVal));
}

/**
 * @brief Writes the logic level of a pin to high/low.
 *
 * @param pin The pin to write to.
 *
 * @param val The value to write to the pin.
 *
 * @return True, if the pin was written to successfully; false, otherwise.
 */
bool gpioWritePin(const uint8_t pin, const PinValues val) {
  // TODO: Add validation for pin input.
  // Input Validation
  if (val < 0 || val >= NUM_PIN_VALUES) return false;

  // Ensure GPIO Chip is initialized before writing.
  if (!gpioChip && !gpioInit()) return false;

  // Ensure pin to write to is configured as an output (PINCONF_OUTPUT).
  if (pinConfigs[pin].dir != PINCONF_OUTPUT) return false;

  // Write val to the pin.
  const uint32_t offset = (uint32_t)pin;
  struct gpiod_line_request *line = pinConfigs[pin].lineReq;
  return (gpiod_line_request_set_value(line, offset, val) == 0);
}
