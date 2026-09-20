#ifndef __LOGGER_H__
#define __LOGGER_H__

#ifdef LOGGER_ENABLE

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>

/**
 * @brief Severity tag prepended to a logged message.
 */
typedef enum {
  LOG_DEBUG,
  LOG_INFO,
  LOG_WARN,
  LOG_ERROR,
  NUM_LOG_LEVELS,
} LogLevel;

/**
 * @brief Logs a printf()-style, UTC-timestamped message to stdout, e.g.
 * `[2026-09-04 12:34:56 UTC] [ERROR] Failed to do XYZ`.
 *
 * @param level Severity of the message, used to populate its tag.
 * @param format printf()-style format string.
 * @param ... Arguments corresponding to `format`'s specifiers, if any.
 */
void loggerLog(const LogLevel level, const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;

/**
 * @brief va_list variant of loggerLog(), for wrapping this logger w/in
 * other variadic functions.
 *
 * @param level Severity of the message, used to populate its tag.
 * @param format printf()-style format string.
 * @param args Arguments corresponding to `format`'s specifiers, if any.
 * 
 * @warning Lengthy format strings may be truncated and prevent logging.
 */
void loggerLogV(const LogLevel level, const char* format, va_list args);

#ifdef __cplusplus
}
#endif

/* Convenience wrappers -- omit the LogLevel argument at call sites. */
#define LOG_DBG(...) loggerLog(LOG_DEBUG, __VA_ARGS__)
#define LOG_INF(...) loggerLog(LOG_INFO, __VA_ARGS__)
#define LOG_WRN(...) loggerLog(LOG_WARN, __VA_ARGS__)
#define LOG_ERR(...) loggerLog(LOG_ERROR, __VA_ARGS__)

#endif /* LOGGER_ENABLE */

#endif  // __LOGGER_H__
