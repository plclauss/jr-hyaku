#ifdef LOGGER_ENABLE

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE  // Exposes gmtime_r() under -std=c17.
#endif

#include "utils/logger/logger.h"

#include <stdio.h>
#include <time.h>

/* Human-readable tag printed for each LogLevel, indexed by its enum value. */
static const char* const LOG_LEVEL_TAGS[NUM_LOG_LEVELS] = {
    [LOG_DEBUG] = "DEBUG",
    [LOG_INFO] = "INFO",
    [LOG_WARN] = "WARN",
    [LOG_ERROR] = "ERROR",
};

/**
 * @brief Writes the current UTC timestamp, formatted as
 * "YYYY-MM-DD HH:MM:SS", into buf.
 *
 * @param buf Destination buffer.
 * @param bufSz Size, in bytes, of `buf`.
 */
static void loggerFormatTimestamp(char* buf, const size_t bufSz) {
  const time_t now = time(NULL);
  struct tm utcTime;
  gmtime_r(&now, &utcTime);  // RPi's RTC/NTP-synced clock is kept in UTC.
  strftime(buf, bufSz, "%Y-%m-%d %H:%M:%S", &utcTime);
}

void loggerLogV(const LogLevel level, const char* format, va_list args) {
  if (level < 0 || level >= NUM_LOG_LEVELS || format == NULL) return;

  char timestamp[32];
  loggerFormatTimestamp(timestamp, sizeof(timestamp));

  printf("[%s] [%s] ", timestamp, LOG_LEVEL_TAGS[level]);
  vprintf(format, args);
  printf("\r\n");
}

void loggerLog(const LogLevel level, const char* format, ...) {
  va_list args;
  va_start(args, format);
  loggerLogV(level, format, args);
  va_end(args);
}

#endif /* LOGGER_ENABLE */
