#!/usr/bin/env python3
# Strengthen the scheduling runner's observations into shutdown regressions.
from pathlib import Path
import json,sys
out=Path(sys.argv[1]);rows=json.loads((out/'smoke-observations.json').read_text());checked=[]
assert len(rows)==6
for row in rows:
 data=(out/(row['case']+'.log')).read_bytes()
 passed=row['returncode']==0 and row['halted'] and not row['assertion'] and data.count(b'Machine halting!')==1
 checked.append({'case':row['case'],'passed':passed,'returncode':row['returncode']})
 assert passed,row
(out/'shutdown-trace-results.json').write_text(json.dumps(checked,indent=2)+'\n')
print('Six scheduling/trace runs: normal exit and exactly one halt PASS')
