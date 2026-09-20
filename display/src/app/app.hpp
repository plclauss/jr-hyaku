#ifndef __APP_HPP__
#define __APP_HPP__

#include <atomic>
#include <cstdint>

#include "app/ipc/server.hpp"
#include "bsp/oled/oled.h"

class App {
 public:
  // Initializers / De-initializers
  explicit App(Server& server) : server_(&server){};
  ~App() = default;

  App(const App& other) = delete;             // Copy Constructor
  App& operator=(const App& other) = delete;  // Copy Assignment
  App(App&& other) = delete;                  // Move Constructor
  App& operator=(App&& other) = delete;       // Move Assignment

  // Thread Entry- and Exit-Points
  void run();
  void stop();

 private:
  /* Thread-related data members. */
  std::atomic<bool> running_{false};

  /* Functional data members / functions. */
  Server* server_{nullptr};

  bool jsonCommandIsValid(const nlohmann::json& json);

  ImageParameters imageParams_{};
  std::string getHomeDir();
  bool jsonReadImageData(const std::string& line);
  bool jsonHandleCommand(const nlohmann::json& json);
  bool revertDisplayToIDLE();
};

#endif  // __APP_HPP__
