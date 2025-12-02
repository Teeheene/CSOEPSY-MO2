
class MainController
{
	string rawInput;
	vector<string> cmd;
	bool initialized;

public:
	MainController() :
		initialized(false)
	{}

	void run()
	{	
		Dispatcher dispatcher;
		thread t;
		int minIns, maxIns;
      int minMem, maxMem;

		while (running)
		{
			cout << "root:\\> ";
			getline(cin, rawInput);
			cmd = tokenizeInput(rawInput);

			if(cmd.empty()) continue;

			if (!initialized)
			{
				if (cmd[0] == "initialize")
				{
					// TODO: UPDATE CONFIG TO MATCH MO2
					Config cfg;
					cout << "Initializing processor configuration..." << endl;

					if (cfg.loadFile())
					{
						std::cout << "configuration loaded successfully.\n\n";
						cfg.print();

						dispatcher.configure(cfg);

						minIns = cfg.minIns;
						maxIns = cfg.maxIns;
                  minMem = cfg.minMemPerProc;
                  maxMem = cfg.maxMemPerProc;

						std::cout << "scheduler started successfully.\n\n";
						initialized = true;
						
						t = thread([&dispatcher]() { dispatcher.run(); });
						t.detach();
					}
					else
					{
						std::cerr << "failed to load configuration.\n\n";
					}
				}
				else if (cmd[0] == "exit")
				{
					cout << "exiting program..." << endl;
					running = false;
				}
				else
				{
					cout << "Please initialize first!" << endl;
				}
			}

			else if (initialized)
			{
				if (cmd[0] == "screen")
				{
					if (cmd.size() == 1)
					{
						cout << "Missing argument after 'screen'" << endl;
					}
					else if (cmd[1] == "-s")
					{
                  //screen -s PROC-1 120
						if (cmd.size() == 4)
						{
                     try {
                        int mem = stoi(cmd[3]); //memsize
                        if(mem >= 64 && mem <= 65536) { 
                           //update create random process to contain 
                           //name and mem size
                           shared_ptr<Process> p = createRandomProcess(minIns, 
                                 maxIns, mem);
                           dispatcher.addProcess(p);
                           // dispatcher.enterProcessScreen(p->pname);
                        } else {
                           cout << "invalid memory allocation\n" << endl;
                        }

                     } catch (const std::exception& e) {
                        cout << "invalid memory allocation\n" << endl;
                     }
					   } else {
                     cout << "Missing arguments: screen -s <name> <mem-size>\n";
						}
					}
					// else if(cmd[1] == "-r") {
					// 	if (cmd.size() == 2) 
					// 		cout << "Missing argument: Process Name" << endl;
					// 	else
					// 		dispatcher.enterProcessScreen(cmd[2]);
					// }
					else if(cmd[1] == "-ls") 
						dispatcher.ls();
               else if(cmd[1] == "-c") {
                  //screen -c PROC-1 64 "instr"
                  if (cmd.size() >= 4) {
                     try {
                        int mem = stoi(cmd[3]);
                        if(mem >= 64 && mem <= 65536) {
                           shared_ptr<Process> p = make_shared<Process>(cmd[2], mem);
                           p->decode(cmd[4]);
						   dispatcher.addProcess(p);
                           // dispatcher.enterProcessScreen(p->pname);
                        } else {
                           cout << "invalid memory allocation" << endl;
                        }
                     } catch (const exception& e) {
                        cout << "invalid memory allocation" << endl;
                     }
					   } else {
                     cout << "Missing arguments: screen -c <name>"
                        << " \"<instructions>\"" << endl;
						}
               }
				}
				else if (cmd[0] == "scheduler-start" || cmd[0] == "scheduler-test")
				{
					dispatcher.startTest(minMem, maxMem);
				}
				else if (cmd[0] == "scheduler-stop")
				{
					dispatcher.stopTest();
				}
				else if (cmd[0] == "backing-store")
				{
					dispatcher.backingstore();
				}
				else if (cmd[0] == "process-smi")
				{
					dispatcher.printProcessSMI();
				}
				else if (cmd[0] == "vmstat")
				{
					dispatcher.printVMStat();
				}
				else if (cmd[0] == "exit")
				{
					cout << "exiting program..." << endl;
					running = false;
				}
				else
				{
					cout << "Unknown command." << endl;
				}
			}
		}
	}

//	void handleReportCommand(Scheduler& dispatcher. {
//    
//		std::string report = dispatcher.reportUtil();
//		std::ofstream outFile("csopesy-log.txt");
//    outFile << report;
//   	outFile.close();
//	}
};


