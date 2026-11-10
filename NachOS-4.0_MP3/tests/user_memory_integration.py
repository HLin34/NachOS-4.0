#!/usr/bin/env python3
from pathlib import Path
import json,re,subprocess,sys
binary,out=sys.argv[1:];out=Path(out);out.mkdir(exist_ok=False);rows=[]
for case,ids in [('single',[1]),('dual',[1,2]),('dual_reverse',[2,1])]:
 for name in ['syscall-one.bin','syscall-two.bin']:Path(name).unlink(missing_ok=True)
 args=sum((['-e','user_memory_'+str(i)] for i in ids),[])
 try:
  r=subprocess.run([binary,*args],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=8);code,data=r.returncode,r.stdout
 except subprocess.TimeoutExpired as e:code,data=124,e.stdout or b''
 (out/(case+'.log')).write_bytes(data);returns=[int(v) for v in re.findall(rb'return value:\s*(-?\d+)',data)]
 correct=True
 for i in ids:
  path=Path('syscall-one.bin' if i==1 else 'syscall-two.bin');expected=bytes((n*17+i)%97+32 for n in range(300))
  correct=correct and path.exists() and path.read_bytes()==expected
 passed=code==0 and sorted(returns)==sorted(60+i for i in ids) and data.count(b'Machine halting!')==1 and b'Assertion failed' not in data and correct
 rows.append({'case':case,'passed':passed,'returncode':code,'returns':returns,'files_correct':correct});print(case+': '+('PASS' if passed else 'FAIL'),flush=True)
(out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
raise SystemExit(0 if all(row['passed'] for row in rows) else 1)
