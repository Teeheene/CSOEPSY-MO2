#include "Memory.hpp"

extern shared_ptr<MemoryManager> globalMem;

using namespace std;

enum class ProcessState {
	RUNNING,
	READY,
	IDLE,
	FINISHED 
};

class Dispatcher; // forward declare

class Process : public enable_shared_from_this<Process> {
public:
	//public details
	int pid;
	string pname;
	size_t pc = 0;
	int totalInstr;
	bool sleeping = false;
	vector<ProcessLogEntry> logHistory;
	ProcessState state;

	mutex logMtx;
	mutex mtx;
	
private:
	//private details
	static inline atomic<int> nextPid = 1;

	//tasks
	vector<Instruction> instructions;
	unordered_map<string, int> symbolTable; 
	int nextVirtAddr = 0;
	bool running = false;

public:
	Process(string name = "") :
		state(ProcessState::READY)	
	{
		pid = nextPid.fetch_add(1);
		if(name.empty()) {
			pname = "PROC-" + to_string(pid);
		}

		if (globalMem) {
			globalMem->allocateMemory(pid);
		}
	}

	//run1 instr
	void runCycle(Dispatcher*, int);

	//exec
	uint16_t processArg(const string&);
	void decode(const string&);
	string execute(Instruction, Dispatcher*);

	//instr 
	Instruction getInstruction();
	bool hasInstructions();
	bool isFinished();
	int countInstructions(const vector<Instruction>&);
	int instructionsLeft();
	
	//sleep
	void handleSleep(Dispatcher*, int);
	
	//logs
	void log(int, string);
	void smi();
	string toStringRecentTimeLog();
	string toStringLogs();

private:
	// Helper to resolve a variable name to a Virtual Address
	int getAddress(const string& varName) {
		if (symbolTable.find(varName) == symbolTable.end()) {
			// If variable is new, assign it the next available virtual address
			symbolTable[varName] = nextVirtAddr++;
		}
		return symbolTable[varName];
	}
};

string Process::toStringLogs() {
	lock_guard<mutex> lock(logMtx);

	if(logHistory.empty()) return "";

	string res = ""; 
	for(ProcessLogEntry p : logHistory) {
		res += p.toString();	
	}

	return res;
}

int Process::instructionsLeft() {
	return totalInstr - pc;
}

string Process::toStringRecentTimeLog() {
	lock_guard<mutex> lock(logMtx);
	time_t timestamp = time(nullptr);

	if(logHistory.empty()) {
		char strTime[100];
		tm* translTimestamp = localtime(&timestamp);
		strftime(strTime, sizeof(strTime), "%m/%d/%Y %I:%M:%S%p", translTimestamp);

		return "(" + string(strTime) + ")";
	} else {
		return logHistory.back().toStringTimestamp();
	}
}

void Process::log(int id, string out) {
	if(out.empty()) return; 
	lock_guard<mutex> lock(logMtx);
	ProcessLogEntry entry(pid, id, out);
	logHistory.push_back(entry);
}

void Process::runCycle(Dispatcher* dispatcher, int coreId) {
	if(hasInstructions()) {
		string out = execute(getInstruction(), dispatcher);
		{
			lock_guard<mutex> lock(mtx);
			pc++;
		}
		log(coreId, out);
	}
}

Instruction Process::getInstruction() {
	lock_guard<mutex> lock(mtx);
	if(instructions.empty())
		throw runtime_error("ERROR empty instructions");
	Instruction instr = instructions.front();
	instructions.erase(instructions.begin());
	return instr;
}

bool Process::isFinished() {
	return pc >= static_cast<size_t>(totalInstr);
}

bool Process::hasInstructions() {
	if(instructions.empty())
		return false;
	return true;
}

void Process::smi() {
	cout << "Process name: " << pname << endl;
	cout << "ID: " << pid << endl;

	cout << "Logs: " << endl;

	{
		lock_guard<mutex> lock(logMtx);
		for(auto l : logHistory) {
			l.print();
		}
	}

	cout << endl; 
	cout << "Current instruction line: " << pc << endl;
	cout << "Lines of code: " << totalInstr << endl;
}

// checks to see if its a variable or value
// and returns a value
uint16_t Process::processArg(const string& arg) {
	if(isNumber(arg)) {
		return stringToUint16(arg);
	} else {
		int addr = getAddress(arg);
		if (globalMem) {
			// access(pid, virtualAddress, isWrite, writeValue)
			return globalMem->access(pid, addr, false); 
		} else {
			// Fallback if memory manager not initialized (shouldn't happen in full config)
			return 0;
		}
	}
}

// Count instructions in a vector, expanding FORs recursively
int Process::countInstructions(const vector<Instruction> &instrs) {
    int total = 0;

    for (const Instruction &instr : instrs) {
        if (instr.type == OpCode::FOR) {
			  total++;
            if (instr.args.size() < 2) continue; // skip malformed

            const string &innerStr = instr.args[0];
            uint16_t loopCount = processArg(instr.args[1]); // or 0 if unavailable
            if (loopCount == 0) continue;

            vector<Instruction> innerInstrs = parseInstructionArray(innerStr);
            int innerCount = countInstructions(innerInstrs); // recursive

            total += loopCount * innerCount;
        } else {
            total += 1;
        }
    }

    return total;
}

void Process::decode(const string& src) {
	stringstream ss(src);
	string instr;

	while(getline(ss, instr, ';')) {
		trim(instr);
		if(instr.empty()) continue;
		instructions.push_back(parseInstruction(instr));
	}

	totalInstr = countInstructions(instructions);
}	

string Process::execute(Instruction instr, Dispatcher* dispatcher = nullptr) {
	switch(instr.type) {

	case OpCode::PRINT: {
		string out;

		if(instr.args.empty())
			break;

		out = instr.args[0]; 
		if(instr.args.size() < 2)
			out += " from process " + to_string(pid);
		else
			out += to_string(processArg(instr.args[1]));
		return out;
	}
		
	case OpCode::DECLARE: {		
		if(instr.args.empty())
			break;

		const string& var = instr.args[0];
		uint16_t value;

		if(instr.args.size() < 2)  
			value = 10; 	
		else 
			value = stringToUint16(instr.args[1]); 

		int addr = getAddress(var);
		if(globalMem) {
			globalMem->access(pid, addr, true, value);
		}

		break;
	}

	case OpCode::ADD: {
		if(instr.args.size() < 3)
			//throw runtime_error("[ERROR] Missing Arguments, ADD requires 3"); 
			break;

		const string& dest = instr.args[0];	
		const string& rawA = instr.args[1];	
		const string& rawB = instr.args[2];	

		const uint16_t a = processArg(rawA);
		const uint16_t b = processArg(rawB);
		
		uint32_t sum = static_cast<uint32_t>(a) + static_cast<uint32_t>(b);

		if(sum > UINT16_MAX)
			//throw runtime_error("[ERROR] ADD overflow");
			break;

		int addr = getAddress(dest);
		if(globalMem) {
			globalMem->access(pid, addr, true, static_cast<uint16_t>(sum));
		}
		break;
	}

	case OpCode::SUBTRACT: {
		if(instr.args.size() < 3)
			//throw runtime_error("[ERROR] Missing Arguments, SUBTRACT requires 3"); 
			break;

		const string& dest = instr.args[0];	
		const string& rawA = instr.args[1];	
		const string& rawB = instr.args[2];	

		const uint16_t a = processArg(rawA);
		const uint16_t b = processArg(rawB);
		
    	uint16_t diff = (b > a) ? 0 : (a - b); // clamp at 0
		int addr = getAddress(dest);
		if(globalMem) {
			globalMem->access(pid, addr, true, diff);
		}
		break;
	}

	case OpCode::SLEEP: {
		if(instr.args.empty())
			//throw runtime_error("[ERROR] Missing Arguments, SLEEP requires 1");
			break;

		state = ProcessState::IDLE;
		int ms = processArg(instr.args[0]);
		handleSleep(dispatcher, ms);
		break;
	}

	case OpCode::FOR: {
		if(instr.args.size() < 2)
			//throw runtime_error("[ERROR] Missing Arguments, FOR requires 2");
			break;

		const string innerStr = instr.args[0];
		uint16_t loopCount = processArg(instr.args[1]);

		if(loopCount == 0)
			break;

		// 1 cycle, reading for loop
		vector<Instruction> instrs = parseInstructionArray(innerStr);	

		lock_guard<mutex> lock(mtx);
		for(int i = 0; i < loopCount; i++) {
      	instructions.insert(
         	instructions.begin(),
         	std::make_move_iterator(instrs.begin()),
         	std::make_move_iterator(instrs.end())
      	);
		}

		break;
	}

	default: break;

	}

	return "";
}


