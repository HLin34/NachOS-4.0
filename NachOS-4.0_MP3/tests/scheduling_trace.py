#!/usr/bin/env python3
# Independent trace accounting: reconstruct CPU intervals and ready ownership.
import json,re,sys,math
from pathlib import Path
out=Path(sys.argv[1]);observations=json.loads((out/'smoke-observations.json').read_text())
rows=[]
for row in observations:
 expected=[3] if row['case'].startswith('single') else [1,2]
 assert row['returns']==expected and not row['assertion'],row
 # Shutdown is a known, separately recorded failure, not a passing condition.
 assert row['returncode'] in (0,124),row
 if row['returncode']==0:assert row['halted'],row
 rows.append({'case':row['case'],'scheduling_passed':True,'shutdown_completed':row['returncode']==0 and row['halted']})
for case in ['pair_trace','pair_reverse_trace']:
 text=(out/(case+'.log')).read_text();ready=set();tau={};elapsed={};last_burst={};current=None;start=0;estimates=0;preemptions=0
 ids={1:1,2:2} if case=='pair_trace' else {1:2,2:1}
 def close(a,b):return math.isclose(a,b,rel_tol=3e-5,abs_tol=1e-6)
 def remaining(t):return max(0,tau.get(t,0)-elapsed.get(t,0))
 for token in re.finditer(r'\[([ABCDE])\] Tick \[(\d+)\]:[^\n]*|return value:\s*([12])',text):
  if token[3]:
   exiting=next(t for t,status in ids.items() if status==int(token[3]));assert current==exiting,(case,'exit owner',current,exiting)
   current=None;continue
  event,tick,line=token[1],int(token[2]),token[0]
  thread=int(re.search(r'Thread \[(\d+)\]',line)[1]);tau.setdefault(thread,0);elapsed.setdefault(thread,0)
  if event=='A':
   assert thread not in ready,(case,'duplicate admission',thread);ready.add(thread)
  elif event=='B':
   assert thread in ready,(case,'dequeue absent',thread)
   # All queued keys must be stable; the selected candidate has minimal
   # committed remaining time. FIFO ties are exercised by direct tests.
   assert all(remaining(thread)<=remaining(t)+1e-7 for t in ready),(case,'wrong queue head',line)
   ready.remove(thread)
   if current is None:current=thread;start=tick
  elif event=='C':
   old,burst,new=map(float,re.search(r'from: \[([^]]+)\], add \[([^]]+)\], to \[([^]]+)\]',line).groups())
   assert current==thread,(case,'blocked owner',current,thread)
   full=elapsed[thread]+tick-start
   assert close(burst,full) and close(old,tau[thread]),(case,'burst accounting',line,full,tau[thread])
   estimate=.5*tau[thread]+.5*full
   assert close(new,estimate),(case,'recurrence',line,estimate)
   tau[thread]=estimate;elapsed[thread]=0;last_burst[thread]=full;current=None;estimates+=1
  elif event=='D':
   previous=int(re.search(r'thread \[(\d+)\]',line)[1]);burst=float(re.search(r'executed \[([^]]+)\]',line)[1])
   assert close(burst,last_burst[previous]),(case,'D lost burst',line)
   current=thread;start=tick
  elif event=='E':
   previous=int(re.search(r'thread \[(\d+)\]',line)[1]);burst=float(re.search(r'executed \[([^]]+)\]',line)[1])
   assert current==previous,(case,'preempted owner',current,previous)
   full=elapsed[previous]+tick-start
   assert close(burst,full),(case,'E burst accounting',line,full)
   assert remaining(thread)<max(0,tau[previous]-full),(case,'unnecessary preemption',line,tau,elapsed)
   elapsed[previous]=full;current=thread;start=tick;preemptions+=1
 assert not ready and estimates>0,(case,ready)
 if case=='pair_reverse_trace':assert preemptions>0,'real timer preemption not exercised'
 rows.append({'case':case+'_accounting','passed':True,'estimate_updates_checked':estimates,'strictly_shorter_preemptions_checked':preemptions})
 print(f'{case}: CPU intervals, estimate updates and preemption choices PASS',flush=True)
(out/'scheduling-integration.json').write_text(json.dumps(rows,indent=2)+'\n')
