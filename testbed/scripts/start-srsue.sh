#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
TESTBED_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="$(cd -- "${TESTBED_DIR}/.." && pwd)"

WORKSPACE_ROOT="${WORKSPACE_ROOT:-${REPO_ROOT}}"
TEMPLATE="${SRSUE_CONFIG_TEMPLATE:-${WORKSPACE_ROOT}/testbed/configs/srsue/ue.conf.tpl}"
RENDERED="${SRSUE_CONFIG_RENDERED:-${WORKSPACE_ROOT}/testbed/runtime/ue.conf}"
LOG_DIR="${WORKSPACE_ROOT}/testbed/logs/srsue"
CONSOLE_LOG="${LOG_DIR}/ue-console.log"
SUBSCRIBER_ENV_FILE="${SUBSCRIBER_ENV_FILE:-${WORKSPACE_ROOT}/testbed/subscribers/test-ue.env}"
SRSRAN_4G_SRC="${SRSRAN_4G_SRC:-${WORKSPACE_ROOT}/testbed/sources/srsRAN_4G}"
SRSUE_BIN="${SRSUE_BIN:-${SRSRAN_4G_SRC}/build/srsue/src/srsue}"

mkdir -p "$(dirname "${RENDERED}")" "${LOG_DIR}"
touch "${CONSOLE_LOG}"

if [[ ! -f "${SUBSCRIBER_ENV_FILE}" ]]; then
  echo "subscriber env not found: ${SUBSCRIBER_ENV_FILE}"
  exit 1
fi

# shellcheck disable=SC1090
source "${SUBSCRIBER_ENV_FILE}"

UE_SRATE="${UE_SRATE:-23.04e6}"
UE_NOF_PRB="${UE_NOF_PRB:-106}"
UE_FREQ_OFFSET="${UE_FREQ_OFFSET:-0}"
TIME_ADV_NSAMPLES="${TIME_ADV_NSAMPLES:-100}"

B210_MASTER_CLOCK="${B210_MASTER_CLOCK:-}"
if [[ -z "${B210_MASTER_CLOCK}" ]]; then
  case "${UE_SRATE}" in
    15.36e6) B210_MASTER_CLOCK="30.72e6" ;;
    23.04e6) B210_MASTER_CLOCK="23.04e6" ;;
    11.52e6) B210_MASTER_CLOCK="23.04e6" ;;
    *) B210_MASTER_CLOCK="23.04e6" ;;
  esac
fi

B210_OTW_FORMAT="${B210_OTW_FORMAT:-sc12}"
B210_NUM_RECV_FRAMES="${B210_NUM_RECV_FRAMES:-256}"
B210_NUM_SEND_FRAMES="${B210_NUM_SEND_FRAMES:-256}"
B210_RECV_FRAME_SIZE="${B210_RECV_FRAME_SIZE:-8184}"
B210_SEND_FRAME_SIZE="${B210_SEND_FRAME_SIZE:-8184}"
B210_CLOCK_SOURCE="${B210_CLOCK_SOURCE:-external}"
B210_TIME_SOURCE="${B210_TIME_SOURCE:-external}"
B210_EXTRA_DEVICE_ARGS="${B210_EXTRA_DEVICE_ARGS:-}"

B210_DEVICE_ARGS="type=b200,master_clock_rate=${B210_MASTER_CLOCK},clock=${B210_CLOCK_SOURCE},otw_format=${B210_OTW_FORMAT},num_recv_frames=${B210_NUM_RECV_FRAMES},num_send_frames=${B210_NUM_SEND_FRAMES},recv_frame_size=${B210_RECV_FRAME_SIZE},send_frame_size=${B210_SEND_FRAME_SIZE}"
if [[ -n "${B210_TIME_SOURCE}" ]]; then
  B210_DEVICE_ARGS="${B210_DEVICE_ARGS},time_source=${B210_TIME_SOURCE}"
fi
if [[ -n "${B210_SERIAL:-}" ]]; then
  B210_DEVICE_ARGS="${B210_DEVICE_ARGS},serial=${B210_SERIAL}"
fi
if [[ -n "${B210_EXTRA_DEVICE_ARGS}" ]]; then
  B210_DEVICE_ARGS="${B210_DEVICE_ARGS},${B210_EXTRA_DEVICE_ARGS}"
fi

sed \
  -e "s|__B210_DEVICE_ARGS__|${B210_DEVICE_ARGS}|g" \
  -e "s|__UE_SRATE__|${UE_SRATE}|g" \
  -e "s|__UE_FREQ_OFFSET__|${UE_FREQ_OFFSET}|g" \
  -e "s|__TIME_ADV_NSAMPLES__|${TIME_ADV_NSAMPLES}|g" \
  -e "s|__B210_TX_GAIN__|${B210_TX_GAIN:-80}|g" \
  -e "s|__B210_RX_GAIN__|${B210_RX_GAIN:-40}|g" \
  -e "s|__NR_DL_ARFCN__|${NR_DL_ARFCN:-368500}|g" \
  -e "s|__SSB_NR_ARFCN__|${SSB_NR_ARFCN:-368410}|g" \
  -e "s|__UE_NOF_PRB__|${UE_NOF_PRB}|g" \
  -e "s|__UE_IMSI__|${IMSI}|g" \
  -e "s|__UE_K__|${K}|g" \
  -e "s|__UE_OPC__|${OPC}|g" \
  -e "s|__UE_APN__|${APN:-internet}|g" \
  "${TEMPLATE}" > "${RENDERED}"

if [[ ! -x "${SRSUE_BIN}" ]]; then
  echo "srsUE binary not found: ${SRSUE_BIN}"
  exit 1
fi

echo "rendered config: ${RENDERED}"
echo "using B210 serial=${B210_SERIAL:-auto} clock=${B210_CLOCK_SOURCE} time_source=${B210_TIME_SOURCE}"

# Best-effort realtime settings to reduce B210 overruns in 20 MHz mode.
ulimit -l unlimited 2>/dev/null || true
SRSUE_RTPRIO="${SRSUE_RTPRIO:-20}"
SRSUE_CPUSET="${SRSUE_CPUSET:-2-5}"
cmd=("${SRSUE_BIN}" "${RENDERED}")

if command -v taskset >/dev/null 2>&1; then
  cmd=(taskset -c "${SRSUE_CPUSET}" "${cmd[@]}")
fi

if command -v chrt >/dev/null 2>&1; then
  if chrt -f "${SRSUE_RTPRIO}" /bin/true >/dev/null 2>&1; then
    cmd=(chrt -f "${SRSUE_RTPRIO}" "${cmd[@]}")
  else
    echo "warning: cannot enable SCHED_FIFO priority ${SRSUE_RTPRIO}; continuing without chrt"
  fi
fi

exec > >(tee -a "${CONSOLE_LOG}") 2>&1
echo "launching: ${cmd[*]}"
exec "${cmd[@]}"
