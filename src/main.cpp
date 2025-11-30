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
#include <random>
#include <fstream>
#include <memory>

//temp
#include <iomanip>

using namespace std;
bool running = true;

/* HEADERS *****************/
#include "utils/helper.hpp" 
#include "instructions/OpCode.hpp"
#include "instructions/Instruction.hpp"
#include "instructions/Parser.hpp"
#include "Logger.hpp"
#include "Process.hpp"
#include "utils/ProcessHelper.hpp"
#include "Initialize.hpp"
#include "Dispatcher.hpp"
#include "MainController.hpp"
/***************************/

int main() {
	MainController os;
	os.run();
}
