#!/usr/bin/env bash
set -euo pipefail
if [ "$#" -ne 1 ]; then
 echo "Usage: bash scripts/verify-build.sh NEW_RESULTS_DIRECTORY" >&2
 exit 2
fi
repo=$(cd "$(dirname "$0")/.." && pwd -P)
root=$(git -C "$repo" rev-parse --show-toplevel)
mkdir -- "$1"
results=$(cd "$1" && pwd -P)
native=$(mktemp -d /tmp/nachos-mp3-build-fix.XXXXXX)
printf '%s\n' "$native" > "$results/native-path.txt"
# MP3 is ordinary source now; no missing submodule or recovered Git object is needed.
git -C "$root" archive HEAD:NachOS-4.0_MP3 | tar -x -C "$native"
# Reuse the identical toolchain/converter already tracked in MP2.
# Export Git blobs, not a Windows checkout whose specs may have CRLF endings.
git -C "$root" archive HEAD NachOS-4.0_MP2/usr NachOS-4.0_MP2/coff2noff/coff2noff.x86Linux | tar -x --strip-components=1 -C "$native"
for file in code/threads/alarm.cc code/threads/scheduler.cc code/threads/scheduler.h code/threads/thread.cc code/threads/thread.h code/lib/list.cc code/build.linux/Makefile.dep code/threads/kernel.cc code/threads/kernel.h code/userprog/addrspace.cc code/userprog/addrspace.h code/userprog/exception.cc code/filesys/filesys.h code/filesys/openfile.h code/lib/sysdep.h code/lib/sysdep.cc code/userprog/ksyscall.h; do
 cp "$repo/$file" "$native/$file"
done
cd "$native/code/build.linux"
make clean > "$results/clean.log" 2>&1
# Do not mistake the archival prebuilt executable for a new successful link.
if [ -f nachos ]; then mv nachos "$native/archival-nachos"; fi
make depend CC='g++ -std=gnu++11 -m32' > "$results/dependencies.log" 2>&1
set +e
make -k CC='g++ -std=gnu++11' > "$results/host-build.log" 2>&1
status=$?
set -e
printf '%s\n' "$status" > "$results/host-build.status"
if [ "$status" -ne 0 ]; then
 echo 'Fresh host build: FAIL'
 exit 1
fi
file nachos > "$results/host-type.log"
echo 'Fresh host build: PASS'
find "$native/usr/local/nachos" -type f \( -path '*/bin/*' -o -name cc1 -o -name cpp -o -name collect2 \) -exec chmod u+x {} +
chmod u+x "$native/coff2noff/coff2noff.x86Linux"
cd "$native/code/test"
flags="-g -G 0 -c -I../userprog -I../lib -B$native/usr/local/nachos/lib/gcc-lib/decstation-ultrix/2.95.2/ -B$native/usr/local/nachos/decstation-ultrix/bin/"
make -B "CFLAGS=$flags" hw3t1 hw3t2 hw3t3 > "$results/user-build.log" 2>&1
echo 'Three MIPS targets: PASS'
python3 - "$results" <<'PY'
import subprocess,json,sys,re
from pathlib import Path
out=Path(sys.argv[1]);rows=[]
for name,args,timeout in [('single_no_trace',['-e','hw3t3'],8),('single_trace',['-d','z','-e','hw3t3'],8),('pair',['-e','hw3t1','-e','hw3t2'],8),('pair_reverse',['-e','hw3t2','-e','hw3t1'],8),('pair_trace',['-d','z','-e','hw3t1','-e','hw3t2'],8)]:
 try:
  r=subprocess.run(['../build.linux/nachos',*args],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=timeout)
  code,data=r.returncode,r.stdout
 except subprocess.TimeoutExpired as e:code,data=124,e.stdout or b''
 (out/(name+'.log')).write_bytes(data)
 returns=[int(v) for v in re.findall(rb'return value:\s*(-?\d+)',data)]
 # These are observations, not assertions of scheduling correctness.
 row={'case':name,'returncode':code,'returns':returns,'halted':b'Machine halting!' in data,'assertion':b'Assertion failed' in data,'trace_events':{chr(c):len(re.findall(rb'\['+bytes([c])+rb'\] Tick',data)) for c in range(65,70)}}
 if 'pair' in name:row['hw3t1_first_observed']=returns[:1]==[1]
 rows.append(row);print(json.dumps(row),flush=True)
(out/'smoke-observations.json').write_text(json.dumps(rows,indent=2)+'\n')
(out/'build-results.json').write_text(json.dumps({'host_build_passed':True,'compiler':'g++ -std=gnu++11 (no -fpermissive)','mips_targets':['hw3t1','hw3t2','hw3t3'],'runtime_is_observational':True},indent=2)+'\n')
PY
