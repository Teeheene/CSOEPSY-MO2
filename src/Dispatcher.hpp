struct sleepingProcess {
	shared_ptr<Process> proc;
	chrono::steady_clock::time_point wakeUpTime;
};

class Dispatcher {
private:
	queue<shared_ptr<Process>> readyQueue;
	queue<shared_ptr<Process>> finishedQueue;
	vector<sleepingProcess> idleQueue;
	int nCores;
	int quantum;

	mutex queueMtx;

public:
	enum class Mode { FCFS, RR };
	Mode mode;

	Dispatcher(int cores, Mode m = Mode::RR, int q = 1) :
		nCores(cores), mode(m), quantum(q) {}

	void run();
	void addProcess(shared_ptr<Process>);
	void sleepProcess(shared_ptr<Process>, int);
	void showFinished();

private:
	void coreLoop();
	void wakeSleepingProcesses();
};

void Dispatcher::run() {
	vector<thread> cores;
	for(int i = 0; i < nCores; i++) {
		cores.emplace_back([this]() { coreLoop(); });
	} 

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

void Dispatcher::showFinished() {
	lock_guard<mutex> lock(queueMtx);

	cout << "Finished" << endl;
	queue<shared_ptr<Process>> temp = finishedQueue;

	while(!temp.empty()) {
		auto p = temp.front();
		temp.pop();

		cout << "[Finished] Process ID: " << p->pid << endl;
	}
}

void Dispatcher::coreLoop() {
	while(true) {
		shared_ptr<Process> proc = nullptr;

		{
			lock_guard<mutex> lock(queueMtx);
			wakeSleepingProcesses();
			if(!readyQueue.empty()) {
				proc = readyQueue.front();
				readyQueue.pop();
			}
		}

		if(!proc) {
			this_thread::sleep_for(chrono::milliseconds(5));
			continue;
		}

		int steps = (mode == Mode::RR) ? quantum : INT_MAX;
		for(int i = 0; i < steps && !proc->isFinished(); i++) {
			proc->runCycle(this);
			this_thread::sleep_for(chrono::milliseconds(100));
		}

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

// sleep helper to connect dispatcher and proc
void Process::handleSleep(Dispatcher* dispatcher, int ms) {
	sleeping = true;
	if(dispatcher)
		dispatcher->sleepProcess(shared_from_this(), ms);
}

