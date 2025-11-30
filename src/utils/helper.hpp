/* helpers */

static inline void ltrim(std::string &s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(),
   	[](unsigned char ch){ return !std::isspace(ch); }));
}

static inline void rtrim(std::string &s) {
	s.erase(std::find_if(s.rbegin(), s.rend(),
		[](unsigned char ch){ return !std::isspace(ch); }).base(), s.end());
}

static inline void trim(std::string &s) {
	ltrim(s);
	rtrim(s);
}


uint16_t stringToUint16(const string &s) {
	unsigned long val = stoul(s);

	if(val > UINT16_MAX) {
		throw out_of_range("[WARNING] Value too big for uint16_t");
	}

	return static_cast<uint16_t>(val);
}

bool isNumber(const std::string& s) {
	if (s.empty()) return false;

	size_t i = 0;

	if (s[0] == '+' || s[0] == '-') i++;

	for (; i < s.size(); i++) {
		if (!std::isdigit(s[i])) return false;
	}

	return true;
}

vector<string> tokenizeInput(string input)
{
	vector<std::string> tokens;
	string token{""};

	if (input.empty())
		return {};

	for (char ch : input)
	{
		// if its not a space
		if (!isspace(static_cast<unsigned char>(ch)))
		{
			token += ch;
		}
		else
		{
			tokens.push_back(token);
			token = "";
		}
	}
	tokens.push_back(token);

	return tokens;
}


