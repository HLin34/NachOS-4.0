#!/usr/bin/env bash
# Export the committed baseline and build only in a new destination directory.
set -u
set -o pipefail
baseline=3f1ff05f1405b5c992f196f998cfb5756dd68bd4
if [ "$#" -ne 1 ]; then
  echo "Usage: bash scripts/verify-baseline.sh NEW_OUTPUT_DIRECTORY" >&2
  exit 2
fi
repo_root=$(git -C "$(dirname "$0")/.." rev-parse --show-toplevel) || exit 2
mkdir -- "$1" || exit 2
output_root=$(cd "$1" && pwd -P) || exit 2
case "$output_root" in
  /mnt/*) echo "Use a Linux-native destination, such as /tmp/nachos-verification-03; the legacy compiler fails on the inspected Windows mount." >&2; exit 2 ;;
esac
exec > >(tee "$output_root/verification.log") 2>&1
echo "Baseline: $baseline"
date -u
uname -a
for tool in git tar make g++ ed timeout; do
  command -v "$tool" || exit 2
done
g++ --version | head -n 1
printf 'int main() { return 0; }\n' > "$output_root/compiler-check.cc"
if ! g++ -m32 "$output_root/compiler-check.cc" -o "$output_root/compiler-check"; then
  echo "BLOCKED: 32-bit C++ compile/link dependencies unavailable."
  echo "On the inspected Ubuntu host: install g++-multilib with an authenticated administrator session."
  exit 2
fi
git -C "$repo_root" archive "$baseline" | tar -x -C "$output_root" || exit 2
failed=0
for mp in 1 2; do
  assignment="$output_root/NachOS-4.0_MP$mp"
  host="$assignment/code/build.linux"
  echo "MP$mp: regenerate host dependencies and rebuild"
  host_options=()
  if [ "${NACHOS_COMPAT:-0}" = 1 ]; then
    host_options=('CC=g++ -fpermissive -std=gnu++11')
  fi
  if ! (cd "$host" && make clean && make depend && make "${host_options[@]}") > "$output_root/mp$mp-host.log" 2>&1; then
    echo "FAIL: MP$mp host build; see mp$mp-host.log"
    failed=1
    continue
  fi
  # Only adjust file modes in the exported build directory.
  find "$assignment/usr/local/nachos" -type f \( -path '*/bin/*' -o -name cc1 -o -name cpp -o -name collect2 \) -exec chmod u+x {} +
  chmod u+x "$assignment/coff2noff/coff2noff.x86Linux" || exit 2
  if [ "$mp" = 1 ]; then
    targets=(add halt createFile fileIO_test1 fileIO_test2 LotOfAdd)
  else
    targets=(consoleIO_test1 consoleIO_test2)
  fi
  user_flags="-g -G 0 -c -I../userprog -I../lib -B$assignment/usr/local/nachos/lib/gcc-lib/decstation-ultrix/2.95.2/ -B$assignment/usr/local/nachos/decstation-ultrix/bin/"
  if ! (cd "$assignment/code/test" && make clean && make "CFLAGS=$user_flags" "${targets[@]}") > "$output_root/mp$mp-user-build.log" 2>&1; then
    echo "FAIL: MP$mp MIPS user-program build; see mp$mp-user-build.log"
    failed=1
    continue
  fi
  if [ "$mp" = 1 ]; then
    (cd "$assignment/code/test" && timeout -k 3 20 ../build.linux/nachos -e fileIO_test1 && timeout -k 3 20 ../build.linux/nachos -e fileIO_test2) > "$output_root/mp$mp-run.log" 2>&1
  else
    (cd "$assignment/code/test" && timeout -k 3 20 ../build.linux/nachos -e consoleIO_test1 -e consoleIO_test2) > "$output_root/mp$mp-run.log" 2>&1
  fi
  result=$?
  echo "MP$mp example-run exit: $result"
  if [ "$result" -ne 0 ]; then failed=1; fi
done
echo "Build/example checks only; hidden tests, correctness and fairness are not certified."
exit "$failed"
