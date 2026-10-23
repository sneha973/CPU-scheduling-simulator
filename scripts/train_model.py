"""Generate a reproducible C++-labelled workload set and train a small CART tree."""
import csv, json, random, subprocess
from collections import Counter
from pathlib import Path
from statistics import mean, pstdev
ROOT=Path(__file__).resolve().parents[1]; ENGINE=ROOT/'build'/'scheduler_engine.exe'; OUT=ROOT/'data'; FEATURES=['process_count','burst_mean','burst_std','burst_cv','arrival_density','priority_mean','priority_spread','quantum_burst_ratio']; ALGS=['FCFS','SJF','RR','PRIORITY']
import sys
sys.path.insert(0,str(ROOT))
from simulator.reference_engine import simulate as reference_simulate
def feats(ps,q):
    b=[p['burst'] for p in ps]; a=[p['arrival'] for p in ps]; pr=[p['priority'] for p in ps]; avg=mean(b); sd=pstdev(b); span=max(a)-min(a)+avg
    return [len(ps),avg,sd,sd/avg,len(ps)/span,mean(pr),max(pr)-min(pr),q/avg]
def label(results):
    w=[results[n]['aggregate']['average_waiting_time'] for n in ALGS]; r=[results[n]['aggregate']['average_response_time'] for n in ALGS]; wm=max(w) or 1; rm=max(r) or 1
    return min(ALGS,key=lambda n:(.5*results[n]['aggregate']['average_waiting_time']/wm+.5*results[n]['aggregate']['average_response_time']/rm,ALGS.index(n)))
def gini(y):
    c=Counter(y); n=len(y); return 1-sum((v/n)**2 for v in c.values())
def tree(xs,ys,depth=0,max_depth=7):
    counts=Counter(ys); pred=counts.most_common(1)[0][0]
    if depth>=max_depth or len(counts)==1 or len(ys)<12:return {'class':pred}
    base=gini(ys); best=None
    for f in range(len(FEATURES)):
        vals=sorted(set(x[f] for x in xs))
        for lo,hi in zip(vals,vals[1:]):
            t=(lo+hi)/2; li=[i for i,x in enumerate(xs) if x[f]<=t]; ri=[i for i,x in enumerate(xs) if x[f]>t]
            if not li or not ri:continue
            gain=base-len(li)/len(ys)*gini([ys[i] for i in li])-len(ri)/len(ys)*gini([ys[i] for i in ri])
            if best is None or gain>best[0]:best=(gain,f,t,li,ri)
    if best is None or best[0]<0.002:return {'class':pred}
    _,f,t,li,ri=best
    return {'feature':f,'threshold':t,'left':tree([xs[i] for i in li],[ys[i] for i in li],depth+1,max_depth),'right':tree([xs[i] for i in ri],[ys[i] for i in ri],depth+1,max_depth),'class':pred}
def predict(node,x):
    while 'feature' in node: node=node['left'] if x[node['feature']]<=node['threshold'] else node['right']
    return node['class']
def main():
    rng=random.Random(20261023); rows=[]; count=2400
    for i in range(count):
        n=rng.randint(2,12); mode=i%4
        ps=[]
        for j in range(n):
            # Deliberately span burst shape, arrival clustering, priority, and RR quantum.
            if mode==0: burst=rng.randint(1,8)
            elif mode==1: burst=max(1,int(rng.expovariate(.16)))
            elif mode==2: burst=rng.choice([1,2,3,20,30,40])
            else: burst=rng.randint(3,30)
            arrival=rng.randint(0,4) if i%3 else rng.randint(0,30)
            ps.append({'id':f'P{j+1}','arrival':arrival,'burst':burst,'priority':rng.randint(0,5)})
        q=rng.choice([1,2,3,5,8,12,20,30])
        if ENGINE.exists():
            cp=subprocess.run([str(ENGINE)],input=json.dumps({'processes':ps,'quantum':q}),text=True,capture_output=True,timeout=5,check=True,cwd=ROOT);result=json.loads(cp.stdout)['results']
        else: result=reference_simulate(ps,q)
        rows.append((feats(ps,q),label(result)))
    OUT.mkdir(exist_ok=True); csvpath=OUT/'synthetic_workloads.csv'
    with csvpath.open('w',newline='',encoding='utf-8') as f:
        w=csv.writer(f);w.writerow(FEATURES+['label']);w.writerows([x+[y] for x,y in rows])
    by={c:[] for c in ALGS}
    for x,y in rows:by[y].append((x,y))
    train=[];test=[]
    for c,items in by.items():
        rng.shuffle(items); cut=max(1,int(.8*len(items)));train+=items[:cut];test+=items[cut:]
    model=tree([x for x,y in train],[y for x,y in train]); pred=[predict(model,x) for x,y in test]; truth=[y for x,y in test]; accuracy=sum(a==b for a,b in zip(pred,truth))/len(test)
    confusion={c:{d:0 for d in ALGS} for c in ALGS}
    for y,p in zip(truth,pred):confusion[y][p]+=1
    test_counts=Counter(truth)
    report={'seed':20261023,'samples':count,'label_engine':'C++17 engine' if ENGINE.exists() else 'Python reference fallback (C++ build unavailable)','features':FEATURES,'label_objective':'lowest equal-weighted normalized average waiting and response time','train_size':len(train),'test_size':len(test),'accuracy':accuracy,'per_class':{c:{'support':test_counts[c],'precision':confusion[c][c]/max(1,sum(confusion[r][c] for r in ALGS)),'recall':confusion[c][c]/max(1,sum(confusion[c].values()))} for c in ALGS},'confusion_matrix':confusion,'target_accuracy':0.8,'target_met':accuracy>=0.8,'class_counts':dict(Counter(y for x,y in rows))}
    (OUT/'model.json').write_text(json.dumps({'features':FEATURES,'tree':model,'report':report},indent=2),encoding='utf-8');(OUT/'evaluation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report,indent=2))
if __name__=='__main__':main()
