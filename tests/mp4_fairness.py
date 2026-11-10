#!/usr/bin/env python3
import argparse,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('binary',type=Path);p.add_argument('results',type=Path);p.add_argument('--tsan',action='store_true');a=p.parse_args()
a.results.mkdir(parents=True,exist_ok=True)
cases=[('fifo',5),('spurious',5),('parallel',5),('progress',2),('progress',3),('progress',5),('progress',7),('invalid_id',5)]
if not a.tsan:cases += [(s,5) for s in ['cancel_head','cancel_middle','cancel_tail','cancel_all','cancel_grant']]
rows=[]
for case,n in cases:
 for attempt in range(3 if a.tsan else 1):
  env=os.environ.copy();env['TSAN_OPTIONS']='halt_on_error=1:exitcode=66'
  try:
   r=subprocess.run([str(a.binary.resolve()),case,str(n)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=20,env=env);code,data=r.returncode,r.stdout
  except subprocess.TimeoutExpired as e:code,data=124,e.stdout or b''
  (a.results/f'{case}-n{n}-{attempt}.log').write_bytes(data)
  unavailable=a.tsan and (b'FATAL: ThreadSanitizer: unexpected memory mapping' in data or b'CHECK failed: tsan_platform_linux.cpp' in data)
  if not unavailable:break
 passed=code==0 and data.rstrip().endswith(b'PASS')
 rows.append({'case':case,'participants':n,'passed':passed,'unavailable':unavailable,'returncode':code})
 print(f'{case} n={n}: '+('PASS' if passed else 'UNAVAILABLE' if unavailable else 'FAIL'),flush=True)
(a.results/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
raise SystemExit(0 if all(row['passed'] for row in rows) else 1)
