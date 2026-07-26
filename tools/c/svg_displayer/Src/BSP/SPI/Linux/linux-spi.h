/**
 * @file linux-spi.h
 * @author Paul Clauss (plclauss@gmail.com)
 * @brief Function declarations related to SPI communication.
 * @date 2025-11-23
 */

#ifndef __LINUX_SPI_H__
#define __LINUX_SPI_H__

#include <inttypes.h>
#include <stdbool.h>

/**
 * @brief A container to store the SPI device files' metadata (incl. the file
 * descriptor for reading/writing/determining whether the SPI is initialized).
 */
typedef struct LinuxSPIConfig {
  // OS-related data members
  int32_t fd;
  char *devfile;

  // SPI configuration
  uint8_t mode;
  uint8_t bitsPerWord;
  uint32_t speedHz;
} LinuxSPIConfig;

/**
 * @brief Misc. defines to aid w/ specifying a Linux SPI config. struct/obj.
 */
#define LINUX_INVALID_FILE_DESCRIPTOR_VAL (-1)
#define LINUX_DFLT_SPI_MODE (0)       // CPOL = 0, CPHA = 0
#define LINUX_DFLT_BITS_PER_WORD (8)  // Byte-addressable

bool spiInit(LinuxSPIConfig *spiConf);

bool spiTransferData(
  const LinuxSPIConfig spiConf,
  const uint8_t *tx,
  uint8_t *rx,
  const int32_t bufSize,
  int32_t *bufLen);

#endif  // __LINUX_SPI_H__
