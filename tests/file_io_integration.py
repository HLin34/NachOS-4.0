#!/usr/bin/env python3
import argparse,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description='Actual MIPS file-error recovery and bundled MP1 examples')
p.add_argument('test_dir',type=Path);p.add_argument('results',type=Path);p.add_argument('--mp',choices=['1','2'],required=True)
a=p.parse_args();a.results.mkdir(parents=True,exist_ok=True)
cases=[('recovery',['-e','file_io_user'])]
if a.mp=='1':cases.extend([('bundled_write',['-e','fileIO_test1']),('bundled_read',['-e','fileIO_test2'])])
rows=[]
for name,args in cases:
    try:
        r=subprocess.run([str((a.test_dir.parent/'build.linux/nachos').resolve()),*args],
                         cwd=a.test_dir,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=8)
        code,out=r.returncode,r.stdout
    except subprocess.TimeoutExpired as e:code,out=124,e.stdout or b''
    text=out.decode(errors='replace')
    passed=code==0 and text.count('Machine halting!')==1 and 'Assertion failed' not in text
    if name=='recovery':
        f=a.test_dir/'io-recovery.bin'
        completed=('9901' in text) if a.mp=='1' else text.count('return value:0')==1
        passed &= completed and f.exists() and f.read_bytes()==b'ABCD'
    if name=='bundled_read':passed &= 'Passed!' in text
    (a.results/(name+'.log')).write_bytes(out)
    rows.append({'case':name,'passed':bool(passed),'returncode':code})
    print('MP'+a.mp+' '+name+': '+('PASS' if passed else 'FAIL'),flush=True)
(a.results/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
raise SystemExit(0 if all(r['passed'] for r in rows) else 1)
