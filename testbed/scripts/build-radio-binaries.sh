#!/usr/bin/env bash
set -euo pipefail

WORKSPACE_ROOT="${WORKSPACE_ROOT:-/workspace}"
SRSRAN_PROJECT_SRC="${SRSRAN_PROJECT_SRC:-${WORKSPACE_ROOT}/testbed/sources/srsRAN_Project}"
SRSRAN_4G_SRC="${SRSRAN_4G_SRC:-${WORKSPACE_ROOT}/testbed/sources/srsRAN_4G}"
SRSRAN_PROJECT_BUILD_DIR="${SRSRAN_PROJECT_BUILD_DIR:-build-docker}"
SRSRAN_4G_BUILD_DIR="${SRSRAN_4G_BUILD_DIR:-build-docker}"

build_gnb() {
  cd "${SRSRAN_PROJECT_SRC}"
  cmake -B "${SRSRAN_PROJECT_BUILD_DIR}" -G Ninja -DCMAKE_BUILD_TYPE=Release
  ninja -C "${SRSRAN_PROJECT_BUILD_DIR}" gnb
}

build_srsue() {
  cd "${SRSRAN_4G_SRC}"
  cmake -S . -B "${SRSRAN_4G_BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_GUI=OFF \
    -DENABLE_SRSUE=ON \
    -DENABLE_SRSENB=OFF \
    -DENABLE_SRSEPC=OFF
  ninja -C "${SRSRAN_4G_BUILD_DIR}" srsue srsran_rf_uhd
}

build_gnb
build_srsue
