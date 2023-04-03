#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "Usage: $(basename "$0") <path/to/file.csv>"
  exit 1
fi

file_base=${1%.*}
file_ext="${1##*.}"
file_path="$(dirname "$(realpath "$1")")"

# Split the csv into 100 equal chunks
split --number=r/100 --numeric-suffixes "$file_base.$file_ext" "$file_base." --additional-suffix=".$file_ext"

# And generate a curl compatible file list
find "$file_path" -name "$(basename "$file_base.*.$file_ext")" > "$file_path/filelist.csv"
sed -i 's|^|file://|' "$file_path/filelist.csv"
sort -o "$file_path/filelist.csv" "$file_path/filelist.csv"
