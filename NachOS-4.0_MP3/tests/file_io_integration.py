#!/usr/bin/env python3
from pathlib import Path
import json, re, subprocess, sys
binary,out=sys.argv[1:];out=Path(out);out.mkdir(exist_ok=False)
rows=[]
for name,programs,expected in [('single_capacity',['file_io_0'],[70]),('nine_reuse',['file_io_0']*9,[70]*9),('dual',['file_io_1','file_io_2'],[71,72]),('dual_reverse',['file_io_2','file_io_1'],[71,72])]:
 for filename in ['io-capacity.bin','io-one.bin','io-two.bin']:
  Path(filename).unlink(missing_ok=True)
 args=[binary]
 for program in programs:args+=['-e',program]
 try:
  r=subprocess.run(args,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=12);code,data=r.returncode,r.stdout
 except subprocess.TimeoutExpired as e:code,data=124,e.stdout or b''
 (out/(name+'.log')).write_bytes(data)
 returns=[int(v) for v in re.findall(rb'return value:\s*(-?\d+)',data)]
 files={'io-capacity.bin':b'A'} if 'capacity' in name or name=='nine_reuse' else {'io-one.bin':b'B','io-two.bin':b'C'}
 contents={p:Path(p).read_bytes().hex() if Path(p).exists() else None for p in files}
 passed=code==0 and sorted(returns)==sorted(expected) and data.count(b'Machine halting!')==1 and b'Assertion failed' not in data and all(contents[p]==v.hex() for p,v in files.items())
 row={'case':name,'passed':passed,'returncode':code,'returns':returns,'files_hex':contents};rows.append(row);print(json.dumps(row),flush=True)
(out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
raise SystemExit(0 if all(r['passed'] for r in rows) else 1)
