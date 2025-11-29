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

public:
	enum class Mode { FCFS, RR };
	Mode mode;
	int nCores;
	int quantum;

	Dispatcher(int cores, Mode m = Mode::RR, int q = 1) :
		nCores(cores), mode(m), quantum(q) {}

	void run();
	void addProcess(shared_ptr<Process>);
	void sleepProcess(shared_ptr<Process>, int);
	void showFinished();
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

void Dispatcher::sleepProcess(shared_ptr<Process> proc, int ms) {
	lock_guard<mutex> lock(queueMtx);
	cout << "SLEEPING" << endl; 
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
		int steps = (mode == Mode::RR) ? quantum : INT_MAX;
		for(int i = 0; i < steps && !proc->isFinished(); i++) {
			proc->runCycle(this, id);
			this_thread::sleep_for(chrono::milliseconds(100));
		}

		//update queue after rr or fcfs
		{
			lock_guard<mutex> lock(queueMtx);
			if(proc->isFinished()) {
				finishedQueue.push(proc);
			} else if(!proc->sleeping) {
				readyQueue.push(proc);
			}
		}
	}
}

void Dispatcher::wakeSleepingProcesses() {
	auto now = chrono::steady_clock::now();
	for (auto it = idleQueue.begin(); it != idleQueue.end();) {
		if (it->wakeUpTime <= now) {
			readyQueue.push(it->proc);
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

