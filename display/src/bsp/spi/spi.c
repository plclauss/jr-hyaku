/**
 * @file spi.c
 * @author Paul Clauss (plclauss@gmail.com)
 * @brief Implementations of functions declared in `spi.h`.
 * @date 2025-11-23
 */

#include "bsp/spi/spi.h"

#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* ************************* DEFINES / CUSTOM VARS ************************** */

// Defines the maximum singular transfer size.
// If the data to transfer exceeds this amount, it will be chunked.
#define SPI_MAX_RW_SIZE_B (4096)

/* ************************** FUNCTION DEFINITIONS ************************** */

/**
 * @brief Initializes an SPI interface, using the configuration provided.
 *
 * @param[in, out] spiConf Defines the SDI device file to open, and what
 * configuration to use for it.
 *
 * This struct's file descriptor will be updated w/ the file descriptor of the
 * device file, if opened successfully.
 *
 * @return True, if the initialization/configuration succeeded; false,
 * otherwise.
 *
 * @note It is recommended that the calling function rely on the return value of
 * this function, rather than the value of the file descriptor.
 */
bool spiInit(SPIConfig *spiConf) {
  // Open the device file.
  spiConf->fd = open(spiConf->devfile, O_RDWR);
  if (spiConf->fd < 0) return false;

  // Configure it to the configuration in the static global handle.
  // Note: It is recommended to use the #defines in <linux/types.h> to define
  // the SPI mode, so we conver the provided value here.
  int32_t spiMode;
  switch (spiConf->mode) {
    case 1: {
      spiMode = SPI_MODE_1;
      break;
    }
    case 2: {
      spiMode = SPI_MODE_2;
      break;
    }
    case 3: {
      spiMode = SPI_MODE_3;
      break;
    }
    case 0:
    default: {
      spiMode = SPI_MODE_0;
      break;
    }
  }

  if (
    ioctl(spiConf->fd, SPI_IOC_WR_MODE, &spiMode) < 0 ||
    ioctl(spiConf->fd, SPI_IOC_WR_BITS_PER_WORD, &spiConf->bitsPerWord) < 0 ||
    ioctl(spiConf->fd, SPI_IOC_WR_MAX_SPEED_HZ, &spiConf->speedHz) < 0) {
    close(spiConf->fd);
    return false;
  }

  return true;
}

/**
 * @brief Transfers data via SPI.
 *
 * @param spiConf A struct, containing the required information to carry-out an
 * SPI transfer.
 *
 * @param[in] tx The data to transfer to the peripheral (via MOSI/PICO).
 *
 * This value should never be NULL; if the calling function need only receive
 * data, the master/controller must still provide dummy data to transmit.
 *
 * @param[in, out] rx The destination buffer for received data (via MISO/POCI).
 *
 * This value may be NULL, if the calling function merely wishes to send data.
 *
 * @param bufSize The sizes of the tx and rx buffers, in bytes.
 *
 * @param[out] bufLen A reference to a int32_t *, used to inform the calling
 * function of how much data was received.
 *
 * This value is only relevant if rx is non-NULL.
 *
 * @return True, if the transfer succeeded; false, otherwise.
 */
bool spiTransferData(
  const SPIConfig spiConf,
  const uint8_t *tx,
  uint8_t *rx,
  const int32_t bufSize,
  int32_t *bufLen) {
  // Input Validation
  if (
    spiConf.fd < 0 || !tx || bufSize <= 0 ||
    ((rx && !bufLen) || (!rx && bufLen))) {
    return false;
  }

  // Write data via SPI.
  // Note: We transfer data in chunks, since SPI can only handle so much data at
  // a time. Since our OLED/microSD data is quite large, this is necessary.
  int32_t offset = 0;
  while (offset < bufSize) {
    // Determine the amount of data to transfer this iteration.
    int32_t chunkSize;
    if ((bufSize - offset) >= SPI_MAX_RW_SIZE_B) {
      chunkSize = SPI_MAX_RW_SIZE_B;
    } else {
      chunkSize = (bufSize - offset);
    }

    // Transfer data via SPI.
    const struct spi_ioc_transfer transferContext = {
      .tx_buf = (unsigned long)(tx + offset),
      .rx_buf = (rx) ? (unsigned long)(rx + offset) : 0UL,
      .len = chunkSize,
      .bits_per_word = spiConf.bitsPerWord,
      .speed_hz = spiConf.speedHz,
    };
    const int32_t numBytesTransferred =
      ioctl(spiConf.fd, SPI_IOC_MESSAGE(1), &transferContext);
    if (numBytesTransferred != transferContext.len) return false;

    // Advance the offset by the number of bytes transmitted.
    offset += chunkSize;
  }

  // If an Rx buffer was provided, update the length before returning.
  if (rx) (*bufLen) = bufSize;
  return true;
}
