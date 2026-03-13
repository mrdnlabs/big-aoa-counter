#!/usr/bin/env bash
set -euo pipefail

IMAGE_TAG="${IMAGE_TAG:-big-aoa-counter:dev}"
ARCH="${ARCH:-aarch64}"

docker build --build-arg ARCH="${ARCH}" --tag "${IMAGE_TAG}" .
mkdir -p build
container_id="$(docker create "${IMAGE_TAG}")"
trap 'docker rm -f "${container_id}" >/dev/null 2>&1 || true' EXIT
docker cp "${container_id}:/opt/app" ./build
echo "Build artifacts copied to ./build/app"
