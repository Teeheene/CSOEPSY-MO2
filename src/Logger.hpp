struct ProcessLogEntry {
	time_t timestamp;
	int cid;
	int pid;
	string out;

	ProcessLogEntry(int pid_, int cid_, string out_) {
		timestamp = time(nullptr);
		pid = pid_;
		cid = cid_;
		out = out_;	
	}

	void print() {
		//get timestamp adjusted to local time
		char strTime[100];
		tm* translTimestamp = localtime(&timestamp);
		strftime(strTime, sizeof(strTime), "%m/%d/%Y %I:%M:%S%p", translTimestamp);

		//print the log details
		cout << "(" << strTime << ")" << " Core:" << cid 
			<< " \"" << out << "\"" << endl;
	}

	string toStringTimestamp() {
		//get timestamp adjusted to local time
		char strTime[100];
		tm* translTimestamp = localtime(&timestamp);
		strftime(strTime, sizeof(strTime), "%m/%d/%Y %I:%M:%S%p", translTimestamp);

		//print the log details
		return "(" + string(strTime) + ")";
	}

	string toString() {
		//get timestamp adjusted to local time
		char strTime[100];
		tm* translTimestamp = localtime(&timestamp);
		strftime(strTime, sizeof(strTime), "%m/%d/%Y %I:%M:%S%p", translTimestamp);

		//print the log details
		return "(" + string(strTime) + ")" + " Core:" + to_string(cid)
			+ " \"" + out + "\"\n";
	}
};
