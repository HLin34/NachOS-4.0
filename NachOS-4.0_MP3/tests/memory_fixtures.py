#!/usr/bin/env python3
from pathlib import Path
import struct,sys,json
out=Path(sys.argv[1]);out.mkdir(exist_ok=False);P=128;MEM=128*P;STACK=1024;H=52
layouts={
 'short':([(0,5),(200,7),(400,9),(600,13)],[3]),
 'exact':([(0,128),(0,0),(0,0),(0,0)],[]),
 'two_pages':([(0,256),(0,0),(0,0),(0,0)],[]),
 'unaligned':([(3,300),(400,180),(700,200),(1100,50)],[5,6,7]),
 'rdata':([(0,70),(400,45),(128,256),(512,30)],[1,2]),
 'shared':([(0,120),(160,20),(120,40),(256,10)],[]),
 'gapped':([(0,20),(700,13),(300,9),(1200,5)],[2]),
 'small':([(0,40),(128,10),(0,0),(0,0)],[]),
 'full':([(0,0),(0,0),(0,0),(0,MEM-STACK)],[]),
 'oversized':([(0,20),(0,0),(0,0),(128,MEM)],[]),
}
def fixture(name,segments,ro,endian='<'):
 raw=bytearray(H);fields=[0xbadfad];image_end=max((v+n for v,n in segments if n),default=0)
 pages=(image_end+STACK+P-1)//P
 image=bytearray(pages*P) if pages<=128 else bytearray()
 for index,(v,n) in enumerate(segments):
  offset=len(raw)+17 if index!=3 and n else 0
  fields += [v,offset,n]
  if index!=3 and n:
   raw.extend(b'\xee'*17);payload=bytes((j*13+index*53+5)%256 for j in range(n));raw.extend(payload)
   if image:image[v:v+n]=payload
 raw[:H]=struct.pack(endian+'13i',*fields);(out/name).write_bytes(raw)
 if image:
  flags=bytes(1 if i in ro else 0 for i in range(pages))
  (out/(name+'.expected')).write_bytes(struct.pack('<I',pages)+image+flags)
 return raw
for name,(segments,ro) in layouts.items():fixture(name,segments,ro)
fixture('swapped',*layouts['short'],endian='>')
raw=bytearray((out/'small').read_bytes());(out/'short_header').write_bytes(raw[:8])
for name,field,value in [('bad_magic',0,123),('negative_size',3,-1),('negative_address',1,-1),('bad_offset',2,2),('integer_end',1,2147483637)]:
 bad=bytearray(raw);struct.pack_into('<i',bad,field*4,value);(out/name).write_bytes(bad)
bad=bytearray((out/'two_pages').read_bytes());(out/'truncated').write_bytes(bad[:-15])
segments=[(0,128),(64,8),(0,0),(0,0)];fixture('overlap',segments,[])
(out/'manifest.json').write_text(json.dumps({'page_size':P,'physical_pages':128,'header_size':H,'layouts':list(layouts)},indent=2)+'\n')
