#!/usr/bin/env bash
set -euo pipefail
repo=$(git -C "$(dirname "$0")/.." rev-parse --show-toplevel)
if [ "$#" -ne 1 ]; then echo "Usage: bash scripts/test-mp4-lifecycle.sh NEW_RESULTS_DIRECTORY" >&2; exit 2; fi
if [ "${NACHOS_MP4_BASELINE:-0}" = 1 ]; then
  echo "Current ID-aware tests require the maintained Table API; use retained historical baseline evidence." >&2
  exit 2
fi
mkdir -- "$1"
results=$(cd "$1" && pwd -P)
native=$(mktemp -d /tmp/nachos-mp4.XXXXXX)
printf '%s\n' "$native" > "$results/build-path.txt"
code="$native/Dining-Philosopher"
  mkdir "$code"
  for file in main.cpp fork.cpp table.cpp philosopher.cpp fork.hpp table.hpp philosopher.hpp config.hpp Makefile; do
    cp "$repo/Dining-Philosopher/$file" "$code/$file"
  done
cp "$code/config.hpp" "$native/default-config.hpp"
python3 - "$code/config.hpp" <<'PY'
import re,sys
from pathlib import Path
p=Path(sys.argv[1]);s=p.read_text()
s=re.sub(r'#define MINTHINKTIME .*','#define MINTHINKTIME 1',s)
s=re.sub(r'#define MAXTHINKTIME .*','#define MAXTHINKTIME 1',s)
p.write_text(s)
PY
sources=("$code/fork.cpp" "$code/table.cpp" "$code/philosopher.cpp")
wrap=(-Wl,--wrap=sleep,--wrap=pthread_cond_wait,--wrap=pthread_create,--wrap=pthread_join,--wrap=pthread_mutex_init,--wrap=pthread_mutex_destroy,--wrap=pthread_cond_destroy)
g++ -std=c++11 -g -O1 -Wall -Wextra -pthread -I"$code" "${sources[@]}" "$repo/tests/mp4_lifecycle.cc" "${wrap[@]}" -o "$native/lifecycle-test" > "$results/build.log" 2>&1
python3 "$repo/tests/mp4_lifecycle.py" "$native/lifecycle-test" "$results/cases"
for repeat in $(seq 1 10); do
  python3 "$repo/tests/mp4_lifecycle.py" "$native/lifecycle-test" "$results/stress-$repeat" --stress-only
done
cp "$code/config.hpp" "$native/fast-config.hpp"
for count in 2 7; do
  cp "$native/fast-config.hpp" "$code/config.hpp"
  sed -i "s/#define PHILOSOPHERS .*/#define PHILOSOPHERS $count/" "$code/config.hpp"
  g++ -std=c++11 -g -O1 -Wall -Wextra -pthread -I"$code" "${sources[@]}" "$repo/tests/mp4_lifecycle.cc" "${wrap[@]}" -o "$native/parameter-test" > "$results/build-n$count.log" 2>&1
  python3 "$repo/tests/mp4_lifecycle.py" "$native/parameter-test" "$results/n$count" --stress-only
done
cp "$native/default-config.hpp" "$code/config.hpp"
g++ -std=c++11 -g -O1 -Wall -Wextra -pthread -DMP4_WRAPPERS_ONLY -I"$code" "$code/main.cpp" "${sources[@]}" "$repo/tests/mp4_lifecycle.cc" "${wrap[@]}" -o "$native/application-test" > "$results/build-application.log" 2>&1
python3 "$repo/tests/mp4_lifecycle.py" "$native/application-test" "$results/application" --application
(cd "$code" && make clean && make) > "$results/make.log" 2>&1
python3 - "$code/main" "$results" <<'PY'
import json,subprocess,sys
from pathlib import Path
binary,out=sys.argv[1:];out=Path(out)
try:
    r=subprocess.run([binary],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=25)
    code,data=r.returncode,r.stdout
except subprocess.TimeoutExpired as e:code,data=124,e.stdout or b''
passed=code==0 and b'All philosophers have stopped.' in data
(out/'default-application.log').write_bytes(data)
(out/'default-application.json').write_text(json.dumps({'passed':passed,'returncode':code})+'\n')
print('default 15-second application: '+('PASS' if passed else 'FAIL'),flush=True)
raise SystemExit(0 if passed else 1)
PY
