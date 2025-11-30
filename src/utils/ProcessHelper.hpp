random_device rd;
mt19937 rng(rd());
int rndInt(int a, int b) {
    uniform_int_distribution<int> dist(a, b);
    return dist(rng);
}


string generateInstr(int &budget, int depth = 0) {
    if (budget <= 0) return "";

    vector<string> ops = {"PRINT", "DECLARE", "ADD", "SUBTRACT", "SLEEP"};
    vector<string> vars = {"varA", "varB", "varC"};

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
         	//return "SLEEP " + to_string(rndInt(50, 500));
            return "PRINT(\"Hello World\")";
        return "";
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
shared_ptr<Process> createRandomProcess(int minIns, int maxIns) {
    auto p = make_shared<Process>();
    string script = generateScript(minIns, maxIns);

    //cout << "\n=== GENERATED SCRIPT ===\n" << script << "\n=========================\n";

    p->decode(script);
    return p;
}
