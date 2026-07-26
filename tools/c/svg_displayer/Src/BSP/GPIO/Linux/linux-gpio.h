/**
 * @file linux-gpio.h
 * @author Paul Clauss (plclauss@gmail.com)
 * @brief Function declarations related to GPIO usage.
 * @date 2025-11-23
 */

#ifndef __LINUX_GPIO_H__
#define __LINUX_GPIO_H__

#include <inttypes.h>
#include <stdbool.h>

typedef enum { PINCONF_INPUT, PINCONF_OUTPUT, NUM_PINCONFS } PinDirections;
bool gpioInitPin(const uint8_t pin, const PinDirections dir);

typedef enum { PIN_LOW = 0, PIN_HIGH = 1, NUM_PIN_VALUES = 2 } PinValues;
PinValues gpioReadPin(const uint8_t pin);
bool gpioWritePin(const uint8_t pin, const PinValues val);

#endif  // __LINUX_GPIO_H__
