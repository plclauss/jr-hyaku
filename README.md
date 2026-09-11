# jr-hyaku

A visualizer / tracker tool for those attempting a 100% of the Japan Rail (JR) transit system.

## Table of Contents
- [Inspiration](#inspiration)
- [Installation](#installation)
    - [Overview](#overview)
    - [Asset Installation](#asset-installation)
    - [Database Setup](#database-setup)
    - [Service Setup & Installation](#service-setup--installation)
- [Resources](#resources)
    - [Vemaps](#vemaps)
    - [Geographic Data](#geographic-data)

### Inspiration

I found out not too long ago that some people actually try to "100%" the JR transit system. (That is, they try to ride on every single JR locomotive, presumably for the length of its entire line). This sounded awesome, and I'd love to attempt it! (It'd probably take me a while, but that's besides the point).

Of the posts I saw (on Reddit), people simply used map applications to display their progress. It accomplished the goal of tracking progress, but I thought it'd be cool to track progress on a display that I could hang up on my wall like a photo frame. And that's what this project aims to accomplish!

Currently, it's a WIP, but I'll continue to update this README with relevant information as it arises.

### Installation

#### Overview

This project is pretty minimal on the hardware. I'm only using the following:
- [Raspberry Pi Zero 2W](https://www.raspberrypi.com/products/raspberry-pi-zero-2-w/)
- [128x128 RGB565 SSD1351 OLED](https://www.adafruit.com/product/1431)

The remaining stuff are home-goods, like photo frames and command strips. (You could probably use 3D-printed resources, as well).

---

The directory structure is more involved (all paths are relative to `~/Desktop/jr-hyaku/`):
```
.
├── api/
│   ├── .venv/                  # Python virtual environment
│   ├── api.py                  # HTTP API app / endpoints
│   ├── db.py                   # Database access layer
│   ├── requirements.txt        # Python dependencies
│   ├── jr-hyaku-api.service    # systemd service unit
│   └── install.sh              # API install/setup script
├── assets/
│   └── *.png                   # Static images
├── src/
│   └── TBD                     # SPI, GPIO, OLED drivers, etc.
└── misc/
    └── install_dependencies.sh # System-level dependency installer
```

#### Asset Installation

This project was designed with a 128x128 OLED in mind.

Even though SVGs are scalable (and even though scaling down to 128x128 still looked quite nice on modern displays), converting to a PNG / .bin file for display via SPI C code made the SVG look blurry.

Hence, the `tools/python/data-processing.ipynb` was created to help ratify this. (This just requires more setup for the Pi 😉).

---

If you haven't already, run the `data-processing.ipynb` with your chosen SVG from `assets/Vemaps/`, or just use the default already in the repo.

Once all assetse have been obtained, do the following:

```bash
# On the Pi Zero 2W
mkdir -p ~/Desktop/jr-hyaku
mkdir -p ~/Desktop/jr-hyaku/assets
```

```bash
# On the remote computer, in jr-hyaku -- this may take a while...
sudo apt install -y sshpass
sshpass -p "<pi-pw>" scp tools/python/data/images/*.png <pi-username>@<pi-ip>:Desktop/jr-hyaku/assets
```

#### Database Setup

**INSTRUCTIONS TBD**

#### Service Setup & Installation

The `jr-hyaku-api.service` is used to ensure the HTTP API is always active, from the Pi's boot sequence.

Installation is easy; simply do the following:

```bash
# On the Pi Zero 2W
mkdir -p ~/Desktop/jr-hyaku
mkdir -p ~/Desktop/jr-hyaku/api
cd ~/Desktop/jr-hyaku/api
python3 -m venv .venv
```

```bash
# On the remote computer, in jr-hyaku
sudo apt install -y sshpass
sshpass -p "<pi-pw>" scp api/api.py api/db.py api/install.sh api/jr-hyaku-api.service api/requirements.txt <pi-username>@<pi-ip>:Desktop/jr-hyaku/api
```

```bash
# On the Pi Zero 2W
cd ~/Desktop/jr-hyaku/api
chmod +x ./install.sh
source .venv/bin/active
sudo ./install.sh /opt/api ~/Desktop/jr-hyaku/api/.venv/
```

### Resources

Enumerated below is a series of resources used to help realize this project.

#### Vemaps

[Vemaps](https://vemaps.com/) is an open-source provider of free vector maps.

I used [Vemaps](https://vemaps.com/) to source the SVGs of Japan, but they offer maps of over 1200 entities.

High quality stuff!

#### Geographic Data

This project is intended to work with data from [ekidata.jp](https://ekidata.jp/) data; however, the following are some seriously fantastic alternatives:
- [Association for Open Data of Public Transportation](https://www.odpt.org/)
