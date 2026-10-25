#include<iostream>
#include "Schedular.h"
#include<vector>
#include<fstream>
#include<sstream>
#include<string>

using namespace std;

int main()
{
    fstream process_file;

    vector<Process> listOfProcess;
    
    process_file.open("Process info.csv" , ios::in);
    if(process_file.is_open())
    {
      string line;
      while(getline(process_file , line))
      {
         if(line.empty())
         continue;

         if(line[0] == '#')
         continue;

         stringstream ss(line);

         string p_name;
         string p_at;
         string p_bt;


         getline(ss , p_name , ',');
         getline(ss , p_at , ',');
         getline(ss , p_bt , ',');

         int at = stoi(p_at);
         int bt = stoi(p_bt);

         Process p(p_name , at , bt);
       
         listOfProcess.push_back(p);

      }

    }

    Schedular::run(listOfProcess);

    process_file.close();


}