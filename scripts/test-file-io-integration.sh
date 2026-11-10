#!/usr/bin/env bash
set -euo pipefail
repo=$(git -C "$(dirname "$0")/.." rev-parse --show-toplevel)
if [ "$#" -ne 1 ]; then echo "Usage: bash scripts/test-file-io-integration.sh NEW_RESULTS_DIRECTORY" >&2; exit 2; fi
mkdir -- "$1"
results=$(cd "$1" && pwd -P)
native=$(mktemp -d /tmp/nachos-file-integration.XXXXXX)
printf '%s\n' "$native" > "$results/build-path.txt"
git -C "$repo" archive 3f1ff05f1405b5c992f196f998cfb5756dd68bd4 | tar -x -C "$native"
failed=0
for mp in 1 2; do
  assignment="$native/NachOS-4.0_MP$mp"
  paths=(code/filesys/filesys.h code/filesys/openfile.h code/lib/sysdep.h code/lib/sysdep.cc)
  if [ "$mp" = 1 ]; then paths+=(code/userprog/exception.cc); fi
  if [ "$mp" = 2 ]; then
    paths+=(code/threads/kernel.h code/threads/kernel.cc code/threads/thread.cc code/userprog/addrspace.cc code/userprog/exception.cc code/test/start.S)
  fi
  for path in "${paths[@]}"; do cp "$repo/NachOS-4.0_MP$mp/$path" "$assignment/$path"; done
  (cd "$assignment/code/build.linux" && make clean && make depend && make CC='g++ -fpermissive -std=gnu++11') > "$results/mp$mp-host.log" 2>&1
  find "$assignment/usr/local/nachos" -type f \( -path '*/bin/*' -o -name cc1 -o -name cpp -o -name collect2 \) -exec chmod u+x {} +
  chmod u+x "$assignment/coff2noff/coff2noff.x86Linux"
  (
    cd "$assignment/code/test"
    toolchain="$assignment/usr/local/nachos"
    flags=(-g -G 0 -c -I../userprog -I../lib "-B$toolchain/lib/gcc-lib/decstation-ultrix/2.95.2/" "-B$toolchain/decstation-ultrix/bin/")
    make clean
    make "CFLAGS=${flags[*]}" start.o fileIO_test1 fileIO_test2
    cp "$repo/tests/file_io_user.c" file_io_user.c
    if [ "$mp" = 1 ]; then flags+=(-DMP1_COURSE_MODE); fi
    "$toolchain/bin/decstation-ultrix-gcc" "${flags[@]}" file_io_user.c -o file_io_user.o
    "$toolchain/bin/decstation-ultrix-ld" -T script -N start.o file_io_user.o -o file_io_user.coff
    "$assignment/coff2noff/coff2noff.x86Linux" file_io_user.coff file_io_user
  ) > "$results/mp$mp-user.log" 2>&1
  python3 "$repo/tests/file_io_integration.py" "$assignment/code/test" "$results/mp$mp" --mp "$mp" || failed=1
done
exit "$failed"
