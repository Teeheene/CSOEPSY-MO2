class Instruction {
public:
	OpCode type;
	vector<string> args;

	Instruction(OpCode t = OpCode::NONE) 
		: type(t) {}
	
	void debug() {
		cout << "INSTR: " << toString(type) << " "; 
		for(string s : args) {
			cout << s << " ";
		}
		cout << endl;
	}
};
