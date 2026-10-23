import json
from django.test import TestCase
from .models import SimulationRun

class SchedulerWorkflowTests(TestCase):
    def payload(self):
        return {'quantum':2,'processes':[{'id':'P1','arrival':0,'burst':5,'priority':2},{'id':'P2','arrival':1,'burst':3,'priority':1}]}
    def test_all_algorithms_and_hand_checked_fcfs_metrics(self):
        response=self.client.post('/api/simulate/',data=json.dumps(self.payload()),content_type='application/json')
        self.assertEqual(response.status_code,200,response.content)
        data=response.json();self.assertEqual(set(data['results']),{'FCFS','SJF','RR','PRIORITY'})
        fcfs=data['results']['FCFS'];self.assertEqual([s['process_id'] for s in fcfs['timeline']],['P1','P2'])
        self.assertEqual(fcfs['process_metrics'][0],{'process_id':'P1','completion_time':5,'turnaround_time':5,'waiting_time':0,'response_time':0})
        self.assertEqual(fcfs['process_metrics'][1],{'process_id':'P2','completion_time':8,'turnaround_time':7,'waiting_time':4,'response_time':4})
        self.assertEqual(fcfs['aggregate']['average_waiting_time'],2)
        self.assertEqual(data['recommendation']['objective'],'Lowest equal-weighted normalized average waiting time and response time')
    def test_hand_checked_execution_order_for_each_policy(self):
        body={'quantum':2,'processes':[{'id':'P1','arrival':0,'burst':5,'priority':2},{'id':'P2','arrival':0,'burst':3,'priority':1}]}
        data=self.client.post('/api/simulate/',data=json.dumps(body),content_type='application/json').json()
        expected={'FCFS':['P1','P2'],'SJF':['P2','P1'],'PRIORITY':['P2','P1'],'RR':['P1','P2','P1','P2','P1']}
        for alg,order in expected.items():
            with self.subTest(algorithm=alg):
                segments=data['results'][alg]['timeline']
                self.assertEqual([s['process_id'] for s in segments],order)
                self.assertEqual(segments[0]['start'],0)
                self.assertEqual(segments[-1]['end'],8)
    def test_invalid_workloads_are_rejected(self):
        cases=[{'quantum':0,'processes':self.payload()['processes']},{'quantum':2,'processes':[{'id':'x','arrival':0,'burst':0}]},{'quantum':2,'processes':[{'id':'x','arrival':0,'burst':2},{'id':'x','arrival':1,'burst':2}]},{'quantum':2,'processes':[{'id':'x','arrival':-1,'burst':2}]}]
        for body in cases:
            with self.subTest(body=body):
                response=self.client.post('/api/simulate/',data=json.dumps(body),content_type='application/json')
                self.assertEqual(response.status_code,400)
    def test_idle_gap_and_round_robin_boundary(self):
        body={'quantum':1,'processes':[{'id':'late','arrival':5,'burst':1,'priority':0}]}
        response=self.client.post('/api/simulate/',data=json.dumps(body),content_type='application/json')
        self.assertEqual(response.status_code,200)
        result=response.json()['results']['FCFS'];self.assertEqual(result['timeline'][0],{'process_id':'IDLE','start':0,'end':5})
        self.assertEqual(result['aggregate']['cpu_utilization'],100/6)
    def test_history_is_persisted(self):
        self.client.post('/api/simulate/',data=json.dumps(self.payload()),content_type='application/json')
        self.assertEqual(SimulationRun.objects.count(),1)
        response=self.client.get('/api/history/');self.assertEqual(response.status_code,200);self.assertEqual(response.json()['runs'][0]['process_count'],2)
    def test_browser_page_provides_csrf_token_for_json_post(self):
        from django.test import Client
        browser=Client(enforce_csrf_checks=True)
        page=browser.get('/');self.assertEqual(page.status_code,200)
        token=browser.cookies['csrftoken'].value
        response=browser.post('/api/simulate/',data=json.dumps(self.payload()),content_type='application/json',HTTP_X_CSRFTOKEN=token)
        self.assertEqual(response.status_code,200,response.content)
