#!/usr/bin/env bash
# Build the current local MP2 changes and link a driver against real kernel objects.
set -euo pipefail
repo=$(git -C "$(dirname "$0")/.." rev-parse --show-toplevel)
if [ "$#" -ne 1 ]; then
  echo "Usage: bash scripts/test-mp2-syscalls.sh NEW_RESULTS_DIRECTORY" >&2
  exit 2
fi
mkdir -- "$1"
results=$(cd "$1" && pwd -P)
native=$(mktemp -d /tmp/nachos-syscalls.XXXXXX)
printf '%s\n' "$native" > "$results/build-path.txt"
git -C "$repo" archive 3f1ff05f1405b5c992f196f998cfb5756dd68bd4 | tar -x -C "$native"
paths=(code/threads/kernel.h code/threads/kernel.cc code/threads/thread.cc code/userprog/addrspace.cc code/userprog/exception.cc code/test/start.S code/filesys/filesys.h code/filesys/openfile.h code/lib/sysdep.h code/lib/sysdep.cc)
for path in "${paths[@]}"; do
  if [ "${NACHOS_SYSCALL_BASELINE:-0}" = 1 ] && [ "$path" = code/userprog/exception.cc ]; then continue; fi
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
    -c "$repo/tests/mp2_syscalls.cc" -o syscall_test.o
  objects=()
  for object in *.o; do
    if [ "$object" != main.o ]; then objects+=("$object"); fi
  done
  g++ -m32 "${objects[@]}" -o syscall-test
) > "$results/build.log" 2>&1
set +e
python3 "$repo/tests/mp2_syscalls.py" "$host/syscall-test" "$results/cases"
driver_status=$?
set -e
assignment="$native/NachOS-4.0_MP2"
find "$assignment/usr/local/nachos" -type f \( -path '*/bin/*' -o -name cc1 -o -name cpp -o -name collect2 \) -exec chmod u+x {} +
chmod u+x "$assignment/coff2noff/coff2noff.x86Linux"
(
  cd "$assignment/code/test"
  toolchain="$assignment/usr/local/nachos"
  flags=(-g -G 0 -c -I../userprog -I../lib "-B$toolchain/lib/gcc-lib/decstation-ultrix/2.95.2/" "-B$toolchain/decstation-ultrix/bin/")
  make clean
  make "CFLAGS=${flags[*]}" start.o
  cp "$repo/tests/mp2_syscalls_user.c" user_fixture.c
  for identity in 1 2; do
    if [ "$identity" = 1 ]; then program=syscall_a; else program=syscall_b; fi
    "$toolchain/bin/decstation-ultrix-gcc" "${flags[@]}" "-DTEST_ID=$identity" user_fixture.c -o "$program.o"
    "$toolchain/bin/decstation-ultrix-ld" -T script -N start.o "$program.o" -o "$program.coff"
    "$assignment/coff2noff/coff2noff.x86Linux" "$program.coff" "$program"
  done
) > "$results/user-build.log" 2>&1
python3 "$repo/tests/mp2_syscalls_integration.py" "$assignment/code/test" "$results/mips"
exit "$driver_status"
