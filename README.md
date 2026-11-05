# Qutaq Web App Manager

Run websites as if they were apps.

## Build

To build the program, be sure to install the libraries

**Arch Linux**

```bash
sudo pacman -S --needed base-devel cmake qt6-base qt6-tools qt6-svg qt6-wayland qt6-translations
```

**Debian / Ubuntu**

```bash
sudo apt install build-essential cmake libgl-dev \
    qt6-base-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools \
    qt6-svg-dev qt6-wayland qt6-translations-l10n
```

**Fedora**

```bash
sudo dnf install gcc-c++ cmake \
    qt6-qtbase-devel qt6-qttools-devel qt6-linguist \
    qt6-qtsvg-devel qt6-qtwayland qt6-qttranslations
```

Building:
```bash
git clone https://github.com/Qutaq-Linux/qutaq-webapps.git
cd qutaq-webapps

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```
