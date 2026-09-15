#include "app/app.hpp"

#include <fstream>
#include <stdexcept>

#include "bsp/oled/oled.h"
#include "utils/logger/logger.h"

/**
 * @brief Loops indefinitely parsing / responding to JSON commands from API.
 */
void App::run() {
  this->running_.store(true);
  while (this->running_.load()) {
    /* Check for valid frames. */
    std::optional<nlohmann::json> opt = this->server_->pop();
    if (!opt.has_value()) continue;

    /* Perform display action in command, if valid. */
    this->jsonHandleCommand(opt.value());
  }  // End while-loop.

  /* No resources to clean; just exit. */
}

/**
 * @brief Prepares the App thread for shutdown.
 */
void App::stop() { this->running_.store(false); }

/**
 * @brief Validates whether a JSON command received from the API is valid.
 * @param[in] json A ref. to the JSON received by the API.
 * @return True, if the JSON command is valid; false, otherwise.
 */
bool App::jsonCommandIsValid(const nlohmann::json& json) {
  try {
    std::string cmdType = json.at("cmd").get<std::string>();
    if (cmdType != "show_line") {
      LOG_WRN("%s: Unknown JSON command received: %s", __func__, cmdType);
      return false;
    }

    std::string lineName = json.at("line_name").get<std::string>();
    double completionPct = json.at("completion_pct").get<double>();
    LOG_INF("%s: Valid JSON received: %s", __func__, json.dump().c_str());

    return true;
  } catch (const nlohmann::json::out_of_range& e) {
  } catch (const nlohmann::json::type_error& e) {
  } catch (const std::exception& e) {
    LOG_WRN("%s: JSON invalid (unknown): %s", __func__, e.what());
  }

  return false;
}

/* TODO */
bool App::jsonHandleCommand(const nlohmann::json& json) {
  /* Input Validation */
  if (!this->jsonCommandIsValid(json)) {
#ifdef DEBUG
    LOG_DBG("%s: JSON command invalid; ignoring", __func__);
#endif
    return false;
  }

  /* Send display information to OLED. */
  const std::string line = json.at("line_name").get<std::string>();
  const double completionPct = json.at("completion_pct").get<double>();

  /* TODO: Display relevant information. */
  {
    /* Extract image data from file. */
    ImageParameters imgParams = {
        .image = NULL,
        .width = SSD1351_DISP_WIDTH,
        .height = SSD1351_DISP_HEIGHT,
        .x = 0,
        .y = 0,
    };

    const size_t imgSz =
        ((size_t)(imgParams.width * imgParams.height * sizeof(uint16_t)));
    imgParams.image = (uint8_t*)malloc(imgSz);
    if (!imgParams.image) {
      LOG_WRN("");   /* TODO */
      return false;  // OLED still displaying IDLE state.
    }

    const std::string filepath = "~/Desktop/jr-hyaku/assets/" + line + ".bin";
    std::ifstream bin(filepath, std::ios::binary);
    if (!bin.is_open()) {
      LOG_WRN(""); /* TODO */
      free(imgParams.image);
      return false;  // OLED still displaying IDLE state.
    }

    bin.seekg(0, std::ios::end);
    const std::streamsize fileSz = bin.tellg();
    bin.seekg(0, std::ios::beg);
    if (fileSz < 0 || static_cast<size_t>(fileSz) != imgSz) {
      LOG_WRN(""); /* TODO */
      free(imgParams.image);
      return false;  // OLED still displaying IDLE state.
    }

    if (!bin.read(reinterpret_cast<char*>(imgParams.image), imgSz)) {
      LOG_WRN(""); /* TODO */
      free(imgParams.image);
      return false;  // OLED still displaying IDLE state.
    }

    /* Send image data + completion percentage to OLED. */
  }

  /* TODO: Keep display live for pre-configured amount of time, then revert. */
}
