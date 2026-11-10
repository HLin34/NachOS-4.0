#!/usr/bin/env bash
# Build the current local MP2 changes and link a driver against real kernel objects.
set -euo pipefail
repo=$(git -C "$(dirname "$0")/.." rev-parse --show-toplevel)
if [ "$#" -ne 1 ]; then
  echo "Usage: bash scripts/test-mp2-oom.sh NEW_RESULTS_DIRECTORY" >&2
  exit 2
fi
mkdir -- "$1"
results=$(cd "$1" && pwd -P)
native=$(mktemp -d /tmp/nachos-oom.XXXXXX)
printf '%s\n' "$native" > "$results/build-path.txt"
git -C "$repo" archive 3f1ff05f1405b5c992f196f998cfb5756dd68bd4 | tar -x -C "$native"
paths=(code/threads/kernel.h code/threads/kernel.cc code/threads/thread.cc code/userprog/addrspace.cc code/userprog/exception.cc code/test/start.S code/filesys/filesys.h code/filesys/openfile.h code/lib/sysdep.h code/lib/sysdep.cc)
for path in "${paths[@]}"; do
  cp "$repo/NachOS-4.0_MP2/$path" "$native/NachOS-4.0_MP2/$path"
done
host="$native/NachOS-4.0_MP2/code/build.linux"
(
  cd "$host"
  make clean
  make depend
  make CC='g++ -fpermissive -std=gnu++11'
  g++ -m32 -g -fpermissive -std=gnu++11 -DFILESYS_STUB -DRDATA -DSIM_FIX -Dx86 -DLINUX -DCHANGED \
    -iquote ../lib -iquote ../machine -iquote ../threads -iquote ../userprog -iquote ../filesys -iquote ../network \
    -c "$repo/tests/mp2_oom.cc" -o oom_test.o
  objects=()
  for object in *.o; do
    if [ "$object" != main.o ]; then objects+=("$object"); fi
  done
  g++ -m32 "${objects[@]}" -o oom-test
) > "$results/build.log" 2>&1
python3 "$repo/tests/mp2_oom.py" "$host/oom-test" "$results/cases"
assignment="$native/NachOS-4.0_MP2"
find "$assignment/usr/local/nachos" -type f \( -path '*/bin/*' -o -name cc1 -o -name cpp -o -name collect2 \) -exec chmod u+x {} +
chmod u+x "$assignment/coff2noff/coff2noff.x86Linux"
(
  cd "$assignment/code/test"
  make clean
  make CFLAGS="-g -G 0 -c -I../userprog -I../lib -B$assignment/usr/local/nachos/lib/gcc-lib/decstation-ultrix/2.95.2/ -B$assignment/usr/local/nachos/decstation-ultrix/bin/" consoleIO_test1 consoleIO_test2
) > "$results/user-build.log" 2>&1
python3 "$repo/tests/mp2_oom_integration.py" "$assignment/code/test" "$results/batch"
