#!/usr/bin/env bash
# One-time (or idempotent) Frisket-PDF Linux dev environment bootstrap.
#
# Installs system packages, vcpkg, Qt 6.9.x, manifest dependencies, and
# configures the Ninja build directory at ./build.
#
# Usage:
#   ./scripts/setup-dev-env.sh
#   source scripts/dev-env.sh

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
QT_VERSION="${FRISKET_QT_VERSION:-6.9.1}"
QT_INSTALL_DIR="${FRISKET_QT_INSTALL_DIR:-/opt/Qt}"
VCPKG_ROOT="${VCPKG_ROOT:-/opt/vcpkg}"
VCPKG_INSTALLED_DIR="${VCPKG_INSTALLED_DIR:-/opt/vcpkg_installed}"
BUILD_DIR="${FRISKET_BUILD_DIR:-${REPO_ROOT}/build}"

# shellcheck disable=SC1091
source "${REPO_ROOT}/scripts/dev-env.sh"

log() { printf '>>> %s\n' "$*"; }
die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }

require_cmd() {
    command -v "$1" >/dev/null 2>&1 || die "Missing required command: $1"
}

log "Installing Linux build dependencies..."
sudo apt-get update -qq
sudo apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    git \
    pkg-config \
    python3 \
    python3-pip \
    python3-venv \
    libcups2 \
    libcups2-dev \
    libfontconfig1-dev \
    libgl1-mesa-dev \
    libxkbcommon-dev \
    libxcb1-dev \
    libxcb-cursor-dev \
    libxcb-icccm4-dev \
    libxcb-image0-dev \
    libxcb-keysyms1-dev \
    libxcb-randr0-dev \
    libxcb-render-util0-dev \
    libxcb-shape0-dev \
    libxcb-shm0-dev \
    libxcb-sync-dev \
    libxcb-xfixes0-dev \
    libxcb-xinerama0-dev \
    libxcb-xkb-dev \
    libx11-xcb-dev \
    libdbus-1-dev \
    libegl1-mesa-dev \
    libglib2.0-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libpulse-dev \
    libasound2-dev \
    libspeechd-dev \
    flex \
    bison \
    perl

log "Bootstrapping vcpkg at ${VCPKG_ROOT}..."
if [[ ! -x "${VCPKG_ROOT}/vcpkg" ]]; then
    sudo mkdir -p "$(dirname "${VCPKG_ROOT}")"
    if [[ ! -d "${VCPKG_ROOT}/.git" ]]; then
        sudo git clone --depth=1 https://github.com/microsoft/vcpkg.git "${VCPKG_ROOT}"
    fi
    sudo chown -R "$(id -un):$(id -gn)" "${VCPKG_ROOT}"
    (cd "${VCPKG_ROOT}" && ./bootstrap-vcpkg.sh -disableMetrics)
fi

log "Installing Qt ${QT_VERSION} to ${QT_INSTALL_DIR}..."
if [[ ! -x "${QT_INSTALL_DIR}/${QT_VERSION}/gcc_64/bin/qmake" && -x "${QT_INSTALL_DIR}/${QT_VERSION}/linux_gcc_64/bin/qmake" ]]; then
  ln -sfn linux_gcc_64 "${QT_INSTALL_DIR}/${QT_VERSION}/gcc_64"
fi
if [[ ! -x "${QT_INSTALL_DIR}/${QT_VERSION}/gcc_64/bin/qmake" ]]; then
  sudo mkdir -p "${QT_INSTALL_DIR}"
  sudo chown -R "$(id -un):$(id -gn)" "${QT_INSTALL_DIR}"
  AQT_VENV="${HOME}/.local/venvs/aqtinstall"
  if [[ ! -x "${AQT_VENV}/bin/aqt" ]]; then
    python3 -m venv "${AQT_VENV}"
    "${AQT_VENV}/bin/pip" install --upgrade pip aqtinstall
  fi
  export PATH="${AQT_VENV}/bin:${PATH}"
  require_cmd aqt
  aqt install-qt linux desktop "${QT_VERSION}" linux_gcc_64 \
      --outputdir "${QT_INSTALL_DIR}" \
      --modules qtspeech qtmultimedia
fi

# aqt uses linux_gcc_64 as the install folder name on recent Qt releases.
if [[ ! -d "${QT_INSTALL_DIR}/${QT_VERSION}/gcc_64" && -d "${QT_INSTALL_DIR}/${QT_VERSION}/linux_gcc_64" ]]; then
  ln -sfn linux_gcc_64 "${QT_INSTALL_DIR}/${QT_VERSION}/gcc_64"
fi

log "Installing vcpkg manifest dependencies (this may take several minutes)..."
sudo mkdir -p "${VCPKG_INSTALLED_DIR}"
sudo chown -R "$(id -un):$(id -gn)" "${VCPKG_INSTALLED_DIR}"
export VCPKG_OVERLAY_PORTS="${REPO_ROOT}/vcpkg/overlays/linux:${REPO_ROOT}/vcpkg/overlays/general"
"${VCPKG_ROOT}/vcpkg" install \
    --x-manifest-root="${REPO_ROOT}" \
    --x-install-root="${VCPKG_INSTALLED_DIR}" \
    --clean-buildtrees-after-build \
    --clean-packages-after-build

log "Configuring CMake build directory at ${BUILD_DIR}..."
cmake -B "${BUILD_DIR}" -S "${REPO_ROOT}" -G Ninja \
    -DPDF4QT_INSTALL_QT_DEPENDENCIES=0 \
    -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_INSTALLED_DIR="${VCPKG_INSTALLED_DIR}" \
    -DVCPKG_MANIFEST_INSTALL=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DPDF4QT_QT_ROOT="${PDF4QT_QT_ROOT}"

log "Writing ~/.bashrc Frisket dev block..."
BASHRC="${HOME}/.bashrc"
MARKER_START="# >>> frisket-pdf dev env >>>"
MARKER_END="# <<< frisket-pdf dev env <<<"
if ! grep -qF "${MARKER_START}" "${BASHRC}" 2>/dev/null; then
    cat >> "${BASHRC}" <<EOF

${MARKER_START}
if [[ -f "${REPO_ROOT}/scripts/dev-env.sh" ]]; then
    source "${REPO_ROOT}/scripts/dev-env.sh"
fi
${MARKER_END}
EOF
fi

log "Done."
cat <<EOF

Frisket-PDF dev environment is ready.

Next steps:
  1. Open a new shell, or run:  source scripts/dev-env.sh
  2. Build a target:            cmake --build build --target PdfTool -j\$(nproc)
  3. Run CLI:                   PdfTool help
  4. Run GUI apps (VNC):        unset QT_QPA_PLATFORM; DISPLAY=:1 ./build/usr/bin/Pdf4QtEditor

Key variables (see scripts/dev-env.sh):
  PDF4QT_QT_ROOT=${PDF4QT_QT_ROOT}
  VCPKG_ROOT=${VCPKG_ROOT}
  VCPKG_INSTALLED_DIR=${VCPKG_INSTALLED_DIR}
  FRISKET_BUILD_DIR=${BUILD_DIR}

EOF
