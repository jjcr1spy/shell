#ifndef command_hh
#define command_hh

#include "simpleCommand.hh"

struct Command {
	std::vector<SimpleCommand *> _simpleCommands;
	std::string * _outFile;
	std::string * _inFile;
	std::string * _errFile;
	bool _append;
	bool _background;

	Command();

	void insertSimpleCommand( SimpleCommand * simpleCommand );
	void clear();
	void print();
	void execute();

	static SimpleCommand *_currentSimpleCommand;
};

#endif
