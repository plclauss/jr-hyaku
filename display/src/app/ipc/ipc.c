#include "app/ipc/ipc.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "utils/logger/logger.h"

#define SOCKET_NAME ("/run/jr-hyaku/jr-hyaku.sock")
#define DFLT_SOCKET_BACKLOG (3)

/**
 * @brief Initializes a UNIX domain socket at SOCKET_NAME.
 * @return A valid socket file descriptor, or -1.
 */
int32_t ipcInitUNIXDomainSocket(void) {
  /* Helper Vars. */
  int32_t status;

  /* Create the socket. */
  const int32_t socketFd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (socketFd == -1) {
    LOG_ERR("Failed to open UNIX socket: %s", strerror(errno));
    return LINUX_INVAL_SOCKET_FD;
  }

  /* Make the socket non-blocking, so calls to accept() don't block. */
  const int32_t flags = fcntl(socketFd, F_GETFL, 0);
  if (flags == -1 || fcntl(socketFd, F_SETFL, flags | O_NONBLOCK) == -1) {
    LOG_ERR("Failed to make socket non-blocking: %s", strerror(errno));
    ipcDeinitUNIXDomainSocket(socketFd);
    return LINUX_INVAL_SOCKET_FD;
  }

  /* Bind socket to a name. */
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));

  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, SOCKET_NAME, sizeof(addr.sun_path) - 1);

  unlink(addr.sun_path);  // Remove stale from prior crash / unexpected exit.
  status = bind(socketFd, (const struct sockaddr *)&addr, sizeof(addr));
  if (status == -1) {
    LOG_ERR("Failed to bind socket to %s: %s", SOCKET_NAME, strerror(errno));
    ipcDeinitUNIXDomainSocket(socketFd);
    return LINUX_INVAL_SOCKET_FD;
  }

  /* Listen for incoming connections. */
  status = listen(socketFd, DFLT_SOCKET_BACKLOG);
  if (status == -1) {
    LOG_ERR("Failed to listen on socket: %s", strerror(errno));
    ipcDeinitUNIXDomainSocket(socketFd);
    return LINUX_INVAL_SOCKET_FD;
  }

  LOG_INF("Initialized IPC socket w/ fd=%d!", socketFd);
  return socketFd;
}

/**
 * @brief Calls close() on a file descriptor.
 * @param fd The file descriptor to close -- used normally on the file
 * descriptor returned via a call to ipcInitUNIXDomainSocket().
 */
void ipcDeinitUNIXDomainSocket(const int32_t fd) {
  if (close(fd) == -1) {
    LOG_WRN("Failed to close socket w/ fd=%d: %s", fd, strerror(errno));
  }

  return;
}

/**
 * @brief Reads sz bytes from fd into buf.
 * @param fd The file descriptor to read from - typically a client connection.
 * @param[out] buf The buffer to which the data from the client are written to.
 * @param sz Size of buf, in bytes.
 * @return True, if sz bytes were read into buf; false, otherwise.
 */
static bool ipcReadFull(const int32_t fd, uint8_t *buf, size_t sz) {
  /* Input Validation */
  if (!buf || !sz) {
#ifdef DEBUG
    LOG_DBG("%s: Invalid buffer or buffer size", __func__);
#endif

    return false;
  }

  size_t total = 0;
  while (total < sz) {
    const ssize_t bytesRead = read(fd, buf + total, sz - total);
    if (bytesRead == -1) {
#ifdef DEBUG
      LOG_DBG("%s: Error reading from fd=%d: %s", __func__, fd,
              strerror(errno));
#endif
      return false;
    } else if (bytesRead == 0) {
#ifdef DEBUG
      LOG_DBG("%s: Peer closed connection; continuing", __func__);
#endif
      return false;
    }

    total += (size_t)bytesRead;
  }

  return true;
}

/**
 * @brief Tries to read one full frame from a client.
 * @param fd The file descriptor of the client.
 * @return A ptr to dynamically-allocated memory containing the JSON, or NULL.
 * @warning It is the responsibility of the calling function to free() the JSON.
 */
char *ipcReadFrame(const int32_t fd) {
  /* Input Validation */
  if (fd < 0) {
#ifdef DEBUG
    LOG_DBG("%s: Invalid file descriptor: %d", __func__, fd);
#endif

    return NULL;
  }

  /* Frames are of the format: [ Length(4) | JSON(N) ]                       */
  /* Hence, we check for the presence of a 4-byte field before reading more. */
  uint8_t buf[4] = {0};
  if (!ipcReadFull(fd, buf, 4)) return NULL;

  static const uint32_t __MAX_JSON_SZ_B = 8192U;
  const uint32_t jsonSz = (((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
                           ((uint32_t)buf[2] << 8) | ((uint32_t)buf[3] << 0));
  if (!jsonSz || jsonSz > __MAX_JSON_SZ_B) {
#ifdef DEBUG
    LOG_DBG("%s: JSON LEN header too large; discarding", __func__);
#endif
    return NULL;
  }

  /* Length header read successfully; finish reading frame. */
  char *json = (char *)malloc(jsonSz + 1);
  if (!json) {
#ifdef DEBUG
    LOG_DBG("%s: Failed to allocate memory for JSON; continuing", __func__);
#endif
    return NULL;
  }

  /* Read remaining data. */
  if (!ipcReadFull(fd, (uint8_t *)json, jsonSz)) {
    free(json);
    return NULL;
  }

  json[jsonSz] = '\0';
  return json;
}
