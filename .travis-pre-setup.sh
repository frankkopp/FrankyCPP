#!/usr/bin/env bash

# Install a newer CMake version
curl -sSL https://cmake.org/files/v3.18/cmake-3.18.2-Linux-x86_64.sh -o install-cmake.sh
chmod +x install-cmake.sh
sudo ./install-cmake.sh --prefix=/usr/local --skip-license
ls -l /usr/local
ls -l /usr/local/bin