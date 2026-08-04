#!/usr/bin/env bash
# Build fuzz harnesses inside Docker (Linux/clang/vcpkg/Qt) for local verification.
#
# Usage (from repo root on a machine with Docker):
#   ./scripts/docker-fuzz-build.sh
#   ./scripts/docker-fuzz-build.sh verify

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE="${1:-build}"
IMAGE="${FRISKET_FUZZ_DOCKER_IMAGE:-ubuntu:24.04}"

if [[ -f /.dockerenv ]]; then
    exec "${REPO_ROOT}/scripts/docker-fuzz-build-inner.sh" "${MODE}"
fi

docker run --rm \
    -v "${REPO_ROOT}:/work" \
    -w /work \
    "${IMAGE}" \
    bash /work/scripts/docker-fuzz-build-inner.sh "${MODE}"
