#!/usr/bin/env python3
from pathlib import Path
import json, subprocess, sys, tempfile
binary, root, out = sys.argv[1:]
out=Path(out);out.mkdir(exist_ok=False)
cases=['allocation_error','create_error','open_error','missing_parent','missing_file','open_eintr','create_eintr','create_close_error','capacity','cross_owner','owner_cleanup','owner_cleanup_error','finish_cleanup','destructor_cleanup','cleanup_close_error','close_error','close_eintr','double_close','invalid_ids','length_error','length_host_error','length_eintr','offset_limits','readat_length','read_error','write_error','read_seek_error','write_seek_error','read_eintr','write_eintr','read_seek_eintr','write_seek_eintr','short_read','short_write','zero_write']
rows=[]
for case in cases:
 cwd=tempfile.mkdtemp(prefix='file-io-',dir=root)
 try:
  r=subprocess.run([binary,case],cwd=cwd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=8);code,data=r.returncode,r.stdout
 except subprocess.TimeoutExpired as e:code,data=124,e.stdout or b''
 (out/(case+'.log')).write_bytes(data)
 passed=code==0 and data.rstrip().endswith(b'PASS')
 rows.append({'case':case,'passed':passed,'returncode':code});print(case+': '+('PASS' if passed else 'FAIL'),flush=True)
(out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
raise SystemExit(0 if all(r['passed'] for r in rows) else 1)
