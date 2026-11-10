#!/usr/bin/env python3
import argparse,json,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser(description='Stub file-resource and I/O failure checks')
p.add_argument('binary',type=Path);p.add_argument('results',type=Path)
a=p.parse_args();a.results.mkdir(parents=True,exist_ok=True)
cases=['full_table_no_leak','destructor_cleanup','slot_reuse','missing_open','failed_create',
       'invalid_id_and_double_close','invalid_sizes_null','close_failure','close_interrupted_no_retry',
       'read_error_offset','write_error_offset','short_read','short_write','read_eintr','write_eintr',
       'write_readonly','write_dev_full','length_failure','eof_offsets','random_access']
rows=[]
for case in cases:
    with tempfile.TemporaryDirectory(prefix='nachos-file-io-') as cwd:
        try:
            r=subprocess.run([str(a.binary.resolve()),case],cwd=cwd,stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT,timeout=5)
            code,out=r.returncode,r.stdout
        except subprocess.TimeoutExpired as e:code,out=124,e.stdout or b''
    passed=code==0 and out.rstrip().endswith(b'PASS')
    (a.results/(case+'.log')).write_bytes(out)
    rows.append({'case':case,'passed':passed,'returncode':code})
    print(case+': '+('PASS' if passed else 'FAIL'),flush=True)
(a.results/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
raise SystemExit(0 if all(r['passed'] for r in rows) else 1)
