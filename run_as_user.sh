#!/bin/sh

set -euo

OLD_UID=$1
OLD_GID=$2
shift; shift

groupadd --gid $OLD_GID --non-unique user 2>/dev/null || true
useradd --uid $OLD_UID --gid $OLD_GID --non-unique user 2>/dev/null || true

sudo -E -u user "$@"
