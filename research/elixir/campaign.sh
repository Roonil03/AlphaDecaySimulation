#!/bin/sh
set -eu
if [ "$#" -lt 4 ] || [ "$#" -gt 5 ]; then echo "usage: campaign.sh DIR MANIFEST SCHEDULER CORE" >&2; exit 2; fi
mkdir -p "$1"
exec flock -n "$1/owner.lock" mix run run.exs "$@"


