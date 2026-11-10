#!/usr/bin/env python3
from pathlib import Path
import json,re,subprocess,sys
binary,truncated,out=sys.argv[1:];out=Path(out);out.mkdir(exist_ok=False)
cases=[('oversized_alone',['oversized_user'],[],1,0),('oversized_then_small',['oversized_user','hw3t3'],[3],1,0),('small_then_oversized',['hw3t3','oversized_user'],[3],1,0),('resident_survives',['pressure_user','hw3t3'],[77],1,0),('small_then_resident',['hw3t3','pressure_user'],[3,77],0,0),('two_large',['pressure_user','pressure_user'],[77],1,0),('reclaim_nine_large',['big_exit_user']*9,[78]*9,0,0),('malformed_then_valid',[truncated,'hw3t3'],[3],0,1)]
rows=[]
for name,programs,expected,shortages,invalid in cases:
 args=sum((['-e',program] for program in programs),[])
 try:
  r=subprocess.run([binary,*args],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=6);code,data=r.returncode,r.stdout
 except subprocess.TimeoutExpired as e:code,data=124,e.stdout or b''
 (out/(name+'.log')).write_bytes(data);returns=[int(v) for v in re.findall(rb'return value:\s*(-?\d+)',data)]
 checks={'exit':code==0,'returns':returns==expected,'one_halt':data.count(b'Machine halting!')==1,'no_assertion':b'Assertion failed' not in data,'shortages':data.count(b'Insufficient memory to load')==shortages,'invalid':data.count(b'Invalid executable')==invalid}
 passed=all(checks.values());rows.append({'case':name,'passed':passed,'returncode':code,'returns':returns,'checks':checks});print(name+': '+('PASS' if passed else 'FAIL'),flush=True)
(out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
raise SystemExit(0 if all(row['passed'] for row in rows) else 1)
