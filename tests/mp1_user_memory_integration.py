#!/usr/bin/env python3
import argparse,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description='MP1 invalid-pointer recovery through actual MIPS syscalls')
p.add_argument('test_dir',type=Path);p.add_argument('results',type=Path)
a=p.parse_args();a.results.mkdir(parents=True,exist_ok=True)
try:
    r=subprocess.run([str((a.test_dir.parent/'build.linux/nachos').resolve()),'-e','user_memory_user'],
                     cwd=a.test_dir,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=8)
    code,out=r.returncode,r.stdout
except subprocess.TimeoutExpired as e:code,out=124,e.stdout or b''
text=out.decode(errors='replace');f=a.test_dir/'mp1-bounds.bin'
passed=(code==0 and '9902' in text and text.count('Machine halting!')==1
        and 'Assertion failed' not in text and f.exists()
        and f.read_bytes()==bytes(1+i%120 for i in range(300)))
(a.results/'recovery.log').write_bytes(out)
(a.results/'results.json').write_text(json.dumps([{'case':'mips_invalid_pointer_recovery','passed':passed,'returncode':code}],indent=2)+'\n')
print('MIPS boundary recovery: '+('PASS' if passed else 'FAIL'))
raise SystemExit(0 if passed else 1)
