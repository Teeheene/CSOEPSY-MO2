#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <cstdint>
#include <variant>
#include <stdexcept>
using namespace std;

/* HEADERS *****************/
#include "utils/helper.hpp" 
#include "instructions/OpCode.hpp"
#include "instructions/Instruction.hpp"
#include "instructions/Parser.hpp"
#include "Process.hpp"
/***************************/

/*
 * The idea is to have a system where it can decode 
 * per instruction line instead of returning a full
 * vector of instructions (sorry i tried it hurts)
 * */

int main() {
	// process & instruction demo
	string script = "DECLARE varA 10; DECLARE varB 5; ADD varA 5 6; PRINT(\"varA before looping: \" + varA); SUBTRACT varC varA 3; FOR([FOR([ADD varA varA 4],3)],3); PRINT(\"Result after looping: \" + varA)";

	Process p;
	p.decode(script);
	while(p.hasInstructions())
		p.execute(p.getInstruction());
}
