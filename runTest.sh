#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "Usage: $(basename "$0") <path/to/file.csv>"
  exit 1
fi
test -f "$1" || (echo "\"$1\": No such file or directory" && exit 1)

# Prepare the data
data/splitCSV.sh "$1"

# Spawn the coordinator process
build/coordinator "file://$(dirname "$(realpath "$1")")/filelist.csv" 4242 &

# Spawn some workers
for _ in {1..8}; do
  build/worker "localhost" "4242" &
done

# And wait for completion
time wait
