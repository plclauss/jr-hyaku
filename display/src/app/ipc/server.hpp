#ifndef __SERVER_HPP__
#define __SERVER_HPP__

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <queue>

#include "app/ipc/ipc.h"

class Server {
 public:
  // Initializers / De-initializers
  explicit Server(const int32_t socketFd) : socketFd_(socketFd){};
  ~Server() = default;

  Server(const Server& other) = delete;             // Copy Constructor
  Server& operator=(const Server& other) = delete;  // Copy Assignment
  Server(Server&& other) = delete;                  // Move Constructor
  Server& operator=(Server&& other) = delete;       // Move Assignment

  // Thread Entry- and Exit-Points
  void run();
  void stop();

 private:
  /* Thread-related data members. */
  std::atomic<bool> running_{false};

  /* Functional data members / functions. */
  int32_t socketFd_{LINUX_INVAL_SOCKET_FD};

  std::mutex mutex_;
  std::condition_variable cv_;
  std::queue<nlohmann::json> queue_;

  void push(nlohmann::json cmd) {
    {
      std::lock_guard<std::mutex> lock(this->mutex_);
      queue_.push(std::move(cmd));
    }
    this->cv_.notify_one();
  };

  std::optional<nlohmann::json> pop(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(this->mutex_);
    if (!this->cv_.wait_for(lock, timeout,
                            [this] { return !this->queue_.empty(); })) {
      return std::nullopt; /* Timed out; queue empty. */
    }

    auto cmd = std::move(this->queue_.front());
    this->queue_.pop();
    return cmd;
  }
};

#endif  // __SERVER_HPP__
