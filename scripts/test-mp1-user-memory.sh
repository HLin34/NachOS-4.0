#!/usr/bin/env bash
# Build the current local MP1 changes and link a driver against real kernel objects.
set -euo pipefail
repo=$(git -C "$(dirname "$0")/.." rev-parse --show-toplevel)
if [ "$#" -ne 1 ]; then
  echo "Usage: bash scripts/test-mp1-user-memory.sh NEW_RESULTS_DIRECTORY" >&2
  exit 2
fi
mkdir -- "$1"
results=$(cd "$1" && pwd -P)
native=$(mktemp -d /tmp/nachos-mp1-user.XXXXXX)
printf '%s\n' "$native" > "$results/build-path.txt"
git -C "$repo" archive 3f1ff05f1405b5c992f196f998cfb5756dd68bd4 | tar -x -C "$native"
paths=(code/filesys/filesys.h code/filesys/openfile.h code/lib/sysdep.h code/lib/sysdep.cc code/userprog/exception.cc)
for path in "${paths[@]}"; do
  if [ "${NACHOS_MP1_BASELINE:-0}" = 1 ] && [ "$path" = code/userprog/exception.cc ]; then continue; fi
  cp "$repo/NachOS-4.0_MP1/$path" "$native/NachOS-4.0_MP1/$path"
done
host="$native/NachOS-4.0_MP1/code/build.linux"
(
  cd "$host"
  make clean
  make depend
  make CC='g++ -fpermissive -std=gnu++11'
  g++ -m32 -g -fpermissive -std=gnu++11 -DFILESYS_STUB -DRDATA -DSIM_FIX -Dx86 -DLINUX -DCHANGED \
    -iquote ../lib -iquote ../machine -iquote ../threads -iquote ../userprog -iquote ../filesys -iquote ../network \
    -c "$repo/tests/mp1_user_memory.cc" -o user_memory_test.o
  objects=()
  for object in *.o; do
    if [ "$object" != main.o ]; then objects+=("$object"); fi
  done
  g++ -m32 "${objects[@]}" -o user-memory-test
) > "$results/build.log" 2>&1
set +e
python3 "$repo/tests/mp1_user_memory.py" "$host/user-memory-test" "$results/cases"
direct_status=$?
set -e
if [ "${NACHOS_MP1_BASELINE:-0}" = 1 ]; then exit "$direct_status"; fi
assignment="$native/NachOS-4.0_MP1"
find "$assignment/usr/local/nachos" -type f \( -path '*/bin/*' -o -name cc1 -o -name cpp -o -name collect2 \) -exec chmod u+x {} +
chmod u+x "$assignment/coff2noff/coff2noff.x86Linux"
(
  cd "$assignment/code/test"
  toolchain="$assignment/usr/local/nachos"
  flags=(-g -G 0 -c -I../userprog -I../lib "-B$toolchain/lib/gcc-lib/decstation-ultrix/2.95.2/" "-B$toolchain/decstation-ultrix/bin/")
  make clean
  make "CFLAGS=${flags[*]}" start.o
  cp "$repo/tests/mp1_user_memory_user.c" user_memory_user.c
  "$toolchain/bin/decstation-ultrix-gcc" "${flags[@]}" user_memory_user.c -o user_memory_user.o
  "$toolchain/bin/decstation-ultrix-ld" -T script -N start.o user_memory_user.o -o user_memory_user.coff
  "$assignment/coff2noff/coff2noff.x86Linux" user_memory_user.coff user_memory_user
) > "$results/user-build.log" 2>&1
python3 "$repo/tests/mp1_user_memory_integration.py" "$assignment/code/test" "$results/mips"
exit "$direct_status"
