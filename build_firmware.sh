#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARDUINO_CLI="${ARDUINO_CLI:-/home/mathmpr/CLionProjects/mini-core/bin/arduino-cli}"
ARDUINO_CONFIG="${ARDUINO_CONFIG:-/home/mathmpr/CLionProjects/mini-core/.arduino-cli.yaml}"
MINICORE_SRC="${MINICORE_SRC:-/home/mathmpr/CLionProjects/mini-core/src}"

BOARDS=("esp8266-as-asker" "esp32-as-sender")

echo "Selecione o firmware:"
select BOARD_DIR in "${BOARDS[@]}"; do
  if [[ -n "${BOARD_DIR}" ]]; then
    break
  fi
  echo "Opcao invalida."
done

SKETCH_DIR="${ROOT_DIR}/${BOARD_DIR}"
SKETCH_FILE="${SKETCH_DIR}/${BOARD_DIR}.ino"
CONFIG_FILE="${SKETCH_DIR}/config.h"
BUILD_DIR="$(mktemp -d)"
OUTPUT_FILE="${ROOT_DIR}/firmware.bin"

if [[ ! -f "${SKETCH_FILE}" ]]; then
  echo "Sketch nao encontrado: ${SKETCH_FILE}" >&2
  exit 1
fi

if [[ ! -f "${CONFIG_FILE}" ]]; then
  echo "Config nao encontrado: ${CONFIG_FILE}" >&2
  exit 1
fi

case "${BOARD_DIR}" in
  esp8266-as-asker)
    FQBN="esp8266:esp8266:generic:eesz=1M64"
    EXTRA_FLAGS=(--build-property "compiler.cpp.extra_flags=-I${MINICORE_SRC}")
    ;;
  esp32-as-sender)
    FQBN="esp32:esp32:esp32doit-devkit-v1:UploadSpeed=115200"
    EXTRA_FLAGS=(--build-property "compiler.cpp.extra_flags=-I${MINICORE_SRC}")
    ;;
  *)
    echo "Board nao suportada: ${BOARD_DIR}" >&2
    exit 1
    ;;
esac

echo "Compilando ${BOARD_DIR}..."
"${ARDUINO_CLI}" compile \
  --config-file "${ARDUINO_CONFIG}" \
  --fqbn "${FQBN}" \
  --output-dir "${BUILD_DIR}" \
  "${EXTRA_FLAGS[@]}" \
  "${SKETCH_DIR}"

cp "${BUILD_DIR}/${BOARD_DIR}.ino.bin" "${OUTPUT_FILE}"

echo "Firmware gerado em: ${OUTPUT_FILE}"
