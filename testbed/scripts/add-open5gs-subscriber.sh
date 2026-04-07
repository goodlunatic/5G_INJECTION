#!/usr/bin/env bash
set -euo pipefail

WORKSPACE_ROOT="${WORKSPACE_ROOT:-/workspace}"
SUBSCRIBER_ENV_FILE="${SUBSCRIBER_ENV_FILE:-${WORKSPACE_ROOT}/testbed/subscribers/test-ue.env}"
DB_URI="${DB_URI:-mongodb://127.0.0.1:27018/open5gs}"
OPEN5GS_DBCTL="${OPEN5GS_DBCTL:-/opt/src/open5gs/misc/db/open5gs-dbctl}"

usage() {
  echo "Usage:"
  echo "  ./testbed/scripts/add-open5gs-subscriber.sh add-default"
  echo "  ./testbed/scripts/add-open5gs-subscriber.sh showfiltered"
  echo "  ./testbed/scripts/add-open5gs-subscriber.sh <open5gs-dbctl args...>"
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
    exec docker exec -i testbed-open5gs "${OPEN5GS_DBCTL}" --db_uri="${DB_URI}" add_ue_with_apn "${IMSI}" "${K}" "${OPC}" "${APN:-internet}"
    ;;
  showfiltered)
    exec docker exec -i testbed-open5gs "${OPEN5GS_DBCTL}" --db_uri="${DB_URI}" showfiltered
    ;;
  *)
    exec docker exec -i testbed-open5gs "${OPEN5GS_DBCTL}" --db_uri="${DB_URI}" "$@"
    ;;
esac
