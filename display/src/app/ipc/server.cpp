#include "app/ipc/server.hpp"

#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdexcept>

#include "utils/logger/logger.h"

/**
 * @brief Loops indefinitely checking for client connections / frames from API.
 */
void Server::run() {
  this->running_.store(true);
  while (this->running_.load()) {
    /* Accept any pending connections. */
    struct pollfd pfd;
    pfd.fd = this->socketFd_;
    pfd.events = POLLIN;

    const int32_t readyFds = poll(&pfd, 1, 100);
    if (readyFds == -1) {
#ifdef DEBUG
      LOG_DBG("%s: Error polling for client connections: %s", __func__,
              strerror(errno));
#endif

      break;  // Kills Server thread; App thread will settle in IDLE state.
    } else if (readyFds == 0) {
      continue;  // Timed out; try again.
    }

    const int32_t clientFd = accept(this->socketFd_, nullptr, nullptr);
    if (clientFd == -1) {
#ifdef DEBUG
      LOG_DBG("%s: Failed to accept incoming client connection: %s", __func__,
              strerror(errno));
#endif

      break;  // Kills Server thread; App thread will settle in IDLE state.
    }

    /* Check for incoming frames from the client. */
    char *json = ipcReadFrame(clientFd);
    if (json) {
      try {
        this->queue_.push(nlohmann::json::parse(json));
      } catch (const nlohmann::json::parse_error &e) {
        LOG_WRN("%s: JSON parse error: %s", __func__, e.what());
      } catch (std::exception &e) {
        LOG_WRN("%s: Unknown JSON error: %s", __func__, e.what());
      }

      free(json);
    }

    /* Close connection with client; HTTP API sends one frame per connection. */
    if (close(clientFd) == -1) {
#ifdef DEBUG
      LOG_DBG("%s: Failed to close client connection: %s", __func__,
              strerror(errno));
#endif
    }
  }  // End while-loop.

  /* No resources to clean; just exit. */
}

/**
 * @brief Prepares the Server thread for shutdown.
 */
void Server::stop() { this->running_.store(false); }
