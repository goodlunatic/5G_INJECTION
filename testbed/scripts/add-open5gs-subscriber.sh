#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
TESTBED_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
SUBSCRIBER_ENV_FILE="${SUBSCRIBER_ENV_FILE:-${TESTBED_DIR}/subscribers/test-ue.env}"
CONTAINER_NAME="${CONTAINER_NAME:-testbed-open5gs}"
MONGODB_HOST="${MONGODB_HOST:-127.0.0.1}"

usage() {
  echo "Usage:"
  echo "  ./testbed/scripts/add-open5gs-subscriber.sh add-default"
  echo "  ./testbed/scripts/add-open5gs-subscriber.sh showfiltered"
  echo "  ./testbed/scripts/add-open5gs-subscriber.sh <command...>"
}

if [[ $# -lt 1 ]]; then
  usage
  exit 1
fi

case "$1" in
  add-default)
    if [[ ! -f "${SUBSCRIBER_ENV_FILE}" ]]; then
      echo "subscriber env not found: ${SUBSCRIBER_ENV_FILE}"
      exit 1
    fi
    # shellcheck disable=SC1090
    source "${SUBSCRIBER_ENV_FILE}"
    subscriber_ip="${SUBSCRIBER_IP:-10.45.0.2}"
    amf_value="${AMF:-9001}"
    qci_value="${QCI:-9}"
    subscriber_data="${IMSI},${K},opc,${OPC},${amf_value},${qci_value},${subscriber_ip}"
    exec docker exec -i "${CONTAINER_NAME}" python3 add_users.py --mongodb "${MONGODB_HOST}" --subscriber_data "${subscriber_data}"
    ;;
  showfiltered)
    exec docker exec -i "${CONTAINER_NAME}" mongosh --quiet "mongodb://${MONGODB_HOST}/open5gs" --eval "db.subscribers.find({}, {imsi:1, slice:1, _id:0}).toArray()"
    ;;
  *)
    exec docker exec -it "${CONTAINER_NAME}" "$@"
    ;;
esac
