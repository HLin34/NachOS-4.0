#!/usr/bin/env bash
set -euo pipefail
repo=$(git -C "$(dirname "$0")/.." rev-parse --show-toplevel)
if [ "$#" -ne 2 ]; then echo "Usage: bash scripts/test-mp4-helgrind.sh NEW_RESULTS_DIRECTORY VALGRIND_RUNTIME_DIRECTORY" >&2; exit 2; fi
if [ "${NACHOS_MP4_BASELINE:-0}" = 1 ]; then
  echo "Current ID-aware tests require the maintained Table API; use retained historical baseline evidence." >&2
  exit 2
fi
mkdir -- "$1"
results=$(cd "$1" && pwd -P)
runtime="$2"
native=$(mktemp -d /tmp/nachos-mp4-helgrind.XXXXXX)
code="$native/Dining-Philosopher"
  mkdir "$code"
  for file in fork.cpp table.cpp philosopher.cpp fork.hpp table.hpp philosopher.hpp config.hpp; do cp "$repo/Dining-Philosopher/$file" "$code/$file"; done
g++ -std=c++11 -g -O1 -pthread -I"$code" "$code/fork.cpp" "$code/table.cpp" "$code/philosopher.cpp" \
  "$repo/tests/mp4_helgrind.cc" -Wl,--wrap=sleep -o "$native/checked-test" > "$results/build.log" 2>&1
VALGRIND_LIB="$runtime/usr/libexec/valgrind" python3 - "$runtime/usr/bin/valgrind" "$native/checked-test" "$results" <<'PY'
import json,os,subprocess,sys
from pathlib import Path
tool,binary,out=sys.argv[1:];out=Path(out);rows=[];checker=os.environ.get('MP4_VALGRIND_TOOL','helgrind')
for case in ['turn_race','stress']:
    try:
        r=subprocess.run([tool,'--tool='+checker,'--error-exitcode=67','--exit-on-first-error=yes',binary,case],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=30)
        code,data=r.returncode,r.stdout
    except subprocess.TimeoutExpired as e:code,data=124,e.stdout or b''
    (out/(case+'.log')).write_bytes(data)
    passed=code==0 and b'ERROR SUMMARY: 0 errors' in data and b'PASS' in data
    rows.append({'case':case,'passed':passed,'returncode':code})
    print(checker+' '+case+': '+('PASS' if passed else 'FAIL'),flush=True)
(out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
raise SystemExit(0 if all(r['passed'] for r in rows) else 1)
PY
