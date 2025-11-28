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


		// First, parse the opcode
      if (opcode && !isspace(c)) {
         token.push_back(c);
         if (i + 1 == raw.size() || raw[i+1] == '(' || isspace(raw[i+1])) {
            instr.type = parseOpCode(token);
            opcode = false;
            token.clear();

            // Special handling for FOR instruction
            if (instr.type == OpCode::FOR) {
               // Skip until '('
               while (i < raw.size() && raw[i] != '(') i++;
               i++; // skip '('

               string forArg;
               int parenCount = 1; // initial '('
               while (i < raw.size() && parenCount > 0) {
                  char ch = raw[i];

                  // toggle inQuotes if unescaped
                  if (ch == '"' && (i == 0 || raw[i-1] != '\\')) inQuotes = !inQuotes;

                  if (!inQuotes) {
                     if (ch == '(') parenCount++;
                     if (ch == ')') parenCount--;
                  }

                  if (parenCount > 0) forArg.push_back(ch);
                  i++;
               }

               // Find the last top-level comma outside quotes to split inner instruction from loop count
               size_t commaPos = string::npos;
               int nested = 0;
               bool inQ = false;
               for (size_t j = 0; j < forArg.size(); j++) {
                  char ch = forArg[j];
                  if (ch == '"' && (j == 0 || forArg[j-1] != '\\')) inQ = !inQ;
                  if (!inQ) {
                     if (ch == '(') nested++;
                     if (ch == ')') nested--;
                     if (ch == ',' && nested == 0) commaPos = j;
                  }
               }

               if (commaPos == string::npos)
                  throw runtime_error("Malformed FOR instruction");

               string innerInstrStr = forArg.substr(0, commaPos);
               string loopCountStr = forArg.substr(commaPos + 1);

               instr.args.push_back(innerInstrStr);
               instr.args.push_back(loopCountStr);
               break; // FOR parsed, stop
            }
         }
         continue;
      }

   
		// handle PRINT
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
		
		// handle DELIMITERS
		if((c == ' ' || c == '(' || c == ')' || c == '+') && !inQuotes) {
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

	//instr.debug();

	return instr;
}

vector<Instruction> parseInstructionArray(const string& rawArray) {
   vector<Instruction> result;

   if (rawArray.empty() || rawArray.front() != '[' || rawArray.back() != ']')
   	throw runtime_error("[ERROR] Expected instruction array in brackets");

   string content = rawArray.substr(1, rawArray.size() - 2);
   string token;
   bool inQuotes = false;
   int parenCount = 0;
   int bracketCount = 0;

   for (size_t i = 0; i < content.size(); i++) {
      char c = content[i];

      if (c == '"' && (i == 0 || content[i-1] != '\\')) inQuotes = !inQuotes;

      if (!inQuotes) {
         if (c == '(') parenCount++;
         if (c == ')') parenCount--;
         if (c == '[') bracketCount++;
         if (c == ']') bracketCount--;

         if (c == ',' && parenCount == 0 && bracketCount == 0) {
            if (!token.empty()) {
               result.push_back(parseInstruction(token));
               token.clear();
            }
            continue;
         }
      }

      token.push_back(c);
   }

   // Push last token
   if (!token.empty()) {
      result.push_back(parseInstruction(token));
   }

   return result;
}

