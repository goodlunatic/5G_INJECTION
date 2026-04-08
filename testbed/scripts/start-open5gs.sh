#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
TESTBED_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="$(cd -- "${TESTBED_DIR}/.." && pwd)"
COMPOSE_FILE="${COMPOSE_FILE:-${TESTBED_DIR}/docker-compose.5g-testbed.yml}"
ENV_TEMPLATE="${ENV_TEMPLATE:-${TESTBED_DIR}/configs/open5gs/open5gs.env.tpl}"
ENV_RENDERED="${ENV_RENDERED:-${TESTBED_DIR}/runtime/open5gs.env}"
SUBSCRIBER_ENV_FILE="${SUBSCRIBER_ENV_FILE:-${TESTBED_DIR}/subscribers/test-ue.env}"
LOG_DIR="${LOG_DIR:-${TESTBED_DIR}/logs/open5gs}"
SUPERVISOR_LOG="${LOG_DIR}/open5gs-supervisor.log"
CONTAINER_NAME="${CONTAINER_NAME:-testbed-open5gs}"
DOCKER_NETWORK_NAME="${DOCKER_NETWORK_NAME:-testbed_ran}"
OPEN5GS_IP="${OPEN5GS_IP:-10.53.1.2}"
UE_IP_BASE="${UE_IP_BASE:-10.45.0}"
NETWORK_NAME_FULL="${NETWORK_NAME_FULL:-5G Testbed}"
NETWORK_NAME_SHORT="${NETWORK_NAME_SHORT:-5GTestbed}"
OPEN5GS_DEBUG="${OPEN5GS_DEBUG:-false}"
TIMEZONE_VALUE="${TIMEZONE_VALUE:-${TZ:-UTC}}"

mkdir -p "${LOG_DIR}" "$(dirname -- "${ENV_RENDERED}")"
touch "${SUPERVISOR_LOG}"

log() {
  echo "[$(date '+%F %T')] $*" | tee -a "${SUPERVISOR_LOG}"
}

require_file() {
  local path="$1"
  if [[ ! -f "${path}" ]]; then
    log "required file not found: ${path}"
    exit 1
  fi
}

render_env() {
  require_file "${ENV_TEMPLATE}"
  require_file "${SUBSCRIBER_ENV_FILE}"

  # shellcheck disable=SC1090
  source "${SUBSCRIBER_ENV_FILE}"

  if [[ -z "${IMSI:-}" || -z "${K:-}" || -z "${OPC:-}" ]]; then
    log "subscriber env must define IMSI, K and OPC"
    exit 1
  fi

  local subscriber_ip="${SUBSCRIBER_IP:-${UE_IP_BASE}.2}"
  local amf_value="${AMF:-9001}"
  local qci_value="${QCI:-9}"
  local subscriber_db
  subscriber_db="${IMSI},${K},opc,${OPC},${amf_value},${qci_value},${subscriber_ip}"

  sed \
    -e "s|__OPEN5GS_IP__|${OPEN5GS_IP}|g" \
    -e "s|__UE_IP_BASE__|${UE_IP_BASE}|g" \
    -e "s|__OPEN5GS_DEBUG__|${OPEN5GS_DEBUG}|g" \
    -e "s|__SUBSCRIBER_DB__|${subscriber_db}|g" \
    -e "s|__NETWORK_NAME_FULL__|${NETWORK_NAME_FULL}|g" \
    -e "s|__NETWORK_NAME_SHORT__|${NETWORK_NAME_SHORT}|g" \
    -e "s|__TIMEZONE__|${TIMEZONE_VALUE}|g" \
    "${ENV_TEMPLATE}" > "${ENV_RENDERED}"
}

wait_healthy() {
  local status
  for _ in $(seq 1 60); do
    status="$(docker inspect -f '{{if .State.Health}}{{.State.Health.Status}}{{else}}{{.State.Status}}{{end}}' "${CONTAINER_NAME}" 2>/dev/null || true)"
    if [[ "${status}" == "healthy" || "${status}" == "running" ]]; then
      return 0
    fi
    sleep 1
  done
  log "container did not become healthy in time: ${CONTAINER_NAME}"
  docker logs "${CONTAINER_NAME}" --tail 100 | tee -a "${SUPERVISOR_LOG}" || true
  exit 1
}

ensure_route() {
  local ue_subnet="${UE_IP_BASE}.0/24"
  log "installing host route ${ue_subnet} via ${OPEN5GS_IP}"
  sudo ip route replace "${ue_subnet}" via "${OPEN5GS_IP}"
}

main() {
  render_env

  log "starting 5gc container via ${COMPOSE_FILE}"
  docker compose -f "${COMPOSE_FILE}" up -d --build 5gc
  wait_healthy

  if docker network inspect "${DOCKER_NETWORK_NAME}" >/dev/null 2>&1; then
    local gateway
    gateway="$(docker network inspect "${DOCKER_NETWORK_NAME}" -f '{{(index .IPAM.Config 0).Gateway}}' 2>/dev/null || true)"
    if [[ -n "${gateway}" ]]; then
      log "docker network ${DOCKER_NETWORK_NAME} gateway=${gateway}"
    fi
  fi

  ensure_route
  log "5gc is ready; webui should be reachable on http://127.0.0.1:9999"
}

main "$@"
