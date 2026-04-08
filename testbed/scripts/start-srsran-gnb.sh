#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
TESTBED_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="$(cd -- "${TESTBED_DIR}/.." && pwd)"

WORKSPACE_ROOT="${WORKSPACE_ROOT:-${REPO_ROOT}}"
TEMPLATE="${GNB_CONFIG_TEMPLATE:-${WORKSPACE_ROOT}/testbed/configs/srsran/gnb.yml.tpl}"
RENDERED="${GNB_CONFIG_RENDERED:-${WORKSPACE_ROOT}/testbed/runtime/gnb.yml}"
LOG_DIR="${WORKSPACE_ROOT}/testbed/logs/srsran"
CONSOLE_LOG="${LOG_DIR}/gnb-console.log"
SRSRAN_PROJECT_SRC="${SRSRAN_PROJECT_SRC:-${WORKSPACE_ROOT}/testbed/sources/srsRAN_Project}"
GNB_BIN="${GNB_BIN:-${SRSRAN_PROJECT_SRC}/build/apps/gnb/gnb}"
OPEN5GS_IP="${OPEN5GS_IP:-10.53.1.2}"
DOCKER_NETWORK_NAME="${DOCKER_NETWORK_NAME:-testbed_ran}"

mkdir -p "$(dirname "${RENDERED}")" "${LOG_DIR}"
touch "${CONSOLE_LOG}"

exec > >(tee -a "${CONSOLE_LOG}") 2>&1

X310_TIME_SOURCE="${X310_TIME_SOURCE:-${X310_SYNC:-internal}}"
X310_DEVICE_ARGS="${X310_DEVICE_ARGS:-type=x300,addr=${X310_ADDR:-192.168.40.2},time_source=${X310_TIME_SOURCE},master_clock_rate=184.32e6,num_recv_frames=64,num_send_frames=64}"
GNB_BIND_ADDR="${GNB_BIND_ADDR:-$(docker network inspect "${DOCKER_NETWORK_NAME}" -f '{{(index .IPAM.Config 0).Gateway}}' 2>/dev/null || true)}"
GNB_BIND_ADDR="${GNB_BIND_ADDR:-127.0.0.1}"

sed \
  -e "s|__OPEN5GS_IP__|${OPEN5GS_IP}|g" \
  -e "s|__GNB_BIND_ADDR__|${GNB_BIND_ADDR}|g" \
  -e "s|__X310_ADDR__|${X310_ADDR:-192.168.40.2}|g" \
  -e "s|__X310_DEVICE_ARGS__|${X310_DEVICE_ARGS}|g" \
  -e "s|__X310_CLOCK__|${X310_CLOCK:-external}|g" \
  -e "s|__X310_SYNC__|${X310_SYNC:-external}|g" \
  -e "s|__GNB_SRATE__|${GNB_SRATE:-23.04}|g" \
  -e "s|__GNB_TX_GAIN__|${GNB_TX_GAIN:-31.5}|g" \
  -e "s|__GNB_RX_GAIN__|${GNB_RX_GAIN:-31.5}|g" \
  -e "s|__NR_DL_ARFCN__|${NR_DL_ARFCN:-368500}|g" \
  -e "s|__NR_BW_MHZ__|${NR_BW_MHZ:-20}|g" \
  "${TEMPLATE}" > "${RENDERED}"

if [[ ! -x "${GNB_BIN}" ]]; then
  echo "gNB binary not found: ${GNB_BIN}"
  exit 1
fi

echo "using X310 at ${X310_ADDR:-192.168.40.2}"
echo "using Open5GS AMF at ${OPEN5GS_IP} with gNB bind_addr=${GNB_BIND_ADDR}"
echo "using X310 device_args=${X310_DEVICE_ARGS}"
echo "using X310 clock=${X310_CLOCK:-external} sync=${X310_SYNC:-internal} time_source=${X310_TIME_SOURCE}"
echo "rendered config: ${RENDERED}"
exec "${GNB_BIN}" -c "${RENDERED}"
