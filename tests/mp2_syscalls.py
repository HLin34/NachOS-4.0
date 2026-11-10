#!/usr/bin/env python3
import argparse,json,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser(description='Run the real MP2 syscall handler under fragmented mappings')
p.add_argument('binary',type=Path);p.add_argument('results',type=Path)
a=p.parse_args();a.results.mkdir(parents=True,exist_ok=True)
cases=['filename_cross_page','write_cross_page','read_cross_page','two_contexts','eof_tail',
       'bad_write_range','bad_read_range','readonly_read','invalid_page','negative_frame',
       'negative_size','negative_pointer','zero_size','invalid_id','unterminated_filename',
       'filename_unmapped_tail','invalid_message']
rows=[]
for case in cases:
    with tempfile.TemporaryDirectory(prefix='nachos-syscall-') as cwd:
        try:
            r=subprocess.run([str(a.binary.resolve()),case],cwd=cwd,stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT,timeout=5)
            code,out=r.returncode,r.stdout
        except subprocess.TimeoutExpired as e:code,out=124,e.stdout or b''
    (a.results/(case+'.log')).write_bytes(out)
    passed=code==0 and out.rstrip().endswith(b'PASS')
    rows.append({'case':case,'passed':passed,'returncode':code})
    print(case+': '+('PASS' if passed else 'FAIL'),flush=True)
(a.results/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
raise SystemExit(0 if all(r['passed'] for r in rows) else 1)
