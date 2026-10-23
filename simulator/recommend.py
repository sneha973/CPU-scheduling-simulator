from statistics import mean, pstdev
from .classifier import predict
FEATURES = ['process_count','burst_mean','burst_std','burst_cv','arrival_density','priority_mean','priority_spread','quantum_burst_ratio']
def features(workload, quantum):
    bursts=[p['burst'] for p in workload]; arrivals=[p['arrival'] for p in workload]; priorities=[p['priority'] for p in workload]
    avg=mean(bursts); span=max(arrivals)-min(arrivals)+avg
    return [len(workload),avg,pstdev(bursts),pstdev(bursts)/avg,len(workload)/span,mean(priorities),max(priorities)-min(priorities),quantum/avg]
def recommend(results, workload, quantum):
    names=list(results); waits=[results[n]['aggregate']['average_waiting_time'] for n in names]; responses=[results[n]['aggregate']['average_response_time'] for n in names]
    wmax=max(waits) or 1; rmax=max(responses) or 1
    scores={n:0.5*results[n]['aggregate']['average_waiting_time']/wmax+0.5*results[n]['aggregate']['average_response_time']/rmax for n in names}
    winner=min(names,key=lambda n:(scores[n],names.index(n)))
    vector=features(workload,quantum); model=predict(vector)
    if model:
        winner,report=model
        status=f"Decision tree trained on {report['samples']} synthetic workloads; held-out accuracy {report['accuracy']:.1%}"
    else:
        status='Model not trained yet; showing the direct simulator-score winner'
    return {'algorithm':winner,'objective':'Lowest equal-weighted normalized average waiting time and response time','score':scores[winner],'scores':scores,'features':dict(zip(FEATURES,vector)),'model_status':status}
