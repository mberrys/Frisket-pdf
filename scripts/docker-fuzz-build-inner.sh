#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-build}"
VCPKG_COMMIT="0878b5224d4a4968940ee296a2e7fae2d3b62983"

export DEBIAN_FRONTEND=noninteractive
export VCPKG_ROOT=/work/.docker-vcpkg
export VCPKG_INSTALLED_DIR=/work/.docker-vcpkg-installed
export VCPKG_OVERLAY_PORTS=/work/vcpkg/overlays/linux:/work/vcpkg/overlays/general
export VCPKG_DEFAULT_BINARY_CACHE=/work/.docker-vcpkg-cache
export PDF4QT_QT_ROOT=/opt/Qt/6.11.1/gcc_64
export CMAKE_PREFIX_PATH="${PDF4QT_QT_ROOT}:${CMAKE_PREFIX_PATH:-}"
export LD_LIBRARY_PATH="${PDF4QT_QT_ROOT}/lib:${LD_LIBRARY_PATH:-}"

apt-get update -qq
apt-get install -y --no-install-recommends \
    build-essential clang ninja-build cmake git curl zip unzip tar python3 python3-pip python3-venv \
    libc6-dev linux-libc-dev libcups2 libcups2-dev libfontconfig1-dev libtbb-dev ca-certificates \
    libglib2.0-0 libasan8 libubsan1 libclang-rt-18-dev libgl1-mesa-dev libegl1-mesa-dev libxkbcommon-dev

mkdir -p "${VCPKG_DEFAULT_BINARY_CACHE}"

if [[ ! -x "${VCPKG_ROOT}/vcpkg" ]]; then
    if [[ -d "${VCPKG_ROOT}" ]]; then
        rm -rf "${VCPKG_ROOT}"
    fi
    git clone https://github.com/microsoft/vcpkg.git "${VCPKG_ROOT}"
    cd "${VCPKG_ROOT}"
    git checkout "${VCPKG_COMMIT}"
    ./bootstrap-vcpkg.sh -disableMetrics
fi

if [[ ! -x "${PDF4QT_QT_ROOT}/bin/qmake" ]]; then
    python3 -m venv /tmp/aqt-venv
    /tmp/aqt-venv/bin/pip install -q aqtinstall
    /tmp/aqt-venv/bin/aqt install-qt linux desktop 6.11.1 linux_gcc_64 \
        -O /opt/Qt -m qtimageformats
    ln -sfn linux_gcc_64 /opt/Qt/6.11.1/gcc_64
fi

"${VCPKG_ROOT}/vcpkg" install \
    --x-manifest-root=/work \
    --x-install-root="${VCPKG_INSTALLED_DIR}" \
    --clean-buildtrees-after-build \
    --clean-packages-after-build

rm -rf /work/build-fuzz-docker

cmake -B /work/build-fuzz-docker -S /work -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_C_FLAGS="-pthread" \
    -DCMAKE_CXX_FLAGS="-pthread" \
    -DCMAKE_EXE_LINKER_FLAGS="-pthread" \
    -DTHREADS_PREFER_PTHREAD_FLAG=ON \
    -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
    -DCMAKE_VCPKG_BUILD_TYPE=Release \
    -DVCPKG_INSTALLED_DIR="${VCPKG_INSTALLED_DIR}" \
    -DPDF4QT_INSTALL_QT_DEPENDENCIES=0 \
    -DPDF4QT_BUILD_ONLY_CORE_LIBRARY=ON \
    -DPDF4QT_BUILD_TESTS=OFF \
    -DPDF4QT_ENABLE_SENTRY=OFF \
    -DPDF4QT_ENABLE_SANITIZERS=ON \
    -DPDF4QT_BUILD_FUZZERS=ON \
    -DPDF4QT_QT_ROOT="${PDF4QT_QT_ROOT}"

cmake --build /work/build-fuzz-docker \
    --target fuzz_pdf_parser fuzz_stream_filters fuzz_content_stream fuzz_images \
    -j"$(nproc)"

if [[ "${MODE}" == "verify" ]]; then
    REGRESSION=/work/Fuzz/corpus/regression
    echo "Running fuzz_images on regression corpus..."
    /work/build-fuzz-docker/usr/bin/fuzz_images \
        -max_total_time=30 -print_final_stats=1 \
        "${REGRESSION}"
    echo "Regression corpus OK"
fi
