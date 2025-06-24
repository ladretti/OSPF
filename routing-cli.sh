#!/bin/bash

SOCKET="/tmp/routing.sock"

if [[ $# -eq 0 ]]; then
  echo "Usage: $0 {neighbors|stop}"
  exit 1
fi

echo "$1" | socat - UNIX-CONNECT:"$SOCKET"
