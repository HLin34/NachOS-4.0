#!/usr/bin/env python3
from pathlib import Path
import json,subprocess,sys
binary,out=sys.argv[1:];out=Path(out);out.mkdir(exist_ok=False)
cases=['initial','first_burst','last_slice','zero_burst','kernel_and_no_double_count','ready_time_excluded','clamped_fifo','live_remaining','shorter_timer','longer_timer','equal_timer','zero_timer','empty_timer','idle_timer','timer_recheck','wake_defers','voluntary_yield','settle_before_insert','multiple_preemptions','negative_remaining_timer','fractional_estimate']
rows=[]
for case in cases:
 try:
  r=subprocess.run([binary,case],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=5);code,data=r.returncode,r.stdout
 except subprocess.TimeoutExpired as e:code,data=124,e.stdout or b''
 (out/(case+'.log')).write_bytes(data)
 passed=code==0 and data.rstrip().endswith(b'PASS');rows.append({'case':case,'passed':passed,'returncode':code});print(case+': '+('PASS' if passed else 'FAIL'),flush=True)
(out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
raise SystemExit(0 if all(row['passed'] for row in rows) else 1)
