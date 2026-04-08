#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
TESTBED_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
COMPOSE_FILE="${COMPOSE_FILE:-${TESTBED_DIR}/docker-compose.5g-testbed.yml}"

docker compose -f "${COMPOSE_FILE}" run --rm radio_builder
"${SCRIPT_DIR}/start-open5gs.sh"
docker compose -f "${COMPOSE_FILE}" up -d srsran_gnb srsue_b210
