#!/bin/bash
# Run shadower and save output to logs/ directory with timestamp

CONFIG="${1:-configs/srsran-n3-20MHz-x310-lab.yaml}"
LOGFILE="logs/shadower_$(date +%Y%m%d_%H%M%S).log"

echo "Config : $CONFIG"
echo "Log    : $LOGFILE"
echo "---"

./build/shadower/shadower "$CONFIG" 2>&1 | tee "$LOGFILE"
