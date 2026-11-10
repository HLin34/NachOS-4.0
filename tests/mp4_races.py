#!/usr/bin/env python3
import argparse,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description='ThreadSanitizer checks; startup failure is not a clean run')
p.add_argument('binary',type=Path);p.add_argument('results',type=Path)
p.add_argument('--cancel-diagnostic',action='store_true',help='Also retain the known sleep-interceptor cancellation diagnostic')
a=p.parse_args();a.results.mkdir(parents=True,exist_ok=True)
rows=[]
for case in ['turn_race','sanitizer_workload']+(['stress'] if a.cancel_diagnostic else []):
    for attempt in range(3):
        env=os.environ.copy();env['TSAN_OPTIONS']='halt_on_error=1:exitcode=66'
        try:
            r=subprocess.run([str(a.binary.resolve()),case],stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT,timeout=15,env=env)
            code,out=r.returncode,r.stdout
        except subprocess.TimeoutExpired as e:code,out=124,e.stdout or b''
        (a.results/f'{case}-{attempt}.log').write_bytes(out)
        startup=(b'FATAL: ThreadSanitizer: unexpected memory mapping' in out or
                 b'CHECK failed: tsan_platform_linux.cpp' in out or b'personality' in out and b'FATAL: ThreadSanitizer' in out)
        if not startup:break
    status='unavailable' if startup else 'passed' if code==0 and out.rstrip().endswith(b'PASS') else 'failed'
    rows.append({'case':case,'status':status,'returncode':code})
    print('TSan '+case+': '+status,flush=True)
(a.results/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
raise SystemExit(0 if all(r['status']=='passed' for r in rows) else 2 if all(r['status']=='unavailable' for r in rows) else 1)
