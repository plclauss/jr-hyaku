#include "app/app.hpp"

#include <chrono>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <thread>

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
    const nlohmann::json json = opt.value();
    if (!this->jsonCommandIsValid(json)) {
#ifdef DEBUG
    LOG_DBG("%s: JSON command invalid; ignoring", __func__);
#endif
      continue; // To next loop iteration.
    }

    if (this->jsonHandleCommand(json)) {
      /* Hold on this screen for a few seconds, then revert. */
      std::this_thread::sleep_for(std::chrono::seconds(10));

      /* TODO: Revert to IDLE screen. */
    }
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

/**
 * @brief Reads an image's .bin file into memory for displaying.
 * @param line The name of the line to display; all files follow the same
 * standard, using the name as a unique identifier.
 * @return True, if the file was read into memory; false, otherwise.
 */
bool App::jsonReadImageData(const std::string& line) {
  /* Helper vars. */
  static const int32_t __WIDTH = SSD1351_DISP_WIDTH;
  static const int32_t __HEIGHT = SSD1351_DISP_HEIGHT;

  /* Ensure ImageParamters' base constant values are ok. */
  this->imageParams_.width = __WIDTH;
  this->imageParams_.height = __HEIGHT;
  this->imageParams_.x = 0;
  this->imageParams_.y = 0;

  /* Extract image data from file. */
  const size_t imgSz = static_cast<size_t>(
    __WIDTH * __HEIGHT * sizeof(uint16_t)
  );
  auto buffer = std::make_unique<uint8_t[]>(imgSz);

  const std::string filepath = "~/Desktop/jr-hyaku/assets/" + line + ".bin";
  std::ifstream bin(filepath, std::ios::binary);
  if (!bin.is_open()) {
    LOG_WRN("%s: Failed to open line=%s (path=%s)", __func__, line, filepath);
    return false;
  }

  std::error_code ec;
  const auto fileSz = std::filesystem::file_size(filepath, ec);
  if (ec || imgSz != fileSz) {
    LOG_WRN("%s: Cannot read image; invalid file size=%zu", __func__, fileSz);
    return false;
  }

  if (!bin.read(reinterpret_cast<char*>(buffer.get()), imgSz)) {
    LOG_WRN("%s: Failed to read image data from file", __func__);
    return false;
  }

  delete[] this->imageParams_.image;
  this->imageParams_.image = buffer.release();
  return true;
}

/**
 * @brief Handles a JSON command from the HTTP API.
 * @details Currently, there's only one command from the API -- "show_line".
 * It's used to display completion percentage information about the given line.
 * Hence, this function does precisely that one and only thing.
 * @param[in] json The JSON command from the API.
 * @return True, if the data was drawn to the OLED; false, otherwise.
 */
bool App::jsonHandleCommand(const nlohmann::json& json) {
  /* Extract relevant display-able information. */
  const std::string line = json.at("line_name").get<std::string>();
  const double completionPct = json.at("completion_pct").get<double>();

  /* Display relevant information. */
  {
    // Read image data from assets/.
    if (!this->jsonReadImageData(line)) return false;

    // Clear previous GDDRAM.
    if (!oledClearScreen()) {
      LOG_WRN("%s: Failed to clear previous GDDRAM buffer", __func__);
      return false;
    }

    // Draw image to GDDRAM.
    if (!oledDrawImage(this->imageParams_)) {
      LOG_WRN("%s: Failed to draw PNG to GDDRAM", __func__);
      return false;
    }

    // Draw caption to GDDRAM.
    std::stringstream pctAsString;
    pctAsString << std::fixed << std::setprecision(2) << completionPct;
    const std::string caption = line + "\n" + pctAsString.str();
    const char *captionCStr = caption.c_str();

    static const auto font_ = &Inconsolata_SemiCondensed_Bold4pt7b;
    Coordinate captionBbox =
      oledCalcTextBounds(captionCStr, strlen(captionCStr), font_, NULL);
    if (oledCoordinateIsInvalid(captionBbox)) {
      LOG_WRN(
        "%s: Caption bbox invalid ({%zu, %zu})",
        __func__,
        captionBbox.x,
        captionBbox.y
      );
      return false;
    }

    const TextParameters tp = {
      .font = SEMI_CONDENSED_4PTBOLD,
      .color = WHITE,
      .text = captionCStr,
      .x = 0,
      .y1 = --captionBbox.y,
      .y2 = SSD1351_DISP_HEIGHT,
      .center = true,
    };
    if (!oledDrawString(tp)) {
      LOG_WRN("%s: Failed to draw caption to GDDRAM", __func__);
      return false;
    }

    // Update OLED w/ content of GDDRAM.
    if (!oledUpdateDisplay()) {
      LOG_WRN("%s: Failed to update display w/ line data", __func__);
      return false;
    }
  }

  return true;
}
