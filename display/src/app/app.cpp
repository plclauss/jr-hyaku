#include "app/app.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

#include "utils/logger/logger.h"

/**
 * @brief Loops indefinitely parsing / responding to JSON commands from API.
 */
void App::run() {
  /* Display IDLE screen. */
  if (!this->revertDisplayToIDLE()) {
    LOG_WRN("%s: Failed to display IDLE screen on boot; continuing", __func__);
  }

  /* Thread jobs. */
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

      /* Revert to IDLE screen. */
      if (!this->revertDisplayToIDLE()) {
        LOG_WRN("%s: Failed to revert to IDLE screen; continuing", __func__);
        // Result of GET still displayed; better than clearing.
      }
    } else {
      LOG_WRN("%s: Failed to handle JSON command", __func__);
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
    /* Cmd Type */
    std::string cmdType = json.at("cmd").get<std::string>();
    if (cmdType != "show_line") {
      LOG_WRN("%s: Unknown JSON command received: %s", __func__, cmdType.c_str());
      return false;
    }

    /* Line Name + Completion Percentage */
    std::string lineName = json.at("line_name").get<std::string>();
    double completionPct = json.at("completion_pct").get<double>();

    /* Stations list w/ station_cd, px, and py values. */
    const nlohmann::json& stations = json.at("stations");
    if (!stations.is_array()) {
      LOG_WRN("%s: 'stations' has invalid format", __func__);
      return false;
    }

    if (stations.empty()) {
      LOG_WRN(
        "%s: line=%s has no stations; nothing to draw",
        __func__,
        lineName.c_str()
      );
      return false;
    }

    for (const auto& station : stations) {
      station.at("station_cd").get<int32_t>();
      station.at("px").get<int32_t>();
      station.at("py").get<int32_t>();
    }

    LOG_INF("%s: Valid JSON received: %s", __func__, json.dump().c_str());
    return true;
  } catch (const nlohmann::json::out_of_range& e) {
    LOG_WRN("%s: Missing info in cmd: %s", __func__, e.what());
  } catch (const nlohmann::json::type_error& e) {
    LOG_WRN("%s: Wrong field type: %s", __func__, e.what());
  } catch (const std::exception& e) {
    LOG_WRN("%s: JSON invalid (unknown): %s", __func__, e.what());
  }

  return false;
}

/**
 * @brief Gets the RPi's home directory.
 */
std::string App::getHomeDir() {
  const char *home = std::getenv("HOME");
  return (home) ? std::string(home) : "";
}

/**
 * @brief Reads an image's .bin file into memory for displaying.
 * @param file The name of the file to display; all files follow the same
 * standard, using the name as a unique identifier.
 * @return True, if the file was read into memory; false, otherwise.
 */
bool App::jsonReadImageData(const std::string& file) {
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

  const std::string home = getHomeDir();
  const std::string filepath = home + "/Desktop/jr-hyaku/assets/images/" + file + ".bin";
  std::ifstream bin(filepath, std::ios::binary);
  if (!bin.is_open()) {
#ifdef DEBUG
    LOG_DBG("%s: Failed to open file=%s (path=%s)", __func__, file.c_str(), filepath.c_str());
#endif
    return false;
  }

  std::error_code ec;
  const auto fileSz = std::filesystem::file_size(filepath, ec);
  if (ec || imgSz != fileSz) {
#ifdef DEBUG
    LOG_DBG("%s: Cannot read image; invalid file size=%zu", __func__, fileSz);
#endif
    return false;
  }

  if (!bin.read(reinterpret_cast<char*>(buffer.get()), imgSz)) {
#ifdef DEBUG
    LOG_DBG("%s: Failed to read image data from file", __func__);
#endif
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
  const nlohmann::json& stations = json.at("stations");

  /* Display relevant information. */
  {
    // Read image data from assets/.
    if (!this->jsonReadImageData(line)) return false;

    // Clear previous GDDRAM.
    if (!oledClearScreen()) {
#ifdef DEBUG
      LOG_DBG("%s: Failed to clear previous GDDRAM buffer", __func__);
#endif
      return false;
    }

    // Draw image to GDDRAM.
    if (!oledDrawImage(this->imageParams_)) {
#ifdef DEBUG
      LOG_DBG("%s: Failed to draw PNG to GDDRAM", __func__);
#endif
      return false;
    }

    // Draw caption to GDDRAM.
    std::stringstream pctAsString;
    pctAsString << std::fixed << std::setprecision(2) << completionPct;
    const std::string caption = pctAsString.str() + "%";
    const char *captionCStr = caption.c_str();

    static const auto font_ = &Inconsolata_SemiCondensed_Bold4pt7b;
    Coordinate captionBbox =
      oledCalcTextBounds(captionCStr, strlen(captionCStr), font_, NULL);
    if (oledCoordinateIsInvalid(captionBbox)) {
#ifdef DEBUG
      LOG_DBG(
        "%s: Caption bbox invalid ({%zu, %zu})",
        __func__,
        captionBbox.x,
        captionBbox.y
      );
#endif
      return false;
    }

    const int16_t yStart = ((SSD1351_DISP_HEIGHT - 1) - (captionBbox.y + 1));
    const TextParameters tp = {
      .font = SEMI_CONDENSED_4PTBOLD,
      .color = WHITE,
      .text = captionCStr,
      .x = 0,
      .y1 = yStart,
      .y2 = SSD1351_DISP_HEIGHT,
      .center = true,
    };

    if (!oledDrawString(tp)) {
#ifdef DEBUG
      LOG_DBG("%s: Failed to draw caption to GDDRAM", __func__);
#endif
      return false;
    }

    // Station markers
    for (const auto& station : stations) {
      const int32_t px = station.at("px").get<int32_t>();
      const int32_t py = station.at("py").get<int32_t>();

      if (!oledDrawPoint(px, py, WHITE)) {
#ifdef DEBUG
        LOG_DBG(
          "%s: Failed to draw station marker @ (%d, %d)",
          __func__, px, py
        );
#endif
        return false;
      }
    }

    // Update OLED w/ content of GDDRAM.
    if (!oledUpdateDisplay()) {
#ifdef DEBUG
      LOG_DBG("%s: Failed to update display w/ line data", __func__);
#endif
      return false;
    }

    // Update contrast to maximum.
    if (!oledSetMasterContrast(0xFF)) {
#ifdef DEBUG
      LOG_DBG("%s: Failed to update contrast to maximum; ignoring", __func__);
#endif
    }
  }

  return true;
}

/**
 * @brief Reverts the OLED to an IDLE screen (splash art), following a GET.
 * @details Currently, there are only two IDLE files, and the file displayed
 * is chosen via an RNG.
 * @return True, if the IDLE art was drawn to the OLED; false, otherwise.
 */
bool App::revertDisplayToIDLE() {
  /* Helper vars. */
  const std::string filename = "IDLE_" + std::to_string(rand() % 2);
  static const std::string caption = "jr-hyaku\nby Paul Clauss";
  static const char *captionCStr = caption.c_str();

  /* Display relevant information. */
  {
    // Read image data from assets/.
    if (!this->jsonReadImageData(filename)) return false;

    // Clear previous GDDRAM.
    if (!oledClearScreen()) {
#ifdef DEBUG
      LOG_DBG("%s: Failed to clear previous GDDRAM buffer", __func__);
#endif
      return false;
    }

    // Draw image to GDDRAM.
    if (!oledDrawImage(this->imageParams_)) {
#ifdef DEBUG
      LOG_DBG("%s: Failed to draw PNG to GDDRAM", __func__);
#endif
      return false;
    }

    static const auto font_ = &Inconsolata_SemiCondensed_Bold4pt7b;
    Coordinate captionBbox =
      oledCalcTextBounds(captionCStr, strlen(captionCStr), font_, NULL);
    if (oledCoordinateIsInvalid(captionBbox)) {
#ifdef DEBUG
      LOG_DBG(
        "%s: Caption bbox invalid ({%zu, %zu})",
        __func__,
        captionBbox.x,
        captionBbox.y
      );
#endif
      return false;
    }

    const TextParameters tp = {
      .font = SEMI_CONDENSED_4PTBOLD,
      .color = WHITE,
      .text = captionCStr,
      .x = 0,
      .y1 = 0,
      .y2 = ++captionBbox.y,
      .center = true,
    };

    if (!oledDrawString(tp)) {
#ifdef DEBUG
      LOG_DBG("%s: Failed to draw caption to GDDRAM", __func__);
#endif
      return false;
    }

    // Update OLED w/ content of GDDRAM.
    if (!oledUpdateDisplay()) {
#ifdef DEBUG
      LOG_DBG("%s: Failed to update display w/ IDLE", __func__);
#endif
      return false;
    }

    // Update contrast to minimum.
    if (!oledSetMasterContrast(0x00)) {
#ifdef DEBUG
      LOG_DBG("%s: Failed to update contrast to minimum; ignoring", __func__);
#endif
    }
  }

  return true;
}