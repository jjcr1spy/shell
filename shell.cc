#include <cstdio>
#include "shell.hh"
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "y.tab.hh"

int yyparse(void);

void Shell::prompt() {
	if (isatty(0) && !Shell::_isSrc) {
		printf("myshell>");
		fflush(stdout);
	}
}

void int_handler(int sig) {
	if (sig == SIGINT) { // ctrl-c
		printf("\n");
		if (Shell::_currentCommand._simpleCommands.size() == 0) Shell::prompt(); 	
	}
	
	if (sig == SIGCHLD) { // zombie elimination
		int pid;
		while((pid = waitpid(-1, &pid, WNOHANG)) > 0) { // use while loop bc more children could have changed or terminated sending SIGCHLDs that would not be queued while in handler
			for (int i = 0; i < Shell::_backgroundPids.size(); i++) {
				if (Shell::_backgroundPids[i] == pid) {
					printf("\n[%d] exited.\n", pid);
					Shell::prompt();
					Shell::_backgroundPids.erase(Shell::_backgroundPids.begin() + i);
					break;
				}
			}
		}
	}
}

int main() {
	struct sigaction signal_action; // handling ctrl-c
	signal_action.sa_handler = &int_handler; // function pointer to handle ctrl-c interrupt signal
	sigemptyset(&signal_action.sa_mask); // initializes the signal set to exclude all defined signals
	signal_action.sa_flags = SA_RESTART; // prevents system calls from being interrupted by retrying any calls interrupted by a signal	

	if (sigaction(SIGINT, &signal_action, NULL)) { // non-zero means failed, used to change the action taken by a process on receipt of a specific signal
		perror("sigaction");
		exit(-1);
	}	

	if (sigaction(SIGCHLD, &signal_action, NULL)) {
		perror("sigaction");
		exit(-1);
	}

	Shell::_isSrc = false;
	Shell::prompt();
	yyparse();
}

Command Shell::_currentCommand;
std::vector<int> Shell::_backgroundPids;
int Shell::_lastReturnCode;
std::string Shell::_lastCmd;
bool Shell::_isSrc;
