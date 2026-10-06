#!/bin/sh
# SPDX-License-Identifier: Zlib
set -eu
export XDG_RUNTIME_DIR=$(mktemp -d)
export GSK_RENDERER=cairo
weston --backend=headless-backend.so --socket=zatura-test-weston --idle-time=0 &
weston_pid=$!
cleanup() {
  kill "$weston_pid" 2>/dev/null || true
  wait "$weston_pid" 2>/dev/null || true
  rm -rf "$XDG_RUNTIME_DIR"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
for i in $(seq 20); do
  [ -S "$XDG_RUNTIME_DIR/zatura-test-weston" ] && break
  if ! kill -0 "$weston_pid" 2>/dev/null; then
    echo 'Weston exited before creating its test socket' >&2
    exit 1
  fi
  sleep 0.25
done
if [ ! -S "$XDG_RUNTIME_DIR/zatura-test-weston" ]; then
  echo 'Weston did not create its test socket' >&2
  exit 1
fi
"$@"
