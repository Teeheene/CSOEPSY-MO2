random_device rd;
mt19937 rng(rd());
int rndInt(int a, int b) {
    uniform_int_distribution<int> dist(a, b);
    return dist(rng);
}


string generateInstr(int &budget, int depth = 0) {
    if (budget <= 0) return "";

    //read and write is not generated during stress tests
    vector<string> ops = {"PRINT", "DECLARE", "ADD", "SUBTRACT, SLEEP, WRITE"};
    vector<string> vars = {
    "V01", "V02", "V03", "V04", "V05", "V06", "V07", "V08", "V09", "V10", 
    "V11", "V12", "V13", "V14", "V15", "V16", "V17", "V18", "V19", "V20"
    };

    // Only allow FOR at depth 0
    if (depth == 0 && budget >= 3) ops.push_back("FOR");

    string op = ops[rndInt(0, ops.size() - 1)];
    string var = vars[rndInt(0, vars.size() - 1)];

    if (op != "FOR") {
        budget--;
        if (op == "DECLARE") {
            int val = rndInt(1, 200);
            return "DECLARE " + var + " " + to_string(val);
        }
        if (op == "PRINT")
            return "PRINT(\"Value of " + var + ": \" + " + var + ")";
        if (op == "ADD")
            return "ADD " + var + " " + var + " " + to_string(rndInt(1, 50));
        if (op == "SUBTRACT")
            return "SUBTRACT " + var + " " + var + " " + to_string(rndInt(1, 50));
        if (op == "SLEEP" && depth == 0)
         	// return "SLEEP " + to_string(rndInt(50, 500));
            return "PRINT(\"Value of " + var + ": \" + " + var + ")";
        if (op == "READ") {
            int addr = rndInt(0, 4096);
            stringstream ss;
            ss << "0x" << std::hex << addr;
            return "READ " + var + " " + ss.str();
        }

        if (op == "WRITE") {
            int addr = rndInt(0, 4096);
            stringstream ss;
            ss << "0x" << std::hex << addr;
            
            int val = rndInt(0, 100);
            return "WRITE " + ss.str() + " " + to_string(val);
        }
    }

    // Single-level FOR
    int loopCount = rndInt(1,2);
    int innerCount = min(budget - 1, rndInt(1, 3));

    vector<string> inner;
    for (int i = 0; i < innerCount; i++) {
        string instr = generateInstr(innerCount, depth + 1); // depth + 1 disables FOR inside
        if (instr.empty()) break;
        inner.push_back(instr);
    }

    if (inner.empty()) return generateInstr(budget, depth);

    budget -= 1 + inner.size() * loopCount;

    stringstream ss;
    ss << "FOR([";
    for (size_t i = 0; i < inner.size(); i++) {
        ss << inner[i];
        if (i + 1 < inner.size()) ss << ", ";
    }
    ss << "]," << loopCount << ")";
    return ss.str();
}


// Generate full script
string generateScript(int minIns, int maxIns) {
    int budget = rndInt(minIns, maxIns);
    stringstream script;

    while (budget > 0) {
        string instr = generateInstr(budget);
        if (instr.empty()) break;
        script << instr << "; ";
    }

    return script.str();
}

// Create Process
shared_ptr<Process> createRandomProcess(int minIns, int maxIns, 
      int minMem, int maxMem = -1) {
   string script = generateScript(minIns, maxIns);

   //cout << "DEBUG: " << script << endl;

   int mem = 0;
   if(maxMem == -1)
      mem = minMem;
   else
      mem = rndInt(minMem, maxMem);  

   auto p = make_shared<Process>("", mem);
   p->decode(script);
   return p;
}
