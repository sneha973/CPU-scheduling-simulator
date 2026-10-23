"""Python reference implementation used only when the C++ executable is unavailable."""
from collections import deque

def simulate(workload, quantum):
    results={}
    for name in ('FCFS','SJF','RR','PRIORITY'):
        rem={p['id']:p['burst'] for p in workload}; first={}; completion={}; now=0; timeline=[]
        def append(pid,start,end):
            if start==end:return
            if timeline and timeline[-1]['process_id']==pid and timeline[-1]['end']==start:timeline[-1]['end']=end
            else:timeline.append({'process_id':pid,'start':start,'end':end})
        def run(p,length):
            nonlocal now
            if now<p['arrival']:append('IDLE',now,p['arrival']);now=p['arrival']
            first.setdefault(p['id'],now);append(p['id'],now,now+length);now+=length;rem[p['id']]-=length
            if rem[p['id']]==0:completion[p['id']]=now
        if name in ('FCFS','SJF','PRIORITY'):
            while len(completion)<len(workload):
                ready=[p for p in workload if rem[p['id']]>0 and p['arrival']<=now]
                if not ready:
                    next_arr=min(p['arrival'] for p in workload if rem[p['id']]>0);append('IDLE',now,next_arr);now=next_arr;continue
                if name=='FCFS':p=min(ready,key=lambda x:(x['arrival'],x['id']))
                elif name=='SJF':p=min(ready,key=lambda x:(x['burst'],x['arrival'],x['id']))
                else:p=min(ready,key=lambda x:(x['priority'],x['arrival'],x['id']))
                run(p,rem[p['id']])
        else:
            order=sorted(workload,key=lambda x:(x['arrival'],x['id'])); i=0; q=deque()
            while len(completion)<len(workload):
                if not q and i<len(order) and now<order[i]['arrival']:append('IDLE',now,order[i]['arrival']);now=order[i]['arrival']
                while i<len(order) and order[i]['arrival']<=now:q.append(order[i]);i+=1
                if not q:continue
                p=q.popleft();length=min(quantum,rem[p['id']]);first.setdefault(p['id'],now);append(p['id'],now,now+length);now+=length;rem[p['id']]-=length
                while i<len(order) and order[i]['arrival']<=now:q.append(order[i]);i+=1
                if rem[p['id']]:q.append(p)
                else:completion[p['id']]=now
        metrics=[]
        for p in workload:
            tat=completion[p['id']]-p['arrival'];metrics.append({'process_id':p['id'],'completion_time':completion[p['id']],'turnaround_time':tat,'waiting_time':tat-p['burst'],'response_time':first[p['id']]-p['arrival']})
        busy=sum(s['end']-s['start'] for s in timeline if s['process_id']!='IDLE'); switches=0;prev=None
        for s in timeline:
            if s['process_id']=='IDLE':prev=None
            else:
                if prev is not None and prev!=s['process_id']:switches+=1
                prev=s['process_id']
        n=len(workload);end=now
        results[name]={'timeline':timeline,'process_metrics':metrics,'aggregate':{'average_waiting_time':sum(m['waiting_time'] for m in metrics)/n,'average_turnaround_time':sum(m['turnaround_time'] for m in metrics)/n,'average_response_time':sum(m['response_time'] for m in metrics)/n,'throughput':n/end if end else 0,'context_switches':switches,'cpu_utilization':100*busy/end if end else 0}}
    return results
