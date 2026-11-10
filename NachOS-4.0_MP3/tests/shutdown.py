#!/usr/bin/env python3
from pathlib import Path
import collections,json,re,subprocess,sys
binary,out=sys.argv[1:];out=Path(out);out.mkdir(exist_ok=False)
missing='__missing_mp3_executable__'
cases=[
 ('empty',[],[],0,None),
 ('single',['-e','hw3t3'],[3],0,None),
 ('pair',['-e','hw3t1','-e','hw3t2'],[1,2],0,None),
 ('reverse',['-e','hw3t2','-e','hw3t1'],[1,2],0,None),
 ('random7',['-rs','7','-e','hw3t1','-e','hw3t2'],[1,2],0,None),
 ('random23',['-rs','23','-e','hw3t2','-e','hw3t1'],[1,2],0,None),
 ('missing',['-e',missing],[],1,None),
 ('missing_then_valid',['-e',missing,'-e','hw3t3'],[3],1,None),
 ('valid_then_missing',['-e','hw3t3','-e',missing],[3],1,None),
 ('all_missing',['-e',missing,'-e',missing+'2'],[],2,None),
 ('capacity',sum((['-e','hw3t3'] for _ in range(9)),[]),[3]*9,0,None),
 ('missing_operand',['-e'],[],0,b'Missing executable after -e'),
 ('option_operand',['-e','-e','hw3t3'],[],0,b'Missing executable after -e'),
 ('too_many',sum((['-e','hw3t3'] for _ in range(10)),[]),[],0,b'At most 9 programs per batch'),
 ('console_file',['-co',str((out/'console-output.txt').resolve()),'-e','hw3t1','-e','hw3t2'],[1,2],0,None),
]
rows=[]
for name,args,expected,missing_count,parse_error in cases:
 try:
  r=subprocess.run([binary,*args],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=5);code,data=r.returncode,r.stdout
 except subprocess.TimeoutExpired as e:code,data=124,e.stdout or b''
 (out/(name+'.log')).write_bytes(data)
 returned=[int(v) for v in re.findall(rb'return value:\s*(-?\d+)',data)]
 checks={'exit':code==(1 if parse_error else 0),'returns':returned==expected,'single_halt':data.count(b'Machine halting!')==(0 if parse_error else 1),'no_assertion':b'Assertion failed' not in data,'missing_reports':data.count(b'Unable to open file')==missing_count}
 if parse_error:checks['parse_error']=parse_error in data
 if name=='console_file':checks['complete_console_output']=collections.Counter((out/'console-output.txt').read_bytes())==collections.Counter(b'1\n2\n')
 passed=all(checks.values());rows.append({'case':name,'passed':passed,'returncode':code,'returns':returned,'checks':checks});print(name+': '+('PASS' if passed else 'FAIL'),flush=True)
(out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
raise SystemExit(0 if all(row['passed'] for row in rows) else 1)
