#!/bin/sh

set -eu

OLD_UID=$1
OLD_GID=$2
shift 2

RUN_AS=hostuser

if id -u "$OLD_UID" >/dev/null 2>&1; then
  RUN_AS=$(getent passwd "$OLD_UID" | cut -d: -f1)
else
  groupadd --gid "$OLD_GID" --non-unique hostgroup 2>/dev/null || true
  useradd --uid "$OLD_UID" --gid "$OLD_GID" --non-unique "$RUN_AS"
fi

exec sudo -E -u "$RUN_AS" env USER=user LOGNAME=user CPU_LIMIT="${CPU_LIMIT:-4}" "$@"
