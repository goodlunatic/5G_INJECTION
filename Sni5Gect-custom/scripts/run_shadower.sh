#!/usr/bin/env bash
set -euo pipefail

CONFIG="${1:-configs/srsran-n3-20MHz-x310.yaml}"
LOG_DIR="${LOG_DIR:-logs}"
STAMP="$(date +%Y%m%d_%H%M%S)"
LOGFILE="${LOG_DIR}/shadower_${STAMP}.log"

echo "Config : $CONFIG"
echo "Log    : $LOGFILE"
echo "---"

mkdir -p "$LOG_DIR"
rm -f "$LOG_DIR"/UE-*.pcap "$LOG_DIR"/decode_rate_report.txt

set +e
./build/shadower/shadower "$CONFIG" 2>&1 | tee "$LOGFILE"
status=${PIPESTATUS[0]}
set -e

if [[ -f scripts/decode_rate.py ]]; then
  python3 scripts/decode_rate.py | tee "${LOG_DIR}/decode_rate_${STAMP}.txt" || true
fi

exit "$status"
