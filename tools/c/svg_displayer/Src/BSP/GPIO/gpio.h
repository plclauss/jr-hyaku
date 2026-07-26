/**
 * @file gpio.h
 * @author Paul Clauss (plclauss@gmail.com)
 * @brief A generic GPIO .h file for clients to include (, as opposed to
 * architecture-specific ones). For architecture-specific includes, use the
 * CMake. (All generic, architecture-indepdent information is not preceded w/
 * the architecture name).
 * @date 2025-11-23
 */

#ifndef __GPIO_H__
#define __GPIO_H__

#ifdef LINUX
#include "Linux/linux-gpio.h"
#endif

#endif  // __GPIO_H__
