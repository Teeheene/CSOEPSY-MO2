
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
						if (cmd.size() == 2)
						{
							shared_ptr<Process> p = createRandomProcess(minIns, maxIns);
							dispatcher.addProcess(p);
							dispatcher.enterProcessScreen(p->pname);
						}
						else
						{
							dispatcher.addProcess(createRandomProcess(minIns, maxIns));
							dispatcher.enterProcessScreen(cmd[2]);
						}
					}
					else if (cmd[1] == "-r")
					{
						if (cmd.size() == 2)
						{
							cout << "Missing argument: Process Name" << endl;
						}
						else
						{
							dispatcher.enterProcessScreen(cmd[2]);
						}
					}
					else if (cmd[1] == "-ls") {
						dispatcher.ls();
					}
				}
				else if (cmd[0] == "scheduler-start" || cmd[0] == "scheduler-test")
				{
					dispatcher.startTest();
				}
				else if (cmd[0] == "scheduler-stop")
				{
					dispatcher.stopTest();
				}
				else if (cmd[0] == "report-util")
				{
					//implement below :>
					//handleReportCommand(dispatcher.;
					cout << "report file created at ./csopesy-log.txt" << endl;
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


