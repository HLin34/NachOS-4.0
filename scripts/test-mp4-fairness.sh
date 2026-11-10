#!/usr/bin/env bash
set -euo pipefail
repo=$(git -C "$(dirname "$0")/.." rev-parse --show-toplevel)
if [ "$#" -ne 1 ]; then echo "Usage: bash scripts/test-mp4-fairness.sh NEW_RESULTS_DIRECTORY" >&2; exit 2; fi
mkdir -- "$1"
results=$(cd "$1" && pwd -P)
native=$(mktemp -d /tmp/nachos-mp4-fairness.XXXXXX)
cp "$repo/Dining-Philosopher/"{table.cpp,table.hpp} "$native/"
g++ -std=c++11 -g -O1 -Wall -Wextra -pthread -I"$native" "$native/table.cpp" "$repo/tests/mp4_fairness.cc" -Wl,--wrap=pthread_cond_wait -o "$native/test" > "$results/build.log" 2>&1
python3 "$repo/tests/mp4_fairness.py" "$native/test" "$results/native"
g++ -std=c++11 -g -O1 -fno-omit-frame-pointer -fsanitize=thread -no-pie -pthread -I"$native" "$native/table.cpp" "$repo/tests/mp4_fairness.cc" -Wl,--wrap=pthread_cond_wait -o "$native/tsan" > "$results/build-tsan.log" 2>&1
python3 "$repo/tests/mp4_fairness.py" "$native/tsan" "$results/tsan" --tsan
