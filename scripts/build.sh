#!/usr/bin/env bash
set -euo pipefail

IMAGE_TAG="${IMAGE_TAG:-big-aoa-counter:dev}"
ARCH="${1:-${ARCH:-aarch64}}"
BUILD_OUTPUT_DIR="${BUILD_OUTPUT_DIR:-build/app}"
DIST_DIR="${DIST_DIR:-dist}"

docker build --build-arg ARCH="${ARCH}" --tag "${IMAGE_TAG}" .
rm -rf "${BUILD_OUTPUT_DIR}"
mkdir -p "$(dirname "${BUILD_OUTPUT_DIR}")" "${DIST_DIR}"
container_id="$(docker create "${IMAGE_TAG}")"
trap 'docker rm -f "${container_id}" >/dev/null 2>&1 || true' EXIT
docker cp "${container_id}:/opt/app" "${BUILD_OUTPUT_DIR}"
find "${BUILD_OUTPUT_DIR}" -maxdepth 1 -name '*.eap' -exec cp {} "${DIST_DIR}/" \;
echo "Build artifacts copied to ${BUILD_OUTPUT_DIR}"
echo "EAP packages copied to ${DIST_DIR}"
