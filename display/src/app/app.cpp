#include "app/app.hpp"

#include "utils/logger/logger.h"

/* TODO */
void App::run() {
  this->running_ = true;
  while (this->running_) {
  }
}

/**
 * @brief Prepares the App thread for shutdown.
 */
void App::stop() { this->running_ = false; }
