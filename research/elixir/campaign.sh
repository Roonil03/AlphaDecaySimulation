#!/bin/sh
set -eu
# flock releases ownership on normal exit and process crashes.
if [ "$#" -ne 4 ]; then echo "usage: campaign.sh DIR MANIFEST SCHEDULER CORE" >&2; exit 2; fi
mkdir -p "$1"
exec flock -n "$1/owner.lock" mix run -e 'Process.flag(:trap_exit, true); [dir, manifest, scheduler, core] = System.argv(); {:ok, pid} = Campaign.start_link(dir: dir, manifest: manifest, scheduler: scheduler, core: core); receive do {:EXIT, ^pid, :normal} -> :ok; {:EXIT, ^pid, reason} -> IO.inspect(reason); System.halt(1) end' -- "$@"
