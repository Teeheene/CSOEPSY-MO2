#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <cstdint>
#include <variant>
#include <stdexcept>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <chrono>
#include <climits>
#include <atomic>

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
	// dispatcher demo
	Dispatcher deez(4);

	// process & instruction demo
	string script = "DECLARE varA 10; DECLARE varB 5; ADD varA 5 6; PRINT(\"varA before looping: \" + varA); SUBTRACT varC varA 3; FOR([FOR([ADD varA varA 4],3)],3); PRINT(\"Result after looping: \" + varA)";

	auto p = make_shared<Process>();
	p->decode(script);

	deez.addProcess(p);

	std::thread dispatcherThread([&deez]() {
		 deez.run(); 
	});
	dispatcherThread.detach();

	auto p1 = std::make_shared<Process>();
	p1->decode("PRINT(\"Hello\");");
	deez.addProcess(p1);

	auto p2 = std::make_shared<Process>();
	p2->decode("PRINT(\"Hello\");");
	deez.addProcess(p2);

	auto p3 = std::make_shared<Process>();
	p3->decode("SLEEP 5000; PRINT(\"Hello\");");
	deez.addProcess(p3);

	while(true) {
		deez.showFinished();
		this_thread::sleep_for(chrono::milliseconds(1000));
	}
}
