#include <atomic>
#include <csignal>
#include <cstdint>
#include <thread>

#include "app/app.hpp"
#include "app/ipc/server.hpp"
#include "bsp/oled/oled.h"
#include "utils/logger/logger.h"

typedef enum {
  STAT_CODE_SUCCESS = 0,
  STAT_CODE_INIT_ERR = 1,
  STAT_CODE_UNEXPECTED = 2,
} StatusCodes;

std::atomic<bool> running_{true};
extern "C" void sigtermHandler(int32_t) {
  LOG_INF("SIGTERM received; shutting down");
  running_.store(false);
}

int32_t main(void) {
  /* Initial log(s). */
  LOG_INF("Application started!");

#ifdef DEBUG
  LOG_DBG("DEBUG mode activated.");
#else
  LOG_DBG("DEBUG mode deactivated.");
#endif

  /* Initialize all resources. */
  // Custom SIGTERM handler -- Performs graceful shutdown.
  std::signal(SIGTERM, sigtermHandler);

  // OLED
  if (!oledInit()) {
    LOG_ERR("Failed to initialize OLED");
    return STAT_CODE_INIT_ERR;
  }

  // UNIX domain socket for IPC b/w HTTP API.
  const int32_t socketFd = ipcInitUNIXDomainSocket();
  if (socketFd == LINUX_INVAL_SOCKET_FD) return STAT_CODE_INIT_ERR;

  // High-level handles.
  Server server(socketFd);
  App app(server);

  /* Start application thread, and wait for event to shutdown. */
  std::thread serverThread([&]() { server.run(); });
  std::thread appThread([&]() { app.run(); });

  while (running_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  /* De-initialize all resources. */
  app.stop();
  appThread.join();

  server.stop();
  serverThread.join();

  ipcDeinitUNIXDomainSocket(socketFd);

  oledClearScreen();
  oledUpdateDisplay();
  oledDeinit();

  LOG_INF("Shutdown complete; closing application.");
  return STAT_CODE_SUCCESS;
}
