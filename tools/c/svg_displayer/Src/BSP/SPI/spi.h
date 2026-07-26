/**
 * @file spi.h
 * @author Paul Clauss (plclauss@gmail.com)
 * @brief A generic SPI .h file for clients to include (, as opposed to
 * architecture-specific ones). For architecture-specific includes, use the
 * CMake. (All generic, architecture-indepdent information is not preceded w/
 * the architecture name).
 * @date 2025-11-23
 */

#ifndef __SPI_H__
#define __SPI_H__

#ifdef LINUX
#include "Linux/linux-spi.h"
#endif

#endif  // __SPI_H__
