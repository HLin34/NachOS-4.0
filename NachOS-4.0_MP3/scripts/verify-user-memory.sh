#!/usr/bin/env bash
set -euo pipefail
if [ "$#" -ne 1 ]; then
 echo "Usage: bash scripts/verify-user-memory.sh NEW_RESULTS_DIRECTORY" >&2
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
cp -a "$root/NachOS-4.0_MP2/usr" "$native/usr"
mkdir -p "$native/coff2noff"
cp "$root/NachOS-4.0_MP2/coff2noff/coff2noff.x86Linux" "$native/coff2noff/"
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
# Link real maintained Thread/Scheduler/Alarm objects. Only physical context
# switching and the hardware return-yield request are intercepted in unit tests.
objects=()
for file in *.o; do if [ "$file" != main.o ]; then objects+=("$file"); fi; done
g++ -m32 -std=gnu++11 -g -Wall -DFILESYS_STUB -DRDATA -DSIM_FIX -Dx86 -DLINUX -DCHANGED \
 -iquote ../network -iquote ../filesys -iquote ../userprog -iquote ../threads -iquote ../machine -iquote ../lib \
 "$repo/tests/scheduling.cc" "${objects[@]}" \
 -Wl,--wrap=SWITCH,--wrap=_ZN9Interrupt13YieldOnReturnEv -o "$native/scheduling-test" > "$results/unit-build.log" 2>&1
python3 "$repo/tests/scheduling.py" "$native/scheduling-test" "$results/unit"
g++ -m32 -std=gnu++11 -g -Wall -DFILESYS_STUB -DRDATA -DSIM_FIX -Dx86 -DLINUX -DCHANGED \
 -iquote ../network -iquote ../filesys -iquote ../userprog -iquote ../threads -iquote ../machine -iquote ../lib \
 "$repo/tests/batch_lifecycle.cc" "${objects[@]}" \
 -Wl,--wrap=_ZN6Thread4ForkEPFvPvES0_,--wrap=_ZN6Thread6FinishEv,--wrap=_ZN9Interrupt4HaltEv \
 -o "$native/batch-test" > "$results/batch-unit-build.log" 2>&1
python3 "$repo/tests/batch_lifecycle.py" "$native/batch-test" "$results/batch-unit"
g++ -m32 -std=gnu++11 -g -Wall -DFILESYS_STUB -DRDATA -DSIM_FIX -Dx86 -DLINUX -DCHANGED \
 -iquote ../network -iquote ../filesys -iquote ../userprog -iquote ../threads -iquote ../machine -iquote ../lib \
 "$repo/tests/memory.cc" "${objects[@]}" -Wl,--wrap=read -o "$native/memory-test" > "$results/memory-unit-build.log" 2>&1
python3 "$repo/tests/memory_fixtures.py" "$native/fixtures"
python3 "$repo/tests/memory.py" "$native/memory-test" "$native/fixtures" "$results/memory"
g++ -m32 -std=gnu++11 -g -Wall -DFILESYS_STUB -DRDATA -DSIM_FIX -Dx86 -DLINUX -DCHANGED \
 -iquote ../network -iquote ../filesys -iquote ../userprog -iquote ../threads -iquote ../machine -iquote ../lib \
 "$repo/tests/user_memory.cc" "${objects[@]}" \
 -Wl,--wrap=open,--wrap=read,--wrap=write,--wrap=_ZN9Interrupt4HaltEv -o "$native/user-memory-test" > "$results/user-memory-build.log" 2>&1
python3 "$repo/tests/user_memory.py" "$native/user-memory-test" "$native" "$results/user-memory"



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
for name,args,timeout in [('single_no_trace',['-e','hw3t3'],4),('single_trace',['-d','z','-e','hw3t3'],4),('pair',['-e','hw3t1','-e','hw3t2'],4),('pair_reverse',['-e','hw3t2','-e','hw3t1'],4),('pair_trace',['-d','z','-e','hw3t1','-e','hw3t2'],4),('pair_reverse_trace',['-d','z','-e','hw3t2','-e','hw3t1'],4)]:
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

python3 "$repo/tests/scheduling_trace.py" "$results"

python3 "$repo/tests/shutdown.py" "$native/code/build.linux/nachos" "$results/shutdown"

python3 "$repo/tests/shutdown_trace.py" "$results"

# Genuine MIPS pressure workloads; keep all generated files in the native export.
for name in pressure_user big_exit_user oversized_user; do
 cp "$repo/tests/$name.c" "$name.c"
 "$native/usr/local/nachos/bin/decstation-ultrix-gcc" $flags -c "$name.c" -o "$name.o"
 "$native/usr/local/nachos/bin/decstation-ultrix-ld" -T script -N start.o "$name.o" -o "$name.coff"
 "$native/coff2noff/coff2noff.x86Linux" "$name.coff" "$name"
done > "$results/pressure-user-build.log" 2>&1
python3 "$repo/tests/memory_integration.py" "$native/code/build.linux/nachos" "$native/fixtures/truncated" "$results/pressure"

cp "$repo/tests/user_memory_user.c" user_memory_user.c
for identity in 1 2; do
 "$native/usr/local/nachos/bin/decstation-ultrix-gcc" $flags "-DTEST_ID=$identity" -c user_memory_user.c -o "user_memory_$identity.o"
 "$native/usr/local/nachos/bin/decstation-ultrix-ld" -T script -N start.o "user_memory_$identity.o" -o "user_memory_$identity.coff"
 "$native/coff2noff/coff2noff.x86Linux" "user_memory_$identity.coff" "user_memory_$identity"
done > "$results/user-memory-mips-build.log" 2>&1
python3 "$repo/tests/user_memory_integration.py" "$native/code/build.linux/nachos" "$results/user-memory-mips"
