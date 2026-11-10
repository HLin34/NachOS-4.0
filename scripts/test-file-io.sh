#!/usr/bin/env bash
set -euo pipefail
repo=$(git -C "$(dirname "$0")/.." rev-parse --show-toplevel)
if [ "$#" -ne 1 ]; then echo "Usage: bash scripts/test-file-io.sh NEW_RESULTS_DIRECTORY" >&2; exit 2; fi
mkdir -- "$1"
results=$(cd "$1" && pwd -P)
native=$(mktemp -d /tmp/nachos-file-io.XXXXXX)
printf '%s\n' "$native" > "$results/build-path.txt"
git -C "$repo" archive 3f1ff05f1405b5c992f196f998cfb5756dd68bd4 | tar -x -C "$native"
failed=0
for mp in 1 2; do
  assignment="NachOS-4.0_MP$mp"
  for path in filesys/filesys.h filesys/openfile.h lib/sysdep.h lib/sysdep.cc; do
    cp "$repo/$assignment/code/$path" "$native/$assignment/code/$path"
  done
  code="$native/$assignment/code"
  g++ -m32 -g -fpermissive -std=gnu++11 -DFILESYS_STUB -Dx86 -DLINUX \
    -iquote "$code/lib" -iquote "$code/filesys" \
    "$repo/tests/file_io_errors.cc" "$code/lib/sysdep.cc" "$code/lib/debug.cc" \
    -Wl,--wrap=read,--wrap=write,--wrap=pread,--wrap=pwrite,--wrap=close \
    -o "$native/mp$mp-file-test" > "$results/mp$mp-build.log" 2>&1
  python3 "$repo/tests/file_io_errors.py" "$native/mp$mp-file-test" "$results/mp$mp" || failed=1
done
exit "$failed"
