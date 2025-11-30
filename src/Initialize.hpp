
// TODO: UPDATE CONFIG

struct Config
{
    int numcpu;
    std::string scheduler;
    long long int quantumCycles;
    long long int batchFreq;
    long long int minIns;
    long long int maxIns;
    long long int delayExec;

    bool loadFile();
    void print() const;
};

bool Config::loadFile()
{
    std::ifstream file;
    file.open("src/config.txt");
	 if (!file.is_open()) {
        std::cerr << "[Error] Could not open config.txt" << std::endl;
        return false;
    }


    std::string text;

    int error = 0;
    while (getline(file, text))
    {
        if (text.empty())
            continue;

        if (error != 0)
            continue;
        std::istringstream iss(text);
        std::string key, value;
        iss >> key >> value;

        if (key == "num_cpu")
        {
            int val = std::stoi(value);
            if (val >= 1 && val <= 128)
                numcpu = val;
            else
                error = 1;
        }

        else if (key == "scheduler")
        {
            if (value == "rr" || value == "fcfs")
                scheduler = (value);
            else
                error = 2;
        }
        else if (key == "quantum_cycles")
        {
            long long int val = std::stoll(value);
            if (val >= 1 && val <= 1LL << 32)
                quantumCycles = val;
            else
                error = 3;
        }
        else if (key == "batch_process_freq")
        {
            long long int val = std::stoll(value);
            if (val >= 1 && val <= 1LL << 32)
                batchFreq = val;
            else
                error = 4;
        }
        else if (key == "min_ins")
        {
            long long int val = std::stoll(value);
            if (val >= 1 && val <= 1LL << 32)
                minIns = val;
            else
                error = 5;
        }
        else if (key == "max_ins")
        {
            long long int val = std::stoll(value);
            if (val >= 1 && val <= 1LL << 32)
                maxIns = val;
            else if (val < minIns)
                error = 6;
            else
                error = 7;
        }
        else if (key == "delays_per_exec")
        {
            long long int val = std::stoll(value);
            if (val >= 0 && val <= 1LL << 32)
                delayExec = val;
            else
                error = 8;
        }
        else
        {
            std::cerr << "[Error] Unknown key: " << key << std::endl;
            return false;
        }

        if (error != 0)
        {
            switch (error)
            {
            case 1:
                std::cerr << "[Error] num_cpu out of range" << std::endl;
                break;
            case 2:
                std::cerr << "[Error] scheduler is not either rr or fcfs" << std::endl;
                break;
            case 3:
                std::cerr << "[Error] quantum_cycles out of range" << std::endl;
                break;
            case 4:
                std::cerr << "[Error] batch_process_freq out of range" << std::endl;
                break;
            case 5:
                std::cerr << "[Error] min_ins out of range" << std::endl;
                break;
            case 6:
                std::cerr << "[Error] min_ins greater than max_ins" << std::endl;
                break;
            case 7:
                std::cerr << "[Error] max_ins out of range" << std::endl;
                break;
            case 8:
                std::cerr << "[Error] delays_per_exec out of range" << std::endl;
                break;
            }
            return false;
        }
    }
    return true;
}

void Config::print() const
{
    std::cout << "numcpu: " << numcpu << "\n";
    std::cout << "scheduler: " << scheduler << "\n";
    std::cout << "quantumCycles: " << quantumCycles << "\n";
    std::cout << "batchFreq: " << batchFreq << "\n";
    std::cout << "minIns: " << minIns << "\n";
    std::cout << "maxIns: " << maxIns << "\n";
    std::cout << "delayExec: " << delayExec << "\n\n";
}
