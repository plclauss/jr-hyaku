#include <atomic>
#include <csignal>
#include <cstdint>
#include <thread>

#include "app/app.hpp"
#include "app/ipc/ipc.h"
#include "app/json/json.hpp"
#include "utils/logger/logger.h"

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
#endif

  /* Initialize all resources. */
  // Custom SIGTERM handler -- Performs graceful shutdown.
  std::signal(SIGTERM, sigtermHandler);

  // UNIX domain socket for IPC b/w HTTP API.
  const int32_t socketFd = ipcInitUNIXDomainSocket();
  if (socketFd == LINUX_INVAL_SOCKET_FD) {
    LOG_ERR("Failed to open UNIX socket");
    return 1;
  }

  /* Start application thread, and wait for event to shutdown. */
  App app;
  std::thread appThread([&]() { app.run(); });

  while (running_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  /* De-initialize all resources. */
  app.stop();
  appThread.join();

  ipcDeinitUNIXDomainSocket(socketFd);

  LOG_INF("Shutdown complete; closing application.");
  return 0;
}
