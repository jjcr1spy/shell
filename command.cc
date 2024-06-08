#include <cstdio>
#include <cstdlib>
#include <iostream>
#include "command.hh"
#include "shell.hh"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

void myunputc(int c);

Command::Command() {
    // initialize a new vector of type SimpleCommand
    _simpleCommands = std::vector<SimpleCommand *>();
    _outFile = NULL;
    _inFile = NULL;
    _errFile = NULL;
    _background = false;
    _append = false;
}

void Command::insertSimpleCommand(SimpleCommand *simpleCommand) {
    _simpleCommands.push_back(simpleCommand);
}

void Command::clear() {
    // deallocate all the simple commands in the command vector
    for (auto simpleCommand : _simpleCommands) {
        delete simpleCommand;
    }

    // remove all references to the simple commands we've deallocated
    _simpleCommands.clear();

    if (_outFile) {
        delete _outFile;
    }
    _outFile = NULL;

    if (_inFile) {
        delete _inFile;
    }
    _inFile = NULL;

    if (_errFile) {
        delete _errFile;
    }
    _errFile = NULL;

    _background = false;
    _append = false;
}

void Command::print() {
    // use for debugging
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    int i = 0;
    for (auto &simpleCommand : _simpleCommands) {
        printf("  %-3d ", i++);
        simpleCommand->print();
    }

    printf("\n\n");
    printf("  Output       Input        Error        Background\n");
    printf("  ------------ ------------ ------------ ------------\n");
    printf("  %-12s %-12s %-12s %-12s\n",
           _outFile ? _outFile->c_str() : "default",
           _inFile ? _inFile->c_str() : "default",
           _errFile ? _errFile->c_str() : "default",
           _background ? "YES" : "NO");
    printf("\n\n");
    print();
}

void Command::execute() {
    // for every simple command we 1) fork a new process 2) setup i/o redirection 3) call exec
    if (_simpleCommands.size() == 0) {
        // don't do anything if there are no simple commands
        Shell::prompt();
        return;
    }

    // save stdin/out/err
    int std_in = dup(0);
    int std_out = dup(1);
    int std_err = dup(2);
    int fdin, fdout, fderr;

    if (_inFile) {
        // set initial input for command
        fdin = open(_inFile->c_str(), O_RDONLY);
    } else {
        fdin = dup(std_in);
    }

    if (_errFile) {
        // set initial error for command
        if (_append) {
            fderr = open(_errFile->c_str(), O_WRONLY | O_APPEND | O_CREAT, 0600);
        } else {
            fderr = open(_errFile->c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        }
    } else {
        fderr = dup(std_err);
    }

    dup2(fderr, 2); // redirect error
    close(fderr);

    int ret;
    for (unsigned int i = 0; i < _simpleCommands.size(); i++) {
        dup2(fdin, 0); // redirect input
        close(fdin);

        if (i == _simpleCommands.size() - 1) {
            // need to update output on last command
            std::string *temp = _simpleCommands[i]->_arguments.back();
            Shell::_lastCmd = *temp;
            if (_outFile) {
                // if our redirect appends
                if (_append) {
                    fdout = open(_outFile->c_str(), O_WRONLY | O_APPEND | O_CREAT, 0600);
                } else {
                    fdout = open(_outFile->c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
                }
            } else {
                fdout = dup(std_out);
            }
        } else {
            // create pipe if not last command
            int fdpipe[2];
            pipe(fdpipe); // pipe makes fdpipe point to two file descriptors that are connected
            fdout = fdpipe[1]; // what is wrote into fdpipe[1] can be read from fdpipe[0]
            fdin = fdpipe[0];
        }

        dup2(fdout, 1); // redirect output
        close(fdout);

        if (strcmp(_simpleCommands[0]->_arguments[0]->c_str(), "exit") == 0) {
            // exit command
            printf("Goodbye!\n");

            close(std_in);
            close(std_out);
            close(std_err);

            exit(0);
        } else if (strcmp(_simpleCommands[i]->_arguments[0]->c_str(), "setenv") == 0) {
            // setenv command
            int err;
            if (_simpleCommands[i]->_arguments.size() != 3) {
                fprintf(stderr, "setenv takes two arguments.\n");
            } else {
                err = setenv(_simpleCommands[i]->_arguments[1]->c_str(), _simpleCommands[i]->_arguments[2]->c_str(), 1);
            }
            if (err) {
                perror("setenv");
            }
        } else if (strcmp(_simpleCommands[i]->_arguments[0]->c_str(), "unsetenv") == 0) {
            // unsetenv command
            int err;
            if (_simpleCommands[i]->_arguments.size() != 2) {
                fprintf(stderr, "unsetenv takes one argument.\n");
            } else {
                err = unsetenv(_simpleCommands[i]->_arguments[1]->c_str());
            }
            if (err) {
                perror("unsetenv");
            }
        } else if (strcmp(_simpleCommands[i]->_arguments[0]->c_str(), "cd") == 0) {
            // cd command
            int err;
            if (_simpleCommands[i]->_arguments.size() == 1) {
                err = chdir(getenv("HOME"));
            } else {
                err = chdir(_simpleCommands[i]->_arguments[1]->c_str());
            }
            if (err < 0) {
                std::cerr << "cd: can't cd to " << *_simpleCommands[i]->_arguments[1] << "\n";
            }
        } else {
            ret = fork(); // create child process
            if (ret == 0) {
                // we are in the child process so run the command
                if (strcmp(_simpleCommands[i]->_arguments[0]->c_str(), "printenv") == 0) {
                    // printenv command
                    if (_simpleCommands[i]->_arguments.size() != 1) {
                        fprintf(stderr, "printenv takes no arguments.\n");
                    } else {
                        char **env = environ;
                        while (*env) {
                            printf("%s\n", *env);
                            env++;
                        }
                    }
                    exit(0);
                }

                char **cmd_and_args = new char *[(int)(_simpleCommands[i]->_arguments.size() + 1)];
                int max = _simpleCommands[i]->_arguments.size();
                for (int j = 0; j < max; j++) {
                    cmd_and_args[j] = (char *)_simpleCommands[i]->_arguments[j]->c_str();
                }
                cmd_and_args[max] = NULL;

                execvp(cmd_and_args[0], cmd_and_args);
                perror("execvp");
                exit(1);
            } else if (ret < 0) {
                // fork failed
                perror("fork");
                return;
            }
        }
    }

    if (!_background) {
        // if command doesnt run in the background wait for last command to finish
        int e;
        waitpid(ret, &e, 0);
        if (WIFEXITED(e)) {
            Shell::_lastReturnCode = WEXITSTATUS(e);
        }
    } else {
        Shell::_backgroundPids.push_back(ret); // for printing when background processes exit and send SIGCHLD
    }

    // redirect 0,1,2 file descriptors back to original stdin,out,err
    dup2(std_in, 0);
    dup2(std_out, 1);
    dup2(std_err, 2);

    // close file descriptors
    close(std_in);
    close(std_out);
    close(std_err);

    clear(); // clear to prepare for next command

    // print new prompt
    Shell::prompt();
}

SimpleCommand *Command::_currentSimpleCommand;
