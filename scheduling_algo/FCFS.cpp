#include "FCFS.h"
#include <algorithm>
using namespace std;




   vector<Process> FCFS::algoRun(vector<Process> P)
   {
      int n = P.size();
      
      if(n == 0)
      return {};

      sort(P.begin() , P.end() , Process::compareByAT);

      for(int i = 0 ; i < n ; i++)
      {
        if(i != 0 && P[i].arrival_time <= P[i-1].completion_time)
        P[i].start_time = P[i-1].completion_time;

        else
        P[i].start_time = P[i].arrival_time;

        P[i].completion_time = P[i].start_time + P[i].burst_time;
        P[i].turnAround_time = P[i].completion_time - P[i].arrival_time;
        P[i].waiting_time = P[i].turnAround_time - P[i].burst_time;


      }

      double cal_WT = 0.0;
      double cal_TAT = 0.0;
      double cal_CT = 0.0;

      for(int i = 0 ; i < n ; i++)
      {
         cal_WT = cal_WT + P[i].waiting_time;
         cal_TAT = cal_TAT + P[i].turnAround_time ;
         cal_CT = cal_CT + P[i].completion_time;
         
      }

      avg_WT = cal_WT/n;
      avg_TAT = cal_TAT/n;
      avg_CT = cal_CT/n;

      return P;

   }
   
double FCFS::getWT() const
{
   return avg_WT;
}

double FCFS::getTAT() const
{
   return avg_TAT;
}

double FCFS::getCT() const
{
   return avg_CT;
}
