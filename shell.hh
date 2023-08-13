#ifndef shell_hh
#define shell_hh

#include "command.hh"

struct Shell {
	static void prompt();
	static Command _currentCommand;
	static std::vector<int> _backgroundPids;
	static int _lastReturnCode;
	static std::string _lastCmd;
	static bool _isSrc;
};

#endif
