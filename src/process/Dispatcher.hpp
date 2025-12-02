#include "Memory.hpp"

shared_ptr<MemoryManager> globalMem = nullptr;

struct sleepingProcess {
	shared_ptr<Process> proc;
	chrono::steady_clock::time_point wakeUpTime;
};

class Dispatcher {
private:
	queue<shared_ptr<Process>> readyQueue;
	queue<shared_ptr<Process>> finishedQueue;
	vector<sleepingProcess> idleQueue;
	mutex queueMtx;

	//core information ordered by vector placement
	//index counts towards core id
	vector<thread> cores;
	vector<shared_ptr<Process>> currentProc;
	mutex coreMtx;

	//info
	enum class Mode { FCFS, RR };
	Mode mode;
	int nCores;
	int quantum;
	int execDelay;
	int batchFreq;
	int minIns;
	int maxIns;

	//stress test decl	
	thread testThread;
	bool test;

	atomic<long long> activeTicks{0};
    atomic<long long> idleTicks{0};

public:
	Dispatcher() :
		mode(Mode::RR),
		nCores(4),
		quantum(3),
		execDelay(10),
		batchFreq(10),
		minIns(5),
		maxIns(10),
		test(false)
	{}

	void configure(Config cfg) {
    if(cfg.scheduler == "fcfs")
        mode = Mode::FCFS;
    else if(cfg.scheduler == "rr")
        mode = Mode::RR;

    nCores = cfg.numcpu;
    quantum = cfg.quantumCycles;
    execDelay = cfg.delayExec;
    batchFreq = cfg.batchFreq; 
    minIns = cfg.minIns;
    maxIns = cfg.maxIns;

    if (cfg.maxOverallMem > 0 && cfg.memPerFrame > 0) {
        globalMem = std::make_shared<MemoryManager>(cfg.maxOverallMem, cfg.memPerFrame);
    } else {
        std::cout << "[WARNING] Memory config missing or invalid. Using default 4096/16." << std::endl;
        globalMem = std::make_shared<MemoryManager>(4096, 16); 
    }
}
	
	void run();
	void addProcess(shared_ptr<Process>);
	void sleepProcess(shared_ptr<Process>, int);
	void showFinished();

	//for debugging
	shared_ptr<Process> searchProcess(string);
	
	//feats
	void startTest(int,int);
	void stopTest();
	// void enterProcessScreen(string);
	void ls();
	void printProcessSMI();
	void printVMStat();
	void backingstore();

private:
	void coreLoop(int);
	void wakeSleepingProcesses();
};

void Dispatcher::run() {
	currentProc = vector<shared_ptr<Process>>(nCores, nullptr); 

	for(int i = 0; i < nCores; i++)
		cores.emplace_back([this, i]() { coreLoop(i); });

	for(auto &t : cores) t.join();
}

void Dispatcher::addProcess(shared_ptr<Process> proc) {
	lock_guard<mutex> lock(queueMtx);
	readyQueue.push(proc);
}



/*===============================================================*/
// LOOP FOR CORES
/*===============================================================*/
void Dispatcher::coreLoop(int id) {
	while(true) {
		shared_ptr<Process> proc = nullptr;

		// lock for waking sleeping processes
		wakeSleepingProcesses();

		{
			lock_guard<mutex> lock(queueMtx);
			if(!readyQueue.empty()) {
				proc = readyQueue.front();
				readyQueue.pop();
			}
		}

		//check to see if it should allocate to cores current process
		{
			lock_guard<mutex> lock(coreMtx);

			if(!proc || proc->state != ProcessState::READY) {
				currentProc[id] = nullptr;
				idleTicks++;
				this_thread::sleep_for(chrono::milliseconds(5));
				continue;
			} else {
				currentProc[id] = proc;
				proc->state = ProcessState::RUNNING;
			}
		}

		if (mode == Mode::FCFS) {
			while (!proc->isFinished()) {
				proc->runCycle(this, id);
				activeTicks++;
				this_thread::sleep_for(chrono::milliseconds(execDelay));
			}

			lock_guard<mutex> lock(queueMtx);
			finishedQueue.push(proc);
			proc->state = ProcessState::FINISHED;
			if(globalMem) globalMem->deallocateMemory(proc->pid);

		} else {
			for(int i = 0; i < quantum && !proc->isFinished() &&
					proc->state != ProcessState::IDLE; i++) {
				proc->runCycle(this, id);
				activeTicks++;
				this_thread::sleep_for(chrono::milliseconds(execDelay));
			}

			//update queue after rr or fcfs
			{
				lock_guard<mutex> lock(queueMtx);

				//skip sleeping states (not to requeue w rq or fq)
				if (proc->state == ProcessState::IDLE) continue;

				if (proc->isFinished() && proc->state != ProcessState::IDLE) {
					finishedQueue.push(proc);
					proc->state = ProcessState::FINISHED;
					// FREE MEMORY
					if(globalMem) globalMem->deallocateMemory(proc->pid); 
				}
			}
		}
	}
}

/*===============================================================*/
// HANDLE SLEEPING PROCESSES
/*===============================================================*/
void Dispatcher::wakeSleepingProcesses() {
	auto now = chrono::steady_clock::now();
	
	if(mode == Mode::FCFS) return; 

	lock_guard<mutex> lock(queueMtx);
	for (auto it = idleQueue.begin(); it != idleQueue.end();) {
		if (it->wakeUpTime <= now) {
			if(it->proc->isFinished()) {
				if(it->proc->state != ProcessState::FINISHED) {
					it->proc->state = ProcessState::FINISHED;
					finishedQueue.push(it->proc);
				}
			} else {
				if(it->proc->state != ProcessState::READY) {
					it->proc->state = ProcessState::READY;
					readyQueue.push(it->proc);
				}
			}
			
			it->proc->sleeping = false;
			it = idleQueue.erase(it);
		} else {
			++it;
		}
	}
}

void Dispatcher::sleepProcess(shared_ptr<Process> proc, int ms) {
	lock_guard<mutex> lock(queueMtx);
	idleQueue.push_back(
		{ proc, chrono::steady_clock::now() + chrono::milliseconds(ms) });
}

void Process::handleSleep(Dispatcher* dispatcher, int ms) {
	sleeping = true;
	if(dispatcher)
		dispatcher->sleepProcess(shared_from_this(), ms);
}

/*===============================================================*/
// DISPATCHER HELPER 
/*===============================================================*/
shared_ptr<Process> Dispatcher::searchProcess(string name) {
	queue<shared_ptr<Process>> tempRQ;
	queue<shared_ptr<Process>> tempFQ;
	vector<shared_ptr<Process>> tempCP;
	vector<sleepingProcess> tempIQ;

	{
		lock_guard<mutex> lock(queueMtx);
		tempRQ = readyQueue;
		tempFQ = finishedQueue;
		tempCP = currentProc;
		tempIQ = idleQueue;
	}

   while (!tempRQ.empty()) {
   	auto &proc = tempRQ.front();
      if (proc && proc->pname == name)
         return proc;
      tempRQ.pop();
   }

	while(!tempFQ.empty()) {
		auto &proc = tempFQ.front();
		if(proc && proc->pname == name)
			return proc;
		tempFQ.pop();
	}

	for(auto proc : tempCP) {
		if(proc && proc->pname == name) {
			return proc;
		}
	}

	for(auto s : tempIQ) {
		if(s.proc && s.proc->pname == name) {
			return s.proc;
		}
	}

	return nullptr;
}

/*===============================================================*/
// REQUIRED FEATURES
/*===============================================================*/
// void Dispatcher::enterProcessScreen(string procName) {
// 	string rawInput;
// 	vector<string> cmd;
// 	bool screenDisplay = true;

// 	auto proc = searchProcess(procName);

// 	if(!proc) {
// 		cout << "Process <" << procName << "> not found." << endl;
// 		screenDisplay = false;
// 	} else {
// 		//clear screen
// 		cout << "\033[2J\033[1;1H";
// 	}

// 	while (screenDisplay)
// 	{
// 		cout << "root:\\> ";
// 		getline(cin, rawInput);
// 		cmd = tokenizeInput(rawInput);
// 		if(cmd.empty()) continue;

// 		if (cmd[0] == "process-smi") {
// 			int pc;
// 			int totalInstr;

//         	{
// 				lock_guard<mutex> lock1(proc->mtx);
//             pc = proc->pc;
//          	totalInstr = proc->totalInstr;
//         	}

//         	cout << "============== PROCESS SCREEN ==============" << endl;
//         	cout << "Process: " << proc->pname << "\nID: " << proc->pid 
// 				<< endl;
//       	cout << "\nLogs:\n";
// 			cout << proc->toStringLogs();

//         	cout << "\nInstructions Status: " << pc << " / " << totalInstr << endl;

// 			if(proc->isFinished())
// 			cout << "Finished!" << endl;

// 			// Calculate Global Memory Stats
// 			double totalMemMB = 0.0;
// 			double usedMemMB = 0.0;
// 			double utilPercent = 0.0;

// 			if (globalMem) {
// 				totalMemMB = static_cast<double>(globalMem->getMemorySize());
// 				usedMemMB = static_cast<double>(globalMem->getUsedMemory());
				
// 				if (totalMemMB > 0) {
// 					utilPercent = (usedMemMB / totalMemMB) * 100.0;
// 				}
// 			}

// 			// Print Header
// 			cout << "\n--------------------------------------------" << endl;
// 			cout << "| PROCESS-SMI V2.0  |  MEMORY MONITOR      |" << endl;
// 			cout << "--------------------------------------------" << endl;
// 			printf("Memory Usage: %.2f KiB / %.2f KiB\n", usedMemMB, totalMemMB);
// 			printf("Memory Util:  %.2f%%\n", utilPercent);
// 			cout << "--------------------------------------------" << endl;
// 			cout << "Running processes and memory usage:" << endl;

// 			// Iterate Running Processes (Requires Core Lock)
// 			{
// 				lock_guard<mutex> lock(coreMtx);
// 				bool noneRunning = true;

// 				for (const auto& p : currentProc) {
// 					if (p != nullptr) {
// 						noneRunning = false;
						
// 						// Get memory for this specific process
// 						double procMemMB = 0.0;
// 						if (globalMem) {
// 							procMemMB = static_cast<double>(globalMem->getProcessMemoryUsage(p->pid));
// 						}

// 						// Print Format: Name + ID + Usage
// 						cout << p->pname << " (ID: " << p->pid << ") \t" 
// 							<< procMemMB << " KiB" << endl;
// 					}
// 				}

// 				if (noneRunning) {
// 					cout << "[No processes currently running on cores]" << endl;
// 				}
// 			}
// 			cout << "--------------------------------------------" << endl;
// 			cout << "============================================" << endl;

// 		} else if (cmd[0] == "vmstat") {
// 			long long active = activeTicks.load();
// 			long long idle = idleTicks.load();
// 			long long totalTicks = active + idle;

// 			// Retrieve Memory Stats (safely via globalMem)
// 			size_t totalMem = 0; 
// 			size_t usedMem = 0;
// 			size_t freeMem = 0;
// 			int pIn = 0;
// 			int pOut = 0;

// 			if (globalMem) {
// 				totalMem = globalMem->getMemorySize();
// 				usedMem = globalMem->getUsedMemory();
// 				freeMem = globalMem->getFreeMemory();
// 				pIn = globalMem->getPagedInCount();
// 				pOut = globalMem->getPagedOutCount();
// 			}

// 			// 3. Print the Output Table
// 			cout << "\n" << string(60, '=') << endl;
// 			cout << "  VMSTAT (Virtual Memory Statistics)" << endl;
// 			cout << string(60, '=') << endl;
			
// 			cout << "Total Memory:       " << totalMem << " KB" << endl;
// 			cout << "Used Memory:        " << usedMem << " KB" << endl;
// 			cout << "Free Memory:        " << freeMem << " KB" << endl;
// 			cout << "Idle CPU Ticks:     " << idle << endl;
// 			cout << "Active CPU Ticks:   " << active << endl;
// 			cout << "Total CPU Ticks:    " << totalTicks << endl;
// 			cout << "Num Paged In:       " << pIn << endl;
// 			cout << "Num Paged Out:      " << pOut << endl;
// 			cout << string(60, '=') << endl;
// 		}else if (cmd[0] == "exit") {
// 			cout << "Returning home..." << endl;
// 			break;
// 		} else {
// 			cout << "Unknown command inside process screen." << endl;
// 		}
      
// 	}
// }

void Dispatcher::printProcessSMI() {
    double totalMemKB = 0.0;
    double usedMemKB = 0.0;
    double utilPercent = 0.0;

    if (globalMem) {
        // Assuming memory size is in Bytes, convert to KB
        totalMemKB = static_cast<double>(globalMem->getMemorySize()) / 1024.0;
        usedMemKB = static_cast<double>(globalMem->getUsedMemory()) / 1024.0;
        
        if (totalMemKB > 0) {
            utilPercent = (usedMemKB / totalMemKB) * 100.0;
        }
    }

    cout << "\n--------------------------------------------" << endl;
    cout << "| PROCESS-SMI V2.0  |  MEMORY MONITOR      |" << endl;
    cout << "--------------------------------------------" << endl;
    
    cout << "Memory Usage: " << usedMemKB << " KiB / " << totalMemKB << " KiB" << endl;
    cout << "Memory Util:  " << utilPercent << "%" << endl;
    cout << "--------------------------------------------" << endl;
    cout << "Running processes and memory usage:" << endl;

    // Iterate Running Processes on Cores
    {
        lock_guard<mutex> lock(coreMtx); // Lock cores to read currentProc safely
        bool noneRunning = true;

        for (int i = 0; i < nCores; i++) {
            if (i >= (int)currentProc.size()) break;

            auto p = currentProc[i];
            if (p != nullptr) {
                noneRunning = false;
                
                double procMemKB = 0.0;
                if (globalMem) {
                    // Convert specific process usage to KB
                    procMemKB = static_cast<double>(globalMem->getProcessMemoryUsage(p->pid)) / 1024.0;
                }

                // Output Format: [Core ID] ProcessName (ID) Memory
                cout << p->pname << " (ID: " << p->pid << ") \t" 
                     << procMemKB << " KiB" << endl;
            }
        }

        if (noneRunning) {
            cout << "[No processes currently running on cores]" << endl;
        }
    }
    
    cout << "--------------------------------------------" << endl;
    cout << "============================================" << endl;
}

void Dispatcher::printVMStat() {
    long long active = activeTicks.load();
    long long idle = idleTicks.load();
    long long totalTicks = active + idle;

    size_t totalMem = 0; 
    size_t usedMem = 0;
    size_t freeMem = 0;
    int pIn = 0;
    int pOut = 0;

    if (globalMem) {
        totalMem = globalMem->getMemorySize();
        usedMem = globalMem->getUsedMemory();
        freeMem = globalMem->getFreeMemory();
        pIn = globalMem->getPagedInCount();
        pOut = globalMem->getPagedOutCount();
    }

    cout << "\n" << string(60, '=') << endl;
    cout << "  VMSTAT (Virtual Memory Statistics)" << endl;
    cout << string(60, '=') << endl;
    cout << "Total Memory:       " << totalMem << " KB" << endl;
    cout << "Used Memory:        " << usedMem << " KB" << endl;
    cout << "Free Memory:        " << freeMem << " KB" << endl;
    cout << "Idle CPU Ticks:     " << idle << endl;
    cout << "Active CPU Ticks:   " << active << endl;
    cout << "Total CPU Ticks:    " << totalTicks << endl;
    cout << "Num Paged In:       " << pIn << endl;
    cout << "Num Paged Out:      " << pOut << endl;
    cout << string(60, '=') << endl;
}

void Dispatcher::backingstore() {
    ofstream file("csopesy-backing-store.txt");
    
    if (!file.is_open()) {
        cout << "[Error] Could not create csopesy-backing-store.txt" << endl;
        return;
    }
    
    double totalMemKB = 0.0;
    double usedMemKB = 0.0;
    double utilPercent = 0.0;

    if (globalMem) {
        totalMemKB = static_cast<double>(globalMem->getMemorySize()) / 1024.0;
        usedMemKB = static_cast<double>(globalMem->getUsedMemory()) / 1024.0;
        
        if (totalMemKB > 0) {
            utilPercent = (usedMemKB / totalMemKB) * 100.0;
        }
    }

    file << "--------------------------------------------" << endl;
    file << "| PROCESS-SMI V2.0  |  BACKING STORE DUMP  |" << endl;
    file << "--------------------------------------------" << endl;
    
    file << "Memory Usage: " << usedMemKB << " KiB / " << totalMemKB << " KiB" << endl;
    file << "Memory Util:  " << utilPercent << "%" << endl;
    file << "--------------------------------------------" << endl;
    file << "Running processes and memory usage:" << endl;
    {
        lock_guard<mutex> lock(coreMtx); 
        bool noneRunning = true;

        for (int i = 0; i < nCores; i++) {
            if (i >= (int)currentProc.size()) break;

            auto p = currentProc[i];
            if (p != nullptr) {
                noneRunning = false;
                
                double procMemKB = 0.0;
                if (globalMem) {
                    procMemKB = static_cast<double>(globalMem->getProcessMemoryUsage(p->pid)) / 1024.0;
                }

                file << p->pname << " (ID: " << p->pid << ") \t" 
                     << procMemKB << " KiB" << endl;
            }
        }

        if (noneRunning) {
            file << "[No processes currently running on cores]" << endl;
        }
    }
    
    file << "--------------------------------------------" << endl;
    file << "============================================" << endl;

    long long active = activeTicks.load();
    long long idle = idleTicks.load();
    long long totalTicks = active + idle;

    size_t totalMem = 0; 
    size_t usedMem = 0;
    size_t freeMem = 0;
    int pIn = 0;
    int pOut = 0;

    if (globalMem) {
        totalMem = globalMem->getMemorySize();
        usedMem = globalMem->getUsedMemory();
        freeMem = globalMem->getFreeMemory();
        pIn = globalMem->getPagedInCount();
        pOut = globalMem->getPagedOutCount();
    }

    file << "\n" << string(60, '=') << endl;
    file << "  VMSTAT (Virtual Memory Statistics)" << endl;
    file << string(60, '=') << endl;
    file << "Total Memory:       " << totalMem << " KB" << endl;
    file << "Used Memory:        " << usedMem << " KB" << endl;
    file << "Free Memory:        " << freeMem << " KB" << endl;
    file << "Idle CPU Ticks:     " << idle << endl;
    file << "Active CPU Ticks:   " << active << endl;
    file << "Total CPU Ticks:    " << totalTicks << endl;
    file << "Num Paged In:       " << pIn << endl;
    file << "Num Paged Out:      " << pOut << endl;
    file << string(60, '=') << endl;

    file.close();
    cout << "Success: Memory snapshot saved to 'csopesy-backing-store.txt'" << endl;
}

void Dispatcher::ls() {
	string runningProcs = "";
	int activeCores = 0;

	lock_guard<mutex> lock1(queueMtx);
	lock_guard<mutex> lock2(coreMtx);

	for(int i = 0; i < nCores; i++) {
   	if(i >= (int)currentProc.size()) continue;

		if(currentProc[i]) {
			activeCores++;
			runningProcs += "[CORE-" + to_string(i+1) + "] " +
				currentProc[i]->toStringRecentTimeLog() + 
				" for PROCESS " + currentProc[i]->pname + " | " + 
				to_string(currentProc[i]->pc) + "/" + 
				to_string(currentProc[i]->totalInstr) +
				" |\n"; 
		} else {
			runningProcs += "[CORE-" + to_string(i+1) + "] " + " INACTIVE\n"; 
		}
	}

	cout << "==============================================================" << endl;
	cout << "CPU utilization: " << (1.0 * activeCores / nCores * 100) << "%" << endl; 
	cout << "Cores used: " << activeCores << endl;
	cout << "Cores available: " << (nCores - activeCores) << endl;
	cout << "==============================================================" << endl;
	cout << "Running Processes: " << endl;
	cout << runningProcs;
	cout << "Finished Processes: " << endl;
	
   queue<shared_ptr<Process>> temp = finishedQueue; // copy to iterate safely

   while (!temp.empty()) {
      auto &p = temp.front();
		cout << "[" << p->pname << "] " << p->toStringRecentTimeLog() << 
			" FINISHED" << " | " << to_string(p->pc) << "/" << to_string(p->totalInstr)
			<< " |\n";
      temp.pop();
   }
	
	cout << "==============================================================" << endl;
}

void Dispatcher::startTest(int min, int max) {
	cout << "Test Started." << endl;
	test = true;
	testThread = thread([=]() {
		while(test) {
			shared_ptr<Process> p = createRandomProcess(minIns, maxIns, min, max);
			addProcess(p);
			this_thread::sleep_for(chrono::milliseconds(batchFreq));
		}
	});
}

void Dispatcher::stopTest() {
	test = false;
	if(testThread.joinable())
		testThread.join();
}
