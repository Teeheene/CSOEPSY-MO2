enum class OpCode {
	PRINT,
	DECLARE,
	ADD,
	SUBTRACT,
	SLEEP,
	FOR,
    READ,
    WRITE,
	NONE
};

string toString(OpCode op) {
    switch (op) {
        case OpCode::PRINT:     return "PRINT";
        case OpCode::DECLARE:   return "DECLARE";
        case OpCode::ADD:       return "ADD";
        case OpCode::SUBTRACT:  return "SUBTRACT";
        case OpCode::SLEEP:     return "SLEEP";
        case OpCode::FOR:       return "FOR";
        case OpCode::READ:      return "READ";
        case OpCode::WRITE:     return "WRITE";
        default:
            return "UNKNOWN";
    }
}
