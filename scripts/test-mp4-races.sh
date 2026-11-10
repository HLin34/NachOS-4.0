#!/usr/bin/env bash
set -euo pipefail
repo=$(git -C "$(dirname "$0")/.." rev-parse --show-toplevel)
if [ "$#" -ne 1 ]; then echo "Usage: bash scripts/test-mp4-races.sh NEW_RESULTS_DIRECTORY" >&2; exit 2; fi
if [ "${NACHOS_MP4_BASELINE:-0}" = 1 ]; then
  echo "Current ID-aware tests require the maintained Table API; use retained historical baseline evidence." >&2
  exit 2
fi
mkdir -- "$1"
results=$(cd "$1" && pwd -P)
native=$(mktemp -d /tmp/nachos-mp4-tsan.XXXXXX)
code="$native/Dining-Philosopher"
  mkdir "$code"
  for file in fork.cpp table.cpp philosopher.cpp fork.hpp table.hpp philosopher.hpp config.hpp; do cp "$repo/Dining-Philosopher/$file" "$code/$file"; done
sed -i -e 's/#define MINTHINKTIME .*/#define MINTHINKTIME 1/' -e 's/#define MAXTHINKTIME .*/#define MAXTHINKTIME 1/' "$code/config.hpp"
g++ -std=c++11 -g -O1 -fno-omit-frame-pointer -fsanitize=thread -no-pie -pthread -I"$code" \
  "$code/fork.cpp" "$code/table.cpp" "$code/philosopher.cpp" "$repo/tests/mp4_lifecycle.cc" \
  -Wl,--wrap=sleep,--wrap=pthread_cond_wait,--wrap=pthread_create,--wrap=pthread_join,--wrap=pthread_mutex_init,--wrap=pthread_mutex_destroy,--wrap=pthread_cond_destroy \
  -o "$native/race-test" > "$results/build.log" 2>&1
python3 "$repo/tests/mp4_races.py" "$native/race-test" "$results/cases"
