struct sleepingProcess {
	shared_ptr<Process> proc;
	chrono::steady_clock::time_point wakeUpTime;
};

class Dispatcher {
private:
	queue<shared_ptr<Process>> runningQueue;
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

public:
	Dispatcher() :
		mode(Mode::RR),
		nCores(4),
		quantum(3),
		execDelay(10),
		batchFreq(10),
		minIns(5),
		maxIns(10)
	{}

	void configure(Config cfg) {
		if(cfg.scheduler == "fcfs")
			mode = Mode::RR;
		else if(cfg.scheduler == "rr")
			mode = Mode::FCFS;

		nCores = cfg.numcpu;
		quantum = cfg.quantumCycles;
		execDelay = cfg.delayExec;
		batchFreq = cfg.batchFreq; 
		minIns = cfg.minIns;
		maxIns = cfg.maxIns;
	}
	
	void run();
	void addProcess(shared_ptr<Process>);
	void sleepProcess(shared_ptr<Process>, int);
	void showFinished();

	//for debugging
	void ls();
	shared_ptr<Process> searchProcess(string);
	void enterProcessScreen(string);
	//implement smi()

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

shared_ptr<Process> Dispatcher::searchProcess(string name) {
	lock_guard<mutex> lock(queueMtx);

	queue<shared_ptr<Process>> temp = runningQueue;
   while (!temp.empty()) {
   	auto &proc = temp.front();
		cout << proc->pname;
      temp.pop();
      if (proc && proc->pname == name)
         return proc;
   }

	queue<shared_ptr<Process>> temp1 = readyQueue;
   while (!temp1.empty()) {
   	auto &proc = temp1.front();
		cout << proc->pname;
      temp1.pop();
      if (proc && proc->pname == name)
         return proc;
   }

	queue<shared_ptr<Process>> temp2 = finishedQueue;
	while(!temp2.empty()) {
		auto &proc = temp2.front();
		temp2.pop();
		if(proc && proc->pname == name)
			return proc;
	}

	for(auto &s : idleQueue) {
		if(s.proc && s.proc->pname == name) {
			return s.proc;
		}
	}

	return nullptr;
}

void Dispatcher::enterProcessScreen(string procName) {
	string rawInput;
	vector<string> cmd;
	bool screenDisplay = true;

	auto proc = searchProcess(procName);

	if(!proc) {
		cout << "Process <" << procName << "> not found." << endl;
		screenDisplay = false;
	} else {
		//clear screen
		cout << "\033[2J\033[1;1H";
	}

	while (screenDisplay)
	{
		cout << "root:\\> ";
		getline(cin, rawInput);
		cmd = tokenizeInput(rawInput);

		int pc;
		int totalInstr;
		vector<ProcessLogEntry> logs;
		string logToString;

		if (cmd[0] == "process-smi")
		{
        	{
				lock_guard<mutex> lock1(proc->mtx);
				lock_guard<mutex> lock2(proc->logMtx);
            logToString = "";
				logs = proc->logHistory;
            pc = proc->pc;
         	totalInstr = proc->totalInstr;
        	}

        	cout << "================ PROCESS SCREEN ================" << endl;
        	cout << "\n\t| Process: " << proc->pname << " | ID: " << proc->pid;
        	cout << " | " << pc << "/" << totalInstr << " |" << endl;
      	cout << "\nLogs:\n";

			for(ProcessLogEntry p : logs) {
				logToString += p.toString();
			}
			cout<< logToString << endl;

			if(proc->isFinished())
				cout << "Finished!" << endl << endl;
		} else if (cmd[0] == "exit") {
			cout << "Returning home..." << endl;
			break;
		} else {
			cout << "Unknown command inside process screen." << endl;
		}
      
		cout << "================================================" << endl;
	}
}

void Dispatcher::sleepProcess(shared_ptr<Process> proc, int ms) {
	lock_guard<mutex> lock(queueMtx);
	idleQueue.push_back(
		{ proc, chrono::steady_clock::now() + chrono::milliseconds(ms) });
}

void Dispatcher::coreLoop(int id) {
	while(true) {
		shared_ptr<Process> proc = nullptr;

		// lock for waking sleeping processes
		{
			lock_guard<mutex> lock(queueMtx);
			wakeSleepingProcesses();
			if(!readyQueue.empty()) {
				proc = readyQueue.front();
				readyQueue.pop();
				if (runningQueue.empty() || runningQueue.back() != proc)
				   runningQueue.push(proc);

			}
		}

		if(!proc) {
			lock_guard<mutex> lock(coreMtx);
			currentProc[id] = nullptr;
			this_thread::sleep_for(chrono::milliseconds(5));
			continue;
		}
		
		{
			lock_guard<mutex> lock(coreMtx);
			currentProc[id] = proc;
		}

		//fetch execution cycle
		int steps = (mode == Mode::RR) ? quantum : proc->totalInstr;
		for(int i = 0; i < steps && !proc->isFinished(); i++) {
			proc->runCycle(this, id);
			this_thread::sleep_for(chrono::milliseconds(10));
		}

//		//update queue after rr or fcfs
//		{
//			lock_guard<mutex> lock(queueMtx);
//			runningQueue.pop();
//
//			if(proc->sleeping) {
//				//nothing
//			} else if(proc->isFinished()) {
//				std::queue<std::shared_ptr<Process>> temp = finishedQueue;
//				bool exists = false;
//				while (!temp.empty()) {
//					 if (temp.front() == proc) { exists = true; break; }
//					 temp.pop();
//				}
//				if (!exists) finishedQueue.push(proc);
//			} else {
//				std::queue<std::shared_ptr<Process>> temp = readyQueue;
//				bool exists = false;
//				while (!temp.empty()) {
//					 if (temp.front() == proc) { exists = true; break; }
//					 temp.pop();
//				}
//				if (!exists) readyQueue.push(proc);
//			}
//		}

				// update queues safely
		{
			lock_guard<mutex> lock(queueMtx);

			// remove from runningQueue ONLY if the front == this proc
			if (!runningQueue.empty() && runningQueue.front() == proc)
				runningQueue.pop();

			// handle sleeping
			if (proc->sleeping) {
				// do nothing
			}
			// handle finished
			else if (proc->isFinished()) {
				queue<shared_ptr<Process>> tmp = finishedQueue;
				bool exists = false;
				while (!tmp.empty()) {
					if (tmp.front() == proc) { exists = true; break; }
					tmp.pop();
				}
				if (!exists)
					finishedQueue.push(proc);
			}
			// handle ready
			else {
				queue<shared_ptr<Process>> tmp = readyQueue;
				bool exists = false;
				while (!tmp.empty()) {
					if (tmp.front() == proc) { exists = true; break; }
					tmp.pop();
				}
				if (!exists)
					readyQueue.push(proc);
			}
		}

	}
}

void Dispatcher::wakeSleepingProcesses() {
	auto now = chrono::steady_clock::now();
	for (auto it = idleQueue.begin(); it != idleQueue.end();) {
		if (it->wakeUpTime <= now) {
			if(it->proc->isFinished())
				finishedQueue.push(it->proc);
			else
				readyQueue.push(it->proc);
			
			it->proc->sleeping = false;
			it = idleQueue.erase(it);
		} else {
			++it;
		}
	}
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
      temp.pop();
		cout << "[" << p->pname << "] " << p->toStringRecentTimeLog() << 
			" FINISHED" << " | " << to_string(p->pc) << "/" << to_string(p->totalInstr)
			<< " |\n";
   }
	
	cout << "==============================================================" << endl;
}

// sleep helper to connect dispatcher and proc
void Process::handleSleep(Dispatcher* dispatcher, int ms) {
	sleeping = true;
	if(dispatcher)
		dispatcher->sleepProcess(shared_from_this(), ms);
}

