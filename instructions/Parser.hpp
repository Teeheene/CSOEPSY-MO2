OpCode parseOpCode(const string& raw) {
	if (raw == "PRINT") return OpCode::PRINT;
	if (raw == "DECLARE") return OpCode::DECLARE;
	if (raw == "ADD") return OpCode::ADD;
	if (raw == "SUBTRACT") return OpCode::SUBTRACT;
	if (raw == "SLEEP") return OpCode::SLEEP;
	if (raw == "FOR") return OpCode::FOR;

	throw runtime_error("Unknown opcode: " + raw);
}

Instruction parseInstruction(const string& raw) {
	Instruction instr;
	string token;
	bool opcode = true;
	bool inQuotes = false;

	for(size_t i = 0; i < raw.size(); i++) {
		char c = raw[i];

		if(c == '"') {
			if(inQuotes) {
				instr.args.push_back(token);
				token.clear();
				inQuotes = false;
			} else {
				inQuotes = true;
			}

			continue;
		}
		
		if(c == ' ' || c == '(' || c == ')' || c == '+') {
			if(!token.empty()) {
				if(opcode) {
					instr.type = parseOpCode(token);
					opcode = false;
				} else {
					instr.args.push_back(token);
				}
				token.clear();
			}
			continue;
		} 
		
		token.push_back(c);
	}

	if(!token.empty()) {
		if(opcode) {
			instr.type = parseOpCode(token);
		} else {
			instr.args.push_back(token);
		}
	}

	instr.debug();

	return instr;
}


