#include "process.h"
#include "ioModule.h"
#include "processMgmt.h"

#include <chrono> // for sleep
#include <thread> // for sleep

#include <queue>

// void transfer(list<Process> &processList, Process* runningPtr)  {
//   for(auto& process : processList)  {
//     if(process.id == runningPtr->id)  {
//       process = *runningPtr;
//       return;
//     }
//   }
// }

int main(int argc, char* argv[])
{
    // single thread processor
    // it's either processing something or it's not
//    bool processorAvailable = true;

    // vector of processes, processes will appear here when they are created by
    // the ProcessMgmt object (in other words, automatically at the appropriate time)
    list<Process> processList;
    
    // this will orchestrate process creation in our system, it will add processes to 
    // processList when they are created and ready to be run/managed
    ProcessManagement processMgmt(processList);

    // this is where interrupts will appear when the ioModule detects that an IO operation is complete
    list<IOInterrupt> interrupts;   

    // this manages io operations and will raise interrupts to signal io completion
    IOModule ioModule(interrupts);  

    // Do not touch
    long time = 1;
    long sleepDuration = 50;
    string file;
    stringstream ss;
    enum stepActionEnum {noAct, admitNewProc, handleInterrupt, beginRun, continueRun, ioRequest, complete, endLevel} stepAction;

    // Do not touch
    switch(argc)
    {
        case 1:
            file = "./procList.txt";  // default input file
            break;
        case 2:
            file = argv[1];         // file given from command line
            break;
        case 3:
            file = argv[1];         // file given
            ss.str(argv[2]);        // sleep duration given
            ss >> sleepDuration;
            break;
        default:
            cerr << "incorrect number of command line arguments" << endl;
            cout << "usage: " << argv[0] << " [file] [sleepDuration]" << endl;
            return 1;
            break;
    }

    processMgmt.readProcessFile(file);


    time = 0;
//    processorAvailable = true;
    Process* runningProcess = nullptr; // Current Running Process
    //list<Process*> readyList; // List of all ready processes in proper order
    queue<Process*> highQueue;
    queue<Process*> mediumQueue;
    queue<Process*> lowQueue;
    list<Process*> blockedList; // List of all blocked processes

    // const int totalMemory = 1024;
    int usedMemory = 0;
    int timeQuantum[] = {16, 32, 64};
    int memoryPartitions[] = {-1,-1,-1,-1}; // To be filled with process IDs (4 partitions of size 256bytes)
    int usedMemoryPartitions = 0;


    //keep running the loop until all processes have been added and have run to completion
    while(processMgmt.moreProcessesComing() || !highQueue.empty() ||!mediumQueue.empty() ||!lowQueue.empty() || !blockedList.empty() || runningProcess) /* TODO add something to keep going as long as there are processes that arent done! */ 
    {
        //Update our current time step
        ++time;

        //let new processes in if there are any
        processMgmt.activateProcesses(time);

        //update the status for any active IO requests
        ioModule.ioProcessing(time);

        //If the processor is tied up running a process, then continue running it until it is done or blocks
        //   note: be sure to check for things that should happen as the process continues to run (io, completion...)
        //If the processor is free then you can choose the appropriate action to take, the choices (in order of precedence) are:
        // - admit a new process if one is ready (i.e., take a 'newArrival' process and put them in the 'ready' state)
        // - address an interrupt if there are any pending (i.e., update the state of a blocked process whose IO operation is complete)
        // - start processing a ready process if there are any ready


        //init the stepAction, update below
        stepAction = noAct;

        
        //TODO add in the code to take an appropriate action for this time step!
        //you should set the action variable based on what you do this time step. you can just copy and paste the lines below and uncomment them, if you want.
        //stepAction = continueRun;  //runnning process is still running
        //stepAction = ioRequest;  //running process issued an io request
        //stepAction = complete;   //running process is finished
        //stepAction = admitNewProc;   //admit a new process into 'ready'
        //stepAction = handleInterrupt;   //handle an interrupt
        //stepAction = beginRun;   //start running a process
        

        //   <your code here> 
        if(runningProcess)  { // Is there a process currently running? ---Yes
          int quantum = timeQuantum[3 - runningProcess->level];
          runningProcess->processorTime++; // Update processor Time
          runningProcess->timeUsedThisQuantum++;
          if(!runningProcess->ioEvents.empty() && runningProcess->ioEvents.front().time == runningProcess->processorTime) {  // Does the running process have an I/O Event? ---Yes
            ioModule.submitIORequest(time, runningProcess->ioEvents.front(), *runningProcess);  // I/O Request
            runningProcess->ioEvents.pop_front();
            runningProcess->state = blocked; // Block Process
            blockedList.push_back(runningProcess); // Add to block list
            stepAction = ioRequest;
          } else if(runningProcess->processorTime >= runningProcess->reqProcessorTime) { // ---No--- Has the running process run long enough? ---Yes
            runningProcess->state = done;
            runningProcess->doneTime = time;
            stepAction = complete;
          } else if(runningProcess->timeUsedThisQuantum >= quantum) { // ---No--- Has the running process run long enough in the level but not done? ---Yes
            if(runningProcess->level > 1) {
              runningProcess->level--;
            }
            runningProcess->state = ready;
            switch(runningProcess->level) {
              case 2: mediumQueue.push(runningProcess); break;
              case 1: lowQueue.push(runningProcess); break;
            }
            stepAction = endLevel;
            runningProcess->timeUsedThisQuantum = 0;
            //transfer(processList, runningProcess);
            runningProcess = nullptr; // If end of level, keep memory allocated
          } else{ //--- No
            stepAction = continueRun;
          }
          if(stepAction == ioRequest || stepAction == complete) { // If process is blocked or done running completely then deallocate memory
            usedMemory -= runningProcess->memoryRequired;
            for(int i = 0; i < 4; i++)  { // Finds memory partition and removes it
              if(memoryPartitions[i] == int(runningProcess->id)) {
                memoryPartitions[i] = -1;
                usedMemoryPartitions--;
                break;
              } else if (i == 3)  { // Error, memory partition not found
                cout << "Error, memory partition not found" << endl;
              }
            }
            //transfer(processList, runningProcess);
            runningProcess = nullptr;
          }
        } else  { // ---No process running
          for(auto& process : processList)  { // Are there any new Arrivals? ---Yes
            if(process.state == newArrival) {
              if(usedMemoryPartitions < 4)  { // Is there memory available? ---Yes
                process.state = ready;
                highQueue.push(&process); // add to High queue
                usedMemory += process.memoryRequired; // Allocate memory
                usedMemoryPartitions++;
                for(int i = 0; i< 4; i++){
                  if(memoryPartitions[i] == -1){
                    memoryPartitions[i] = process.id;
                    break;
                  }
                } 
                stepAction = admitNewProc;
                break;
              } else { // Is there memory available? ---No
                highQueue.push(&process); // add to High queue
                process.state = memBlocked;
                stepAction = admitNewProc;
                break;
              }
            }
          } // If there is a new arrival, then we skip the next statements

          if(!interrupts.empty() && stepAction != admitNewProc) { // ---No--- Are there any pending interrupts? ---Yes
            IOInterrupt interrupt = interrupts.front();
            interrupts.pop_front(); //Removes interrupt

            for (auto it = blockedList.begin(); it != blockedList.end(); ) {  // Checks block list for correct process
              // if((*it)->id == interrupt.procID && (*it)->state == done) { // Checks if interrupt is calling for process that has already completed.
              //   cout << "Process already done; Interrupt" << endl;
              //   stepAction = handleInterrupt;
              //   runningProcess = nullptr;
              //   break;
              // }
              if ((*it)->id == interrupt.procID) {  // Found process and is not done
                if(usedMemoryPartitions < 4) { // Is there memory available? ---Yes
                  (*it)->state = ready;
                  usedMemory += (*it)->memoryRequired;
                  for(int i = 0; i < 4; i++) {
                    if(memoryPartitions[i] == -1) {
                      memoryPartitions[i] = (*it)->id;
                      usedMemoryPartitions++;
                      break;
                    }
                  }
                } else { // No
                  (*it)->state = memBlocked;
                }
                switch((*it)->level) { // Regardless of Memory availability, put process back into queue
                  case 3: highQueue.push(*it); break;
                  case 2: mediumQueue.push(*it); break;
                  case 1: lowQueue.push(*it); break;
                }
                it = blockedList.erase(it); // erases process from blocked list
                stepAction = handleInterrupt;
                break;
              } else {  
                ++it;  
              }  
            }  
          } else if(stepAction != admitNewProc) { // ---No--- Are there any processes in the ready Queues? ---Yes
            if(runningProcess == nullptr) {
              if (!highQueue.empty()) {
                runningProcess = highQueue.front();
                highQueue.pop();
              } else if (!mediumQueue.empty()) {
                runningProcess = mediumQueue.front();
                mediumQueue.pop();
              } else if (!lowQueue.empty()) {
                runningProcess = lowQueue.front();
                lowQueue.pop();
              }
              if (runningProcess) {
                if (runningProcess->state == memBlocked) { // Is memory allocated to this process? ---No
                  if (usedMemoryPartitions < 4)  { // Is there available memory now? ---Yes
                    usedMemory += runningProcess->memoryRequired; // Allocate memory 
                    for(int i = 0; i < 4; i++)  {
                      if(memoryPartitions[i] == -1) { // Found open partition
                        memoryPartitions[i] = runningProcess->id;
                        usedMemoryPartitions++;
                        break;
                      } else if (i == 3)  { // Error, open memory partition not found
                        cout << "Error, empty memory not found" << endl;
                      }
                    }
                  } else { // ---No, find lowest priority process to take memory from.
                    Process* lowProcess = nullptr;
                    for(auto& process : processList)  { // find lowest priority process and set lowProcess pointer to it
                      if(process.state == ready)  {
                        if(lowProcess == nullptr) {
                          lowProcess = &process;
                        } else if(process.level < lowProcess->level) {
                          lowProcess = &process;
                        }
                      }
                    }
                    lowProcess->state = memBlocked; // Deallocate memory
                    usedMemory -= lowProcess->memoryRequired;
                    usedMemoryPartitions--;
                    for(int i = 0; i < 4; i++)  { // Finds memory partition and swaps
                      if(memoryPartitions[i] == int(lowProcess->id)) { // Found lowPriority process's partition
                        memoryPartitions[i] = runningProcess->id;
                        usedMemoryPartitions++;
                        usedMemory += runningProcess->memoryRequired;
                        break;
                      } else if (i == 3)  { // Error, open memory partition not found
                        cout << "Error, memory not found" << endl;
                      }
                    }
                  }
                } // ---Yes, Continue
                runningProcess->state = processing;
                stepAction = beginRun;
              }
            }
          }
        }
        // Leave the below alone (at least for final submission, we are counting on the output being in expected format)
        cout << setw(5) << time << "\t"; 
        
        switch(stepAction)
        {
            case admitNewProc:
              cout << "[   admit]\t";
              break;
            case handleInterrupt:
              cout << "[  inrtpt]\t";
              break;
            case beginRun:
              cout << "[   begin]\t";
              break;
            case continueRun:
              cout << "[ contRun]\t";
              break;
            case ioRequest:
              cout << "[   ioReq]\t";
              break;
            case complete:
              cout << "[  finish]\t";
              break;
            case noAct:
              cout << "[ *noAct*]\t";
              break;
            case endLevel:
              cout << "[endLevel]\t";
              break;
        }

        // You may wish to use a second vector of processes (you don't need to, but you can)
        printProcessStates(processList); // change processList to another vector of processes if desired
        cout << usedMemoryPartitions << " [ ";
        for (int i = 0; i < 4; i++) {cout << memoryPartitions[i] << ' '; } 
        cout << "] usedMem:" << usedMemory << " lvl:";
        if(runningProcess) {cout << runningProcess->level << " ID:";}
        if(runningProcess) {cout << runningProcess->id << " Mem:";}
        if(runningProcess) {cout << runningProcess->memoryRequired;}
        cout << endl;
        this_thread::sleep_for(chrono::milliseconds(sleepDuration));
    }

    return 0;
}
