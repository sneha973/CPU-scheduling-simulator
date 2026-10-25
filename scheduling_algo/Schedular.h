#pragma once
#include <iostream>
#include <vector>
#include "Process.h"
#include <iomanip>
#include "FCFS.h"
#include "SJF.h" 
using namespace std;

class Schedular
{

public:
    static void run(vector<Process> &listOfProcess)
    {
        FCFS fcfs; 
        vector<Process> fcfs_res = fcfs.algoRun(listOfProcess);
        printTable(fcfs_res , fcfs.getWT() , fcfs.getTAT() , fcfs.getCT());

        SJF sjf;
        vector<Process> sjf_res = sjf.algoRun(listOfProcess);
        printTable(sjf_res ,sjf.getWT() , sjf.getTAT() , sjf.getCT());
    }

    static void printTable(const vector<Process> &,
                           const double& , const double& , const double& );
};

void Schedular::printTable(const vector<Process> &p, const double& avg_WT , const double& avg_TAT, const double& avg_CT)
{
    int n = p.size();

    cout << "\n";
    cout << "+---------+----+----+-------+----+-----+----+\n";

    cout << "| " << left << setw(8) << "Process"
         << " | " << setw(2) << "AT"
         << " | " << setw(2) << "BT"
         << " | " << setw(5) << "Start"
         << " | " << setw(2) << "CT"
         << " | " << setw(3) << "TAT"
         << " | " << setw(2) << "WT"
         << " |\n";

    cout << "+---------+----+----+-------+----+-----+----+\n";

    for (int i = 0; i < n; i++)
    {
        cout << "| " << left << setw(8) << p[i].Process_name
             << " | " << setw(2) << p[i].arrival_time
             << " | " << setw(2) << p[i].burst_time
             << " | " << setw(5) << p[i].start_time
             << " | " << setw(2) << p[i].completion_time
             << " | " << setw(3) << p[i].turnAround_time
             << " | " << setw(2) << p[i].waiting_time
             << " |\n";
    }

    cout << "+---------+----+----+-------+----+-----+----+\n";

    cout << " Avg Waiting Time = " << avg_WT << endl;
    cout << " Avg TurnARround Time = " << avg_TAT << endl;
    cout << " Avg Completion Time = " << avg_CT << endl;
}
