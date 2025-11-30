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
	}
	
	void run();
	void addProcess(shared_ptr<Process>);
	void sleepProcess(shared_ptr<Process>, int);
	void showFinished();

	//for debugging
	shared_ptr<Process> searchProcess(string);
	
	//feats
	void startTest();
	void stopTest();
	void enterProcessScreen(string);
	void ls();

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

			//check status
			//only READY state processes are allowed to be taken
			//by core
			if(!proc || proc->state != ProcessState::READY) {
				currentProc[id] = nullptr;
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
				this_thread::sleep_for(chrono::milliseconds(execDelay));
			}

			lock_guard<mutex> lock(queueMtx);
			finishedQueue.push(proc);
			proc->state = ProcessState::FINISHED;

		} else {
			for(int i = 0; i < quantum && !proc->isFinished() &&
					proc->state != ProcessState::IDLE; i++) {
				proc->runCycle(this, id);
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

				} else if(proc->state == ProcessState::RUNNING) {
					readyQueue.push(proc);
					proc->state = ProcessState::READY;
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
		if(cmd.empty()) continue;

		if (cmd[0] == "process-smi") {
			int pc;
			int totalInstr;

        	{
				lock_guard<mutex> lock1(proc->mtx);
            pc = proc->pc;
         	totalInstr = proc->totalInstr;
        	}

        	cout << "================ PROCESS SCREEN ================" << endl;
        	cout << "Process: " << proc->pname << "\nID: " << proc->pid 
				<< endl;
      	cout << "\nLogs:\n";
			cout << proc->toStringLogs();

        	cout << "\nInstructions Status: " << pc << " / " << totalInstr << endl;

			if(proc->isFinished())
			cout << "Finished!" << endl;
			cout << "================================================" << endl;
		} else if (cmd[0] == "exit") {
			cout << "Returning home..." << endl;
			break;
		} else {
			cout << "Unknown command inside process screen." << endl;
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
		cout << "[" << p->pname << "] " << p->toStringRecentTimeLog() << 
			" FINISHED" << " | " << to_string(p->pc) << "/" << to_string(p->totalInstr)
			<< " |\n";
      temp.pop();
   }
	
	cout << "==============================================================" << endl;
}

void Dispatcher::startTest() {
	cout << "Test Started." << endl;
	test = true;
	testThread = thread([&]() {
		while(test) {
			shared_ptr<Process> p = createRandomProcess(minIns, maxIns);
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
