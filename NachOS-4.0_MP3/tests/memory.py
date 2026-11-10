#!/usr/bin/env python3
from pathlib import Path
import subprocess,json,sys
binary,fixtures,out=sys.argv[1:];out=Path(out);out.mkdir(exist_ok=False)
cases=['valid_control']+['layout_'+s for s in ['short','exact','two_pages','unaligned','rdata','shared','gapped','swapped']]+['reject_'+s for s in ['short_header','bad_magic','negative_size','negative_address','bad_offset','integer_end','truncated','overlap','oversized']]+['full_capacity','zero_available','inconsistent_counter','partial_short_read','repeated_failure','resident_isolation','retry_same_object','already_loaded','invalid_translation']
rows=[]
for case in cases:
 try:
  r=subprocess.run([binary,case,fixtures],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=6);code,data=r.returncode,r.stdout
 except subprocess.TimeoutExpired as e:code,data=124,e.stdout or b''
 (out/(case+'.log')).write_bytes(data);passed=code==0 and data.rstrip().endswith(b'PASS')
 rows.append({'case':case,'passed':passed,'returncode':code});print(case+': '+('PASS' if passed else 'FAIL'),flush=True)
(out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
raise SystemExit(0 if all(row['passed'] for row in rows) else 1)
