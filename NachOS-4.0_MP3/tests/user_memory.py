#!/usr/bin/env python3
from pathlib import Path
import json,subprocess,sys,tempfile,struct
binary,root,out=sys.argv[1:];root=Path(root);out=Path(out);out.mkdir(exist_ok=False)
image=root/'user-memory.noff';image.write_bytes(struct.pack('<13i',0xbadfad,0,52,32,0,0,0,0,0,0,128,0,1024)+bytes(32))
cases=['filename_cross_page','filename_max','open_cross_page','negative_name','name_at_limit','far_name','empty_name','unterminated_name','name_too_long','name_invalid_page','name_negative_frame','name_upper_frame','missing_space','missing_table','empty_table','oversized_table','write_cross_page','write_readonly_source','read_cross_page','eof_tail','invalid_later_write','invalid_later_read','readonly_read','negative_count','oversized_count','huge_count','buffer_at_limit','buffer_tail','far_buffer','negative_buffer_frame','upper_buffer_frame','zero_read','zero_write','invalid_descriptor','pc_delay_slot','message_negative','message_unterminated','message_cross_page','message_last_zero']
rows=[]
for case in cases:
 cwd=Path(tempfile.mkdtemp(prefix='syscall-',dir=root))
 try:
  r=subprocess.run([binary,case,str(image.resolve())],cwd=cwd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=6);code,data=r.returncode,r.stdout
 except subprocess.TimeoutExpired as e:code,data=124,e.stdout or b''
 (out/(case+'.log')).write_bytes(data);passed=code==0 and data.rstrip().endswith(b'PASS')
 rows.append({'case':case,'passed':passed,'returncode':code});print(case+': '+('PASS' if passed else 'FAIL'),flush=True)
(out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
raise SystemExit(0 if all(row['passed'] for row in rows) else 1)
