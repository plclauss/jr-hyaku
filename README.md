# jr-hyaku

A visualizer / tracker tool for those attempting a 100% of the Japan Rail (JR) transit system.

## Table of Contents
- [Inspiration](#inspiration)
- [Installation](#installation)
    - [Overview](#overview)
    - [Dependency Installation](#dependency-installation)
        - [libgpiod-2.2](#libgpiod)
        - [nlohmann/json](#nlohmannjson)
    - [Asset Installation](#asset-installation)
    - [Database Setup](#database-setup)
    - [API Setup & Installation](#api-setup--installation)
    - [Displayer Setup & Installation](#displayer-setup--installation)
- [Resources](#resources)
    - [Vemaps](#vemaps)
    - [The Noun Project](#the-noun-project)
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
│   ├── images/*.png            # Static images
│   └── db/*.csv                # Database data
├── src/
│   └── jrhyaku-displayer       # C-side logic (SPI, GPIO, OLED etc.)
└── misc/
    └── install_dependencies.sh # System-level dependency installer
```

#### Dependency Installation

The following must be installed for cross-compilation:
- [libgpiod-2.2](https://mirrors.edge.kernel.org/pub/software/libs/libgpiod/libgpiod-2.2.tar.xz)
- [nlohmann/json](https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)

See below for instructions on how to install these properly. First, though, you'll want to establish the sysroot:

```bash
# On the remote computer.
mkdir -p ~/aarch64-sysroot/
mkdir -p ~/aarch64-sysroot/usr
mkdir -p ~/aarch64-sysroot/usr/{include,lib}
```

*(Note: Normally, you'd want to do this with `debootstrap`, but this is much easier / faster).*

Also, don't forget to add this to your `c_cpp_properties.json` file in VS Code to avoid those pesky Intellisense errors:

```json
"includePath": [
    "${workspaceFolder}/**",
    "${env:HOME}/aarch64-sysroot/usr/include/**" <-- this line!
],
```

##### libgpiod

```bash
# On the remote computer.
wget https://mirrors.edge.kernel.org/pub/software/libs/libgpiod/libgpiod-2.2.tar.xz
tar -xf libgpiod-2.2.tar.xz
cd libgpiod-2.2
./configure --host=aarch64-linux-gnu --prefix=/usr --enable-tools=no
make -j$(nproc)
make install DESTDIR=$HOME/aarch64-sysroot
```

##### nlohmann/json

```bash
# On the remote computer.
mkdir -p ~/aarch64-sysroot/usr/include/nlohmann
wget -O ~/aarch64-sysroot/usr/include/nlohmann/json.hpp \
  https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp
```

#### Asset Installation

This project was designed with a 128x128 OLED in mind.

Even though SVGs are scalable (and even though scaling down to 128x128 still looked quite nice on modern displays), converting to a PNG / .bin file for display via SPI C code made the SVG look blurry.

Hence, the `tools/python/data-processing.ipynb` was created to help ratify this. (This just requires more setup for the Pi 😉).

---

If you haven't already, run the `data-processing.ipynb` with your chosen SVG from `assets/Vemaps/`, or just use the default already in the repo.

Once all assets have been obtained, do the following:

```bash
# On the Pi Zero 2W
mkdir -p ~/Desktop/jr-hyaku
mkdir -p ~/Desktop/jr-hyaku/assets
mkdir -p ~/Desktop/jr-hyaku/assets/{db,images}
```

```bash
# On the remote computer, in jr-hyaku -- this may take a while...
sudo apt install -y sshpass
sshpass -p "<pi-pw>" scp -v tools/python/data/db/*.csv tools/python/db-setup.py <pi-username>@<pi-ip>:Desktop/jr-hyaku/assets/db
sshpass -p "<pi-pw>" scp -v tools/python/data/images/bins/*.bin <pi-username>@<pi-ip>:Desktop/jr-hyaku/assets/images
```

#### Database Setup

The application uses a PostgreSQL database to store the tracking information. Adhere to the following.

*(Note: All instructions are to be done on the Pi Zero 2W).*

First, install the dependeny:

```bash
sudo apt install -y postgresql-15
```

Next, although not strictly necessary, I like to create a password for the default user:

```bash
sudo -i -u postgres
psql
ALTER USER postgres PASSWORD 'postgres';
\q
exit
```

Now, to make access easier, we edit the `/etc/postgresql/15/main/pg_hba.conf` file. `sudo <nano/vim>` into this file, and edit the following lines:

```
local all postgres peer -> local all postgres md5
local all all peer -> local all all md5
```

After, restart the service (, and we might as well get the locale settings up and running while we're at it 😉):

```bash
sudo sed -i 's/^# *en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen
sudo locale-gen

sudo systemctl restart postgresql
```

Now, let's create a new user specifically for this application:

```postgres
psql -U postgres
CREATE USER developer WITH PASSWORD 'yourpassword' CREATEDB;
CREATE DATABASE jrhyaku
  WITH OWNER developer
  ENCODING 'UTF8'
  LC_COLLATE 'en_US.UTF-8'
  LC_CTYPE 'en_US.UTF-8'
  TEMPLATE template0;
exit
```

To make it access under this user easier, we can add the credentials to the `~/.pgpass` file. `sudo <nano/vim>` into this file, and add the following line:

```
localhost:5432:jrhyaku:developer:yourpassword
```

And don't forget to add the permissions!

```bash
sudo chmod 600 ~/.pgpass
sudo chown $(whoami):$(whoami) ~/.pgpass
```

Finally, we can create all of the tables:

```postgres
psql -U developer -d jrhyaku
CREATE TABLE lines (
    pk INTEGER PRIMARY KEY GENERATED ALWAYS AS IDENTITY,
    line_cd INTEGER NOT NULL UNIQUE,
    line_name VARCHAR(80) NOT NULL,
    route_color VARCHAR(8) DEFAULT '#FFFFFF',
    line_type INTEGER DEFAULT 0,
    line_bbox TEXT
);
CREATE TABLE stations (
    pk INTEGER PRIMARY KEY GENERATED ALWAYS AS IDENTITY,
    station_cd INTEGER NOT NULL UNIQUE,
    station_name VARCHAR(80) NOT NULL,
    lon DOUBLE PRECISION NOT NULL,
    lat DOUBLE PRECISION NOT NULL,
    visited_at TIMESTAMPTZ,
    line_cd INTEGER NOT NULL,
    FOREIGN KEY (line_cd) REFERENCES lines(line_cd)
);
```

And, we can insert the data!

```bash
cd ~/Desktop/jr-hyaku/assets/db
python3 -m venv .venv
source .venv/bin/activate
pip install numpy pandas psycopg[binary]
python3 db-setup.py
deactivate
```

#### API Setup & Installation

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
sshpass -p "<pi-pw>" scp -v api/api.py api/db.py api/display.py api/install.sh api/jr-hyaku-api.service api/requirements.txt <pi-username>@<pi-ip>:Desktop/jr-hyaku/api
```

```bash
# On the Pi Zero 2W
cd ~/Desktop/jr-hyaku/api
chmod +x ./install.sh
source .venv/bin/activate
pip install -r requirements.txt
sudo ./install.sh /opt/api ~/Desktop/jr-hyaku/api/.venv/
deactivate
```

#### Displayer Setup & Installation

This one's also easy! If you haven't already (or if there isn't one present in the repo by default), generate the executable:

```bash
# On the remote computer, in jr-hyaku
cd display/build
./build -ld
```

Then, simply send it and configure::

```bash
# On the Pi Zero 2W
mkdir -p ~/Desktop/jr-hyaku
mkdir -p ~/Desktop/jr-hyaku/src
```

```bash
# On the remote computer, in jr-hyaku
sudo apt install -y sshpass
sshpass -p "<pi-pw>" scp -v display/build/bin/jrhyaku-displayer display/jr-hyaku-displayer.service <pi-username>@<pi-ip>:Desktop/jr-hyaku/src
```

```bash
# On the Pi Zero 2W
cd ~/Desktop/jr-hyaku/src
sudo cp jr-hyaku-displayer.service /etc/systemd/system
sudo systemctl daemon-reload
sudo systemctl enable jr-hyaku-displayer.service
sudo systemctl start jr-hyaku-displayer.service
```

### Resources

Enumerated below is a series of resources used to help realize this project.

#### Vemaps

[Vemaps](https://vemaps.com/) is an open-source provider of free vector maps.

I used [Vemaps](https://vemaps.com/) to source the SVGs of Japan, but they offer maps of over 1200 entities.

High quality stuff!

#### The Noun Project

[The Noun Project](https://thenounproject.com/) is a website for artists to publish free icons and stock photos.

I used [The Noun Project](https://thenounproject.com/) to source the SVGs of the IDLE screen; essentially just splash art while there are no active GET requests.

Thus far, I've downloaded SVGs from the following artists (visible under the `assets/The Noun Project/` directory!):
- [Simon Child](https://thenounproject.com/creator/Simon%20Child/) (or, [access their website](http://www.simonchild.work/) directly!)
- [Hey Rabbit](https://thenounproject.com/creator/heyrabbit/) (or, [access their website](http://www.behance.net/heyrabbit) directly!)

#### Geographic Data

This project is intended to work with data from [ekidata.jp](https://ekidata.jp/) data; however, the following are some seriously fantastic alternatives:
- [Association for Open Data of Public Transportation](https://www.odpt.org/)
