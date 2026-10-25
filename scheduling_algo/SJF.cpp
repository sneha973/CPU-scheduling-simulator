#include<vector>
#include<queue>
#include "SJF.h"
#include <algorithm>

using namespace std;

struct Compare
{
    bool operator()(Process a , Process b)
    {
        if(a.burst_time == b.burst_time)
        return a.arrival_time > b.arrival_time;

        return a.burst_time > b.burst_time;
    }

};

   vector<Process> SJF::algoRun(vector<Process> P)
   {
    int n = P.size();

    vector<bool> visited(n , false);
    priority_queue<Process , vector<Process> ,Compare > pq;
    vector<Process> sjf(n);

         int time = 0;
         int j = 0;
         int indx = 0;

         sort(P.begin() , P.end() , Process::compareByAT);

    while(j < n)
    {
        while (indx < n && time >= P[indx].arrival_time)
        {
            if(visited[indx] != true){
            pq.push(P[indx]);
            visited[indx] = true;    
        }
            indx++;
        }

        if(pq.empty())
        {
            time = P[indx].arrival_time;
            continue;
        }

        Process currP = pq.top();
        pq.pop();
        currP.start_time = time;

        currP.completion_time = currP.start_time + currP.burst_time;
        currP.turnAround_time = currP.completion_time - currP.arrival_time;
        currP.waiting_time = currP.turnAround_time - currP.burst_time;
        
        
        sjf[j] = currP;
        j++;
        time = currP.completion_time;
    }

      double cal_WT = 0.0;
      double cal_TAT = 0.0;
      double cal_CT = 0.0;

      for(int i = 0 ; i < n ; i++)
      {
         cal_WT = cal_WT + sjf[i].waiting_time;
         cal_TAT = cal_TAT + sjf[i].turnAround_time ;
         cal_CT = cal_CT + sjf[i].completion_time;
         
      }

      avg_WT = cal_WT/n;
      avg_TAT = cal_TAT/n;
      avg_CT = cal_CT/n;

      return sjf;
   }
 

double SJF::getWT() const
{
   return avg_WT;
}

double SJF::getTAT() const
{
   return avg_TAT;
}

double SJF::getCT() const
{
   return avg_CT;
}




