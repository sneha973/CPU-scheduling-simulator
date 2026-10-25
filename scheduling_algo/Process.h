#pragma once
#include<string>
using namespace std;

class Process
{

public:

  int arrival_time;
  int burst_time;
  string Process_name;
 
  int waiting_time = 0;
  int turnAround_time = 0;
  int completion_time = 0;
  int start_time = 0;

  public:

  Process(string p_name = "" , int at = 0 , int bt = 0): Process_name(p_name) , arrival_time(at) , burst_time(bt){}

 static bool compareByAT(Process& a , Process& b)
{
   if(a.arrival_time == b.arrival_time)
   return a.burst_time < b.burst_time;

   return a.arrival_time < b.arrival_time; // agr a ka at < hai b ke at se then a ko phele rakho 
}

};



