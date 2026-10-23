import json
import re
from django.http import JsonResponse
from django.shortcuts import render
from django.views.decorators.http import require_GET, require_POST
from .engine import run_engine
from .models import SimulationRun
from .recommend import recommend

def home(request): return render(request, 'index.html')

def validate(payload):
    if not isinstance(payload,dict): raise ValueError('Request body must be a JSON object.')
    procs=payload.get('processes'); q=payload.get('quantum')
    if not isinstance(procs,list) or not 1<=len(procs)<=100: raise ValueError('Provide between 1 and 100 processes.')
    if isinstance(q,bool) or not isinstance(q,int) or q<1: raise ValueError('Quantum must be a positive integer.')
    ids=set(); out=[]
    for i,p in enumerate(procs):
        if not isinstance(p,dict): raise ValueError(f'Process {i+1} must be an object.')
        pid=p.get('id')
        if isinstance(pid,bool) or not isinstance(pid,(str,int)) or not str(pid).strip() or len(str(pid))>24: raise ValueError(f'Process {i+1} needs a short ID.')
        pid=str(pid).strip()
        if not re.fullmatch(r'[A-Za-z0-9_-]{1,24}',pid) or pid=='IDLE': raise ValueError(f'Process {i+1} ID may use letters, digits, underscore, and hyphen; IDLE is reserved.')
        if pid in ids: raise ValueError(f'Duplicate process ID: {pid}.')
        ids.add(pid); item={'id':pid}
        for key,minimum in [('arrival',0),('burst',1),('priority',0)]:
            value=p.get(key,0 if key=='priority' else None)
            if isinstance(value,bool) or not isinstance(value,int) or value<minimum or value>1000000: raise ValueError(f'{pid}: {key} must be an integer >= {minimum} and <= 1000000.')
            item[key]=value
        out.append(item)
    return out,q

@require_POST
def simulate(request):
    try:
        workload,q=validate(json.loads(request.body.decode('utf-8')))
        results=run_engine(workload,q); rec=recommend(results,workload,q)
        run=SimulationRun.objects.create(workload=workload,quantum=q,results=results,recommendation=rec)
        return JsonResponse({'id':run.id,'created_at':run.created_at.isoformat(),'workload':workload,'quantum':q,'results':results,'recommendation':rec})
    except (ValueError,UnicodeDecodeError,json.JSONDecodeError) as exc: return JsonResponse({'error':str(exc)},status=400)
    except RuntimeError as exc: return JsonResponse({'error':str(exc)},status=503)

@require_GET
def history(request):
    rows=SimulationRun.objects.order_by('-created_at')[:25]
    return JsonResponse({'runs':[{'id':r.id,'created_at':r.created_at.isoformat(),'process_count':len(r.workload),'recommendation':r.recommendation['algorithm']} for r in rows]})
