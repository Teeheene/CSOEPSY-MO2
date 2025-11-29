class Dispatcher; // forward declare

class Process : public enable_shared_from_this<Process> {
public:
	//public details
	int pid;
	string pname;
	size_t pc = 0;
	bool sleeping = false;
	
private:
	//private details
	static inline atomic<int> nextPid = 1;

	//tasks
	vector<Instruction> instructions;
	unordered_map<string, uint16_t> variables; // varName, uint16
	bool running = false;

public:
	Process(string name = "") {
		pid = nextPid.fetch_add(1);
		if(name.empty()) {
			pname = "PROC-" + pid;
		}
	}

	void decode(const string&);
	uint16_t processArg(const string&);
	void execute(Instruction, Dispatcher*);
	Instruction getInstruction();
	void debug();
	bool hasInstructions();
	bool isFinished();
	void runCycle(Dispatcher*);
	void handleSleep(Dispatcher*, int);
};

bool Process::isFinished() {
	return !hasInstructions();
}

void Process::runCycle(Dispatcher* dispatcher) {
	if(hasInstructions()) {
		execute(getInstruction(), dispatcher);
		pc++;
	}
}

void Process::debug() {
	for(Instruction i : instructions) {
		i.debug();
	}
	cout << "  <PROCESS VARIABLES>" << endl;
	for (const auto& [key, value] : variables) {
      cout << "  " << key << " = " << value << endl;
	}
	cout << endl;
}

void Process::decode(const string& src) {
	stringstream ss(src);
	string instr;

	int i = 1;
	while(getline(ss, instr, ';')) {
		trim(instr);
		if(instr.empty()) continue;
		instructions.push_back(parseInstruction(instr));
	}
}

// checks to see if its a variable or value
// and returns a value
uint16_t Process::processArg(const string& arg) {
	if(isNumber(arg)) {
		return stringToUint16(arg);
	} else {
		return variables[arg];
	}
}

void Process::execute(Instruction instr, Dispatcher* dispatcher = nullptr) {
	switch(instr.type) {

	case OpCode::PRINT: {
		// ive yet to implement the logs for process
		// so it only prints it for now when testing
		cout << "{PRINT INSTR}" << instr.args[0]; 
		if(instr.args.size() < 2)
			cout << " from process " << pid << endl;
		else
			cout << processArg(instr.args[1]) << endl;
		break;
	}
		
	case OpCode::DECLARE: {		
		const string& var = instr.args[0];
		uint16_t value;

		if(instr.args.size() < 2)  
			value = 10; 	
		else 
			value = stringToUint16(instr.args[1]); 

		variables[var] = value;

		break;
	}

	case OpCode::ADD: {
		if(instr.args.size() < 3)
			throw runtime_error("[ERROR] Missing Arguments, ADD requires 3"); 

		const string& dest = instr.args[0];	
		const string& rawA = instr.args[1];	
		const string& rawB = instr.args[2];	

		const uint16_t a = processArg(rawA);
		const uint16_t b = processArg(rawB);
		
		uint32_t sum = static_cast<uint32_t>(a) + static_cast<uint32_t>(b);

		if(sum > UINT16_MAX)
			throw runtime_error("[ERROR] ADD overflow");

		variables[dest] = static_cast<uint16_t>(sum);
		break;
	}

	case OpCode::SUBTRACT: {
		if(instr.args.size() < 3)
			throw runtime_error("[ERROR] Missing Arguments, SUBTRACT requires 3"); 

		const string& dest = instr.args[0];	
		const string& rawA = instr.args[1];	
		const string& rawB = instr.args[2];	

		const uint16_t a = processArg(rawA);
		const uint16_t b = processArg(rawB);
		
		uint16_t diff = a - b;
		variables[dest] = diff;
		break;
	}

	case OpCode::SLEEP: {
		if(instr.args.empty())
			throw runtime_error("[ERROR] Missing Arguments, SLEEP requires 1");

		int ms = processArg(instr.args[0]);
		handleSleep(dispatcher, ms);
		break;
	}

	case OpCode::FOR: {
		if(instr.args.size() < 2)
			throw runtime_error("[ERROR] Missing Arguments, FOR requires 2");

		const string innerStr = instr.args[0];
		uint16_t loopCount = processArg(instr.args[1]);

		if(loopCount == 0)
			break;

		// 1 cycle, reading for loop
		vector<Instruction> instrs = parseInstructionArray(innerStr);	

		for(int i = 0; i < loopCount; i++) {
			for(Instruction i : instrs) {
				instructions.insert(instructions.begin(), i);
			}
		}

		break;
	}

	default: break;

	}
}

Instruction Process::getInstruction() {
	Instruction instr = instructions.front();
	instructions.erase(instructions.begin());
	return instr;
}

bool Process::hasInstructions() {
	if(instructions.empty())
		return false;
	return true;
}


