#ifndef __APP_HPP__
#define __APP_HPP__

#include <atomic>
#include <cstdint>

#include "app/json/json.hpp"

class App {
 public:
  // Initializers / De-initializers
  explicit App(){};
  ~App() = default;

  App(const App& other) = delete;             // Copy Constructor
  App& operator=(const App& other) = delete;  // Copy Assignment
  App(App&& other) = delete;                  // Move Constructor
  App& operator=(App&& other) = delete;       // Move Assignment

  // Thread Entry- and Exit-Points
  void run();
  void stop();

 private:
  std::atomic<bool> running_{false};
};

#endif  // __APP_HPP__
