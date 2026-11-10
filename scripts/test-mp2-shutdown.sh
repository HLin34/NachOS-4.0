#!/usr/bin/env bash
# Build the historical baseline and the focused local fix on Linux-native storage.
set -euo pipefail
repo=$(git -C "$(dirname "$0")/.." rev-parse --show-toplevel)
if [ "$#" -ne 1 ]; then
  echo "Usage: bash scripts/test-mp2-shutdown.sh NEW_RESULTS_DIRECTORY" >&2
  exit 2
fi
mkdir -- "$1"
results=$(cd "$1" && pwd -P)
native=$(mktemp -d /tmp/nachos-shutdown-regression.XXXXXX)
printf '%s\n' "$native" > "$results/build-path.txt"
baseline=3f1ff05f1405b5c992f196f998cfb5756dd68bd4
paths=(code/threads/kernel.h code/threads/kernel.cc code/threads/thread.cc code/userprog/addrspace.cc code/userprog/exception.cc code/test/start.S code/filesys/filesys.h code/filesys/openfile.h code/lib/sysdep.h code/lib/sysdep.cc)
build() {
  local assignment="$1" label="$2"
  (cd "$assignment/code/build.linux" && make clean && make depend && make CC='g++ -fpermissive -std=gnu++11') > "$results/$label-host.log" 2>&1
  find "$assignment/usr/local/nachos" -type f \( -path '*/bin/*' -o -name cc1 -o -name cpp -o -name collect2 \) -exec chmod u+x {} +
  chmod u+x "$assignment/coff2noff/coff2noff.x86Linux"
  (cd "$assignment/code/test" && make clean && make CFLAGS="-g -G 0 -c -I../userprog -I../lib -B$assignment/usr/local/nachos/lib/gcc-lib/decstation-ultrix/2.95.2/ -B$assignment/usr/local/nachos/decstation-ultrix/bin/" consoleIO_test1 consoleIO_test2) > "$results/$label-user.log" 2>&1
}
mkdir "$native/baseline" "$native/fixed"
git -C "$repo" archive "$baseline" | tar -x -C "$native/baseline"
build "$native/baseline/NachOS-4.0_MP2" baseline
set +e
python3 "$repo/tests/mp2_shutdown.py" "$native/baseline/NachOS-4.0_MP2/code/test" "$results/baseline" --case pair
baseline_status=$?
set -e
if [ "$baseline_status" -ne 1 ] || ! python3 - "$results/baseline/results.json" <<'PY'
import json, sys
case = json.load(open(sys.argv[1]))[0]
assert case['timed_out'] and case['checks']['all_programs_returned']
PY
then
  echo "Baseline did not reproduce the expected completed-output/timeout failure."
  exit 1
fi
git -C "$repo" archive "$baseline" | tar -x -C "$native/fixed"
for path in "${paths[@]}"; do
  cp "$repo/NachOS-4.0_MP2/$path" "$native/fixed/NachOS-4.0_MP2/$path"
done
build "$native/fixed/NachOS-4.0_MP2" fixed
python3 "$repo/tests/mp2_shutdown.py" "$native/fixed/NachOS-4.0_MP2/code/test" "$results/fixed"
