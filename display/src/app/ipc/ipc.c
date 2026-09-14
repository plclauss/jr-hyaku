#include "app/ipc/ipc.h"

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "utils/logger/logger.h"

#define SOCKET_NAME ("/run/jr-hyaku.sock")
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
#ifdef DEBUG
    LOG_DBG("Failed to open UNIX socket: %s", strerror(errno));
#endif

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
#ifdef DEBUG
    LOG_DBG("Failed to bind socket to %s: %s", SOCKET_NAME, strerror(errno));
#endif

    ipcDeinitUNIXDomainSocket(socketFd);
    return LINUX_INVAL_SOCKET_FD;
  }

  /* TODO: Call listen(socketFd, DFLT_SOCKET_BACKLOG). */

#ifdef DEBUG
  LOG_DBG("Initialized IPC socket w/ fd=%d!", socketFd);
#endif
  return socketFd;
}

/**
 * @brief Calls close() on a file descriptor.
 * @param fd The file descriptor to close -- used normally on the file
 * descriptor returned via a call to ipcInitUNIXDomainSocket().
 */
void ipcDeinitUNIXDomainSocket(const int32_t fd) {
  if (close(fd) == -1) {
#ifdef DEBUG
    LOG_DBG("Failed to close socket w/ fd=%d: %s", fd, strerror(errno));
#endif
  }

  return;
}
