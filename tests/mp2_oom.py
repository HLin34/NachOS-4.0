#!/usr/bin/env python3
import argparse,json,struct,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser(description='Capacity boundaries and safe cleanup in the actual MP2 loader')
p.add_argument('binary',type=Path);p.add_argument('results',type=Path)
a=p.parse_args();a.results.mkdir(parents=True,exist_ok=True)
def fixture(pages):
    # Four code bytes plus BSS give the requested total including the 1024-byte stack.
    size=pages*128-1024
    f=(a.results/f'pages_{pages}.noff').resolve()
    f.write_bytes(struct.pack('<13I',0xbadfad,0,52,4,0,0,0,0,0,0,4,0,size-4)+b'ABCD')
    return str(f)
tiny,mid,over_half,full,oversized,big_image=[fixture(n) for n in (9,60,65,128,129,137)]
cases={'oversized':(oversized,tiny),'remaining_capacity':(over_half,tiny),
       'zero_available':(tiny,tiny),'exact_fit_reuse':(full,tiny),
       'two_residents_reuse':(mid,tiny),'repeated_rejection':(oversized,tiny),
       'image_exceeds_physical':(big_image,tiny),
       'allocation_rollback_guard':(tiny,tiny)}
rows=[]
for case,paths in cases.items():
    with tempfile.TemporaryDirectory(prefix='nachos-oom-') as cwd:
        try:
            r=subprocess.run([str(a.binary.resolve()),case,*paths],cwd=cwd,
                             stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=8)
            code,out=r.returncode,r.stdout
        except subprocess.TimeoutExpired as e:code,out=124,e.stdout or b''
    passed=code==0 and out.rstrip().endswith(b'PASS')
    (a.results/(case+'.log')).write_bytes(out)
    rows.append({'case':case,'passed':passed,'returncode':code})
    print(case+': '+('PASS' if passed else 'FAIL'),flush=True)
(a.results/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
raise SystemExit(0 if all(r['passed'] for r in rows) else 1)
