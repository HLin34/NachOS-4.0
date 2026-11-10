#!/usr/bin/env python3
import argparse,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description='MP4 race/cancellation lifecycle checks')
p.add_argument('binary',type=Path);p.add_argument('results',type=Path)
p.add_argument('--stress-only',action='store_true')
p.add_argument('--application',action='store_true')
a=p.parse_args();a.results.mkdir(parents=True,exist_ok=True)
if a.application:cases=['normal','create_failure_1','create_failure_3']
elif a.stress_only:cases=['stress']
else:cases=['cancel_thinking','cancel_table','cancel_left','cancel_right','cancel_eating','stress','turn_race','invalid_table']
rows=[]
for case in cases:
    env=os.environ.copy();args=[str(a.binary.resolve())]
    if a.application:
        if case.startswith('create_failure_'):env['MP4_FAIL_CREATE']=case.rsplit('_',1)[1]
    else:args.append(case)
    try:
        r=subprocess.run(args,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=6,env=env)
        code,out=r.returncode,r.stdout
    except subprocess.TimeoutExpired as e:code,out=124,e.stdout or b''
    if a.application:
        expected=1 if case.startswith('create_failure_') else 0
        passed=code==expected and b'All philosophers have stopped.' in out
    else:passed=code==0 and out.rstrip().endswith(b'PASS')
    (a.results/(case+'.log')).write_bytes(out)
    rows.append({'case':case,'passed':passed,'returncode':code})
    print(case+': '+('PASS' if passed else 'FAIL'),flush=True)
(a.results/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
raise SystemExit(0 if all(r['passed'] for r in rows) else 1)
