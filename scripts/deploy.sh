#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENV_FILE="${ROOT_DIR}/.env.devices"
BUILD_DIR="${ROOT_DIR}/build/app"

if [[ -f "${ENV_FILE}" ]]; then
  # shellcheck disable=SC1090
  source "${ENV_FILE}"
fi

device_ip="${AXIS_DEVICE_IP:?AXIS_DEVICE_IP is required}"
device_user="${AXIS_DEVICE_USERNAME:?AXIS_DEVICE_USERNAME is required}"
device_pass="${AXIS_DEVICE_PASSWORD:?AXIS_DEVICE_PASSWORD is required}"

eap_file="$(find "${BUILD_DIR}" -maxdepth 1 -name '*.eap' | head -n 1)"
if [[ -z "${eap_file}" ]]; then
  echo "No .eap found in ${BUILD_DIR}" >&2
  exit 1
fi

curl --fail --silent --show-error --anyauth \
  --user "${device_user}:${device_pass}" \
  --form "file=@${eap_file};type=application/octet-stream" \
  "http://${device_ip}/axis-cgi/applications/upload.cgi"

package_name="$(basename "${eap_file}" .eap)"
package_name="${package_name%_*_*_*}"

curl --fail --silent --show-error --anyauth \
  --user "${device_user}:${device_pass}" \
  "http://${device_ip}/axis-cgi/applications/control.cgi?action=start&package=big_aoa_counter"

echo "Deployed ${eap_file} to ${device_ip}"
