# SVG Displayer

## Table of Contents
- [Description](#description)
- [Dependency List](#dependencies)

### Description

This tool exists merely to test what certain SVGs look like on an SSD1351, post-downscaling.

In the context of the `jr-hyaku` repo, I wasn't sure how good the SVGs would look on something as small as the `128x128 SSD1351 OLED` I own (and have already written firmware for), so this tool helps me determine whether the `SSD1351` is suitable, or if I should consider a larger display.

*Note: All code is expected to run on an RPi5, as this firmware was stolen from another project of mine -- `mp3-player`. That repo is still private (b/c it's incomplete), but the code worked well, so I took it.*

Ideally, I'd want to keep the entire project on something like an RPi Zero, so if I decide against the SSD1351, then I'd look into larger displays with one of the following connectors: DSI, HDMI, or traditional I2C/SPI (all theoretically compatible w/ the RPi Zero line).

### Dependencies
- [CMake 4.2 (or greater)](https://github.com/Kitware/CMake/releases/download/v4.2.0/cmake-4.2.0.tar.gz)
    - Requires `pkg-config` and `libssl-dev`/`openssl-dev`.
- [libgpiod-2.2](https://mirrors.edge.kernel.org/pub/software/libs/libgpiod/libgpiod-2.2.tar.xz)
