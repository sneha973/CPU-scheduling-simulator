#pragma once
#include <vector>
#include "Process.h"
using namespace std;

class SJF
{
    double avg_WT = 0.0;
    double avg_TAT = 0.0;
    double avg_CT = 0.0;

public:
    vector<Process> algoRun(vector<Process> P);

    double getWT() const;
    double getTAT() const;
    double getCT() const;
};