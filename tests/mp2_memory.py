#!/usr/bin/env python3
"""Synthetic NOFF fixtures checked by the actual AddrSpace loader."""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import tempfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('binary', type=Path)
p.add_argument('results', type=Path)
a = p.parse_args()
a.results.mkdir(parents=True, exist_ok=True)
page = 128
# (code start/size, initialized data start/size, readonly start/size, BSS start/size)
cases = {
    'one_full_code_page': (0,128, 128,0, 128,0, 128,0),
    'two_full_code_pages': (0,256, 256,0, 256,0, 256,0),
    'code_tail': (0,259, 259,0, 259,0, 259,0),
    'absent_segment_metadata': (0,129, 129,0, 129,0, 129,0),
    'unaligned_code': (7,270, 277,0, 277,0, 277,0),
    'data_cross_page': (0,141, 141,300, 441,0, 441,0),
    'readonly_cross_page': (0,128, 128,0, 128,270, 398,0),
    'readonly_unaligned_mixed': (0,137, 137,11, 148,270, 418,0),
    'combined_bss_and_gaps': (0,133, 160,273, 512,267, 800,145),
}
rows = []
def run(name, args):
    try:
        # The real kernel binds a Unix socket; run on Linux-native storage.
        with tempfile.TemporaryDirectory(prefix='nachos-memory-case-') as cwd:
            r = subprocess.run([str(a.binary.resolve()), *args], cwd=cwd,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=5)
        code, output = r.returncode, r.stdout
    except subprocess.TimeoutExpired as e:
        code, output = 124, e.stdout or b''
    (a.results / (name + '.log')).write_bytes(output)
    rows.append({'case':name, 'passed':code == 0, 'returncode':code})
    print(name + ': ' + ('PASS' if code == 0 else 'FAIL'), flush=True)

run('initialization', ['init', 'unused'])
for name, spec in cases.items():
    segments = [spec[i:i+2] for i in range(0,8,2)]
    end = max(start+size for start,size in segments)
    image = bytearray(end)
    payload = bytearray()
    header = [0xbadfad]
    for index,(start,size) in enumerate(segments):
        # NOFF order is code, initData, readonlyData, uninitData.
        header += [start, 52+len(payload) if index != 3 else 0, size]
        if index != 3:
            data = bytes(1+((j*31+index*67)%254) for j in range(size))
            payload += data
            image[start:start+size] = data
    if name == 'absent_segment_metadata':
        header[4:7] = [0xfefefefe,0xfefefefe,0]
        header[7:10] = [0xfefefefe,0xfefefefe,0]
    if name == 'combined_bss_and_gaps':
        header[11] = 0xfefefefe  # BSS is zero-filled, never read from a file.
    prefix = (a.results / name).resolve()
    prefix.write_bytes(struct.pack('<13I', *header) + payload)
    Path(str(prefix)+'.expected').write_bytes(struct.pack('<I',len(image))+image)
    pages = (end+1024+page-1)//page
    flags = bytearray(pages)
    ro_start,ro_size = segments[2]
    writable = [segments[1], segments[3], (pages*page-1024,1024)]
    for vpn in range(pages):
        lo,hi = vpn*page,(vpn+1)*page
        touches_ro = ro_size > 0 and lo < ro_start+ro_size and hi > ro_start
        touches_write = any(size > 0 and lo < start+size and hi > start for start,size in writable)
        flags[vpn] = int(touches_ro and not touches_write)
    Path(str(prefix)+'.protection').write_bytes(flags)
    run(name, ['load',str(prefix)])
truncated = (a.results/'truncated_payload').resolve()
truncated.write_bytes(prefix.read_bytes()[:-1])
run('truncated_payload', ['reject', str(truncated)])
(a.results/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
raise SystemExit(0 if all(r['passed'] for r in rows) else 1)
