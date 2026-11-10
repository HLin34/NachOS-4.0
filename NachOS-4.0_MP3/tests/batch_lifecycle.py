#!/usr/bin/env python3
from pathlib import Path
import subprocess,json,sys
binary,out=sys.argv[1:];out=Path(out);out.mkdir(exist_ok=False);rows=[]
for case in ['empty','early_launch','two_live','duplicate','foreign','capacity','standalone']:
 try:
  r=subprocess.run([binary,case],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=5);code,data=r.returncode,r.stdout
 except subprocess.TimeoutExpired as e:code,data=124,e.stdout or b''
 (out/(case+'.log')).write_bytes(data)
 passed=code==0 and data.rstrip().endswith(b'PASS');rows.append({'case':case,'passed':passed,'returncode':code});print(case+': '+('PASS' if passed else 'FAIL'),flush=True)
(out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
raise SystemExit(0 if all(row['passed'] for row in rows) else 1)
