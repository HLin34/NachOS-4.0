#!/usr/bin/env python3
import argparse,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description='Run two actual MIPS file-I/O programs together')
p.add_argument('test_dir',type=Path);p.add_argument('results',type=Path)
a=p.parse_args();a.results.mkdir(parents=True,exist_ok=True)
rows=[]
for name,seed in [('default',None),('seed_7',7),('seed_23',23)]:
    for identity in (1,2):
        (a.test_dir/f'syscall{identity}.bin').unlink(missing_ok=True)
    args=[str((a.test_dir.parent/'build.linux/nachos').resolve())]
    if seed is not None:args+=['-rs',str(seed)]
    args+=['-e','syscall_a','-e','syscall_b']
    try:
        r=subprocess.run(args,cwd=a.test_dir,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=8)
        code,out=r.returncode,r.stdout
    except subprocess.TimeoutExpired as e:code,out=124,e.stdout or b''
    text=out.decode(errors='replace')
    files_correct=True
    for identity in (1,2):
        f=a.test_dir/f'syscall{identity}.bin'
        expected=bytes(1+(i+identity)%120 for i in range(300))
        files_correct &= f.exists() and f.read_bytes()==expected
    passed=code==0 and text.count('return value:0')==2 and text.count('Machine halting!')==1 and files_correct
    (a.results/(name+'.log')).write_bytes(out)
    rows.append({'case':name,'passed':passed,'returncode':code,'files_correct':files_correct})
    print('MIPS '+name+': '+('PASS' if passed else 'FAIL'),flush=True)
(a.results/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
raise SystemExit(0 if all(r['passed'] for r in rows) else 1)
