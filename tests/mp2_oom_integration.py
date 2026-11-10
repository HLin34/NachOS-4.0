#!/usr/bin/env python3
import argparse,json,struct,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description='A rejected MP2 load must not stop other user programs')
p.add_argument('test_dir',type=Path);p.add_argument('results',type=Path)
a=p.parse_args();a.results.mkdir(parents=True,exist_ok=True)
base=(a.test_dir/'consoleIO_test1').read_bytes()
original=list(struct.unpack('<13I',base[:52]))
for name,pages in [('full_memory',128),('oversized',129)]:
    h=original.copy()
    end=max(h[i]+h[i+2] for i in (1,4,7,10) if h[i+2]>0)
    target=pages*128-1024
    if end>target:raise RuntimeError('console fixture unexpectedly too large')
    h[10:13]=[end,0,target-end]
    (a.test_dir/name).write_bytes(struct.pack('<13I',*h)+base[52:])
cases={
    'oversized_alone':(['-e','oversized'],0),
    'oversized_then_survivor':(['-e','oversized','-e','consoleIO_test2'],1),
    'survivor_then_oversized':(['-rs','7','-e','consoleIO_test2','-e','oversized'],1),
    'full_then_small':(['-e','full_memory','-e','consoleIO_test2'],1),
    'small_then_full':(['-e','consoleIO_test2','-e','full_memory'],1),
}
rows=[]
for name,(args,completions) in cases.items():
    try:
        r=subprocess.run([str((a.test_dir.parent/'build.linux/nachos').resolve()),*args],
                         cwd=a.test_dir,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=8)
        code,out=r.returncode,r.stdout
    except subprocess.TimeoutExpired as e:code,out=124,e.stdout or b''
    text=out.decode(errors='replace')
    passed=(code==0 and text.count('return value:0')==completions
            and text.count('Insufficient physical memory for user program ')==1
            and text.count('Machine halting!')==1 and 'Assertion failed' not in text)
    (a.results/(name+'.log')).write_bytes(out)
    rows.append({'case':name,'passed':passed,'returncode':code})
    print('batch '+name+': '+('PASS' if passed else 'FAIL'),flush=True)
(a.results/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
raise SystemExit(0 if all(r['passed'] for r in rows) else 1)
