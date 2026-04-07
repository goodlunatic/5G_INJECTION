#!/usr/bin/env bash
set -euo pipefail

WORKSPACE_ROOT="${WORKSPACE_ROOT:-/workspace}"
OPEN5GS_SRC="${OPEN5GS_SRC:-/opt/src/open5gs}"
OPEN5GS_BUILD="${OPEN5GS_BUILD:-${OPEN5GS_SRC}/build}"
OPEN5GS_CONFIG="${OPEN5GS_CONFIG:-${WORKSPACE_ROOT}/testbed/configs/open5gs/open5gs.yml}"
LOG_DIR="${LOG_DIR:-${WORKSPACE_ROOT}/testbed/logs/open5gs}"
SUPERVISOR_LOG="${LOG_DIR}/open5gs-supervisor.log"
SUBSCRIBER_ENV_FILE="${SUBSCRIBER_ENV_FILE:-${WORKSPACE_ROOT}/testbed/subscribers/test-ue.env}"
DB_URI="${DB_URI:-mongodb://127.0.0.1:27018/open5gs}"
OGSTUN_DEV="${OGSTUN_DEV:-ogstun}"
OGSTUN_IPV4_CIDR="${OGSTUN_IPV4_CIDR:-10.45.0.1/16}"
OGSTUN_IPV6_CIDR="${OGSTUN_IPV6_CIDR:-2001:db8:cafe::1/48}"
OGSTUN_NAT_IPV4_CIDR="${OGSTUN_NAT_IPV4_CIDR:-10.45.0.0/16}"
ENABLE_NAT="${ENABLE_NAT:-false}"
WAN_IFACE="${WAN_IFACE:-}"

mkdir -p "${LOG_DIR}"
touch "${SUPERVISOR_LOG}"

log() {
  echo "[$(date '+%F %T')] $*" | tee -a "${SUPERVISOR_LOG}"
}

setup_ogstun() {
  if ! ip link show "${OGSTUN_DEV}" >/dev/null 2>&1; then
    log "creating ${OGSTUN_DEV}"
    ip tuntap add name "${OGSTUN_DEV}" mode tun
  fi

  ip addr replace "${OGSTUN_IPV4_CIDR}" dev "${OGSTUN_DEV}"
  ip link set "${OGSTUN_DEV}" up

  if [[ -n "${OGSTUN_IPV6_CIDR}" ]]; then
    ip -6 addr replace "${OGSTUN_IPV6_CIDR}" dev "${OGSTUN_DEV}" nodad || true
  fi

  sysctl -w net.ipv4.ip_forward=1 >/dev/null
  sysctl -w net.ipv6.conf.all.forwarding=1 >/dev/null || true

  if [[ "${ENABLE_NAT}" == "true" && -n "${WAN_IFACE}" ]]; then
    log "enabling NAT via ${WAN_IFACE}"
    iptables -t nat -C POSTROUTING -s "${OGSTUN_NAT_IPV4_CIDR}" -o "${WAN_IFACE}" -j MASQUERADE 2>/dev/null || \
      iptables -t nat -A POSTROUTING -s "${OGSTUN_NAT_IPV4_CIDR}" -o "${WAN_IFACE}" -j MASQUERADE
  fi
}

wait_mongo() {
  until mongosh "${DB_URI}" --quiet --eval "db.adminCommand('ping').ok" >/dev/null 2>&1; do
    sleep 1
  done
}

ensure_default_subscriber() {
  if [[ ! -f "${SUBSCRIBER_ENV_FILE}" ]]; then
    log "subscriber env not found: ${SUBSCRIBER_ENV_FILE}"
    return
  fi

  # shellcheck disable=SC1090
  source "${SUBSCRIBER_ENV_FILE}"

  if [[ -z "${IMSI:-}" || -z "${K:-}" || -z "${OPC:-}" ]]; then
    log "subscriber env missing IMSI/K/OPC"
    return
  fi

  if mongosh "${DB_URI}" --quiet --eval "db.subscribers.countDocuments({imsi:'${IMSI}'})" | grep -qx '1'; then
    log "subscriber ${IMSI} already present"
    return
  fi

  log "adding default subscriber ${IMSI}"
  "${OPEN5GS_SRC}/misc/db/open5gs-dbctl" --db_uri="${DB_URI}" add_ue_with_apn "${IMSI}" "${K}" "${OPC}" "${APN:-internet}"
}

start_daemon() {
  local daemon="$1"
  local bin_path
  bin_path="$(find "${OPEN5GS_BUILD}" -type f -name "${daemon}" | head -n 1)"
  if [[ -z "${bin_path}" ]]; then
    log "warning: ${daemon} not found"
    return
  fi
  log "starting ${daemon}"
  "${bin_path}" -c "${OPEN5GS_CONFIG}" >>"${LOG_DIR}/${daemon}.stdout.log" 2>&1 &
}

cleanup() {
  jobs -pr | xargs -r kill
  wait || true
}
trap cleanup EXIT INT TERM

setup_ogstun
wait_mongo
ensure_default_subscriber

for daemon in \
  open5gs-nrfd \
  open5gs-amfd \
  open5gs-smfd \
  open5gs-upfd \
  open5gs-ausfd \
  open5gs-udmd \
  open5gs-udrd \
  open5gs-pcfd \
  open5gs-nssfd
do
  start_daemon "${daemon}"
done

wait -n
