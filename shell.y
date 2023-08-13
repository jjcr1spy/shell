/* CS-252
 * shell.y: parser for shell
 */


%code requires // allows for string and string.h to be included in y.tab.hh
{
#include <string>
#include <string.h>
}

%union
{
	char *string_val;
  	std::string *cpp_string;
}

%token <cpp_string> WORD
%token NOTOKEN GREAT NEWLINE LESS GREATGREAT PIPE AMPERSAND GREATAMPERSAND GREATGREATAMPERSAND TWOGREAT

%{
#include "shell.hh"
#include <regex.h>
#include <dirent.h>
#include <algorithm>
#include <unistd.h>

void yyerror(const char * s);
int yylex();
void expandWildcardsIfNecessary(std::string * arg);
bool cmp(const char * x, const char * y);
void expandWildcards(char * prefix, char * suffix);
%}

%%

goal:
	commands;

commands:
	command
	| commands command;

command: 
	simple_command;

simple_command:    
	pipe_list iomodifier_list background_opt NEWLINE { 
		Shell::_currentCommand.execute(); 
	}
  	| NEWLINE { 
		Shell::prompt(); 
	}
	| error NEWLINE { 
    		yyerrok;
  	};

command_and_args:
	command_word argument_list {
    		Shell::_currentCommand.insertSimpleCommand(Command::_currentSimpleCommand);
  	};

argument_list:
	argument_list argument
  	| // can be empty
  	;

argument:
  	WORD {
		expandWildcardsIfNecessary($1);
  	};

command_word:
	WORD {
    		Command::_currentSimpleCommand = new SimpleCommand();
    		Command::_currentSimpleCommand->insertArgument($1);
  	};

pipe_list:
	pipe_list PIPE command_and_args
  	| command_and_args;

background_opt:
	AMPERSAND { 
		Shell::_currentCommand._background = true; 
	}
  	| // can be empty
  	;

iomodifier_list:
	iomodifier_list iomodifier_opt
  	| // can be empty
  	;

iomodifier_opt:
	GREAT WORD { // redirect output
		if (Shell::_currentCommand._outFile == NULL) Shell::_currentCommand._outFile = $2;
		else printf("Ambiguous output redirect.\n");
  	}
  	| LESS WORD { // redirect input to word
		if (Shell::_currentCommand._inFile == NULL) Shell::_currentCommand._inFile = $2;
		else printf("Ambiguous output redirect.\n");
  	}
  	| GREATGREAT WORD { // redirect output and append
		if (Shell::_currentCommand._outFile == NULL) {
			Shell::_currentCommand._outFile = $2;
    			Shell::_currentCommand._append = true;
		} else printf("Ambiguous output redirect.\n");
  	}
  	| GREATAMPERSAND WORD { // redirect output and error to word
		if (Shell::_currentCommand._outFile != NULL || Shell::_currentCommand._errFile != NULL) {
			printf("Ambiguous output redirect.\n");
		} else {
			Shell::_currentCommand._outFile = new std::string($2->c_str());
			Shell::_currentCommand._errFile = new std::string($2->c_str());
		}
  	}
  	| GREATGREATAMPERSAND WORD { // redirect output and error to word and append
		if (Shell::_currentCommand._outFile != NULL || Shell::_currentCommand._errFile != NULL) {
			printf("Ambiguous output redirect.\n");
		} else {
			Shell::_currentCommand._outFile = new std::string($2->c_str());
			Shell::_currentCommand._errFile = new std::string($2->c_str());
			Shell::_currentCommand._append = true;
		}
  	}
  	| TWOGREAT WORD { // redirect error to word
		if (Shell::_currentCommand._errFile == NULL) Shell::_currentCommand._errFile = $2;
		else printf("Ambiguous output redirect.\n");
  	};

%%

bool cmp(const char * x, const char * y) {
	return strcmp(x, y) < 0;
}

std::vector<char *> vec;

void expandWildcards(char * prefix, char * suffix) {
	if (suffix[0] == 0) { // base case
		vec.push_back(strdup(prefix));
		return;
	}

	std::string expanded = "";
	if (prefix) expanded += prefix;

	char * s = suffix;
	if (suffix[0] == '/') { 
		s++;
		expanded += '/';
	}

	bool hide = false; // include hidden files/dirs or not
	if (*s == '.') hide = true;

	std::string for_reg = "";
	while (*s != '/' && *s) {
		expanded += *s;
		for_reg += *s;
		s++;
	}
	
	if (expanded.find('*') == -1 && expanded.find('?') == -1) { // no need for regex
		expandWildcards(expanded.data(), s); 				
		return;
	} else { // need regex
		char * reg = (char *) malloc(2*for_reg.size() + 10);
		char * a = for_reg.data();
		char * r = reg;

		*(r++) = '^';
		while (*a) { // turn input into regex format
			if (*a == '*') { *(r++) = '.'; *(r++) = '*'; }
			else if (*a == '?') { *(r++) = '.'; }
			else if (*a == '.') { *(r++) = '\\'; *(r++) = '.'; }
			else { *(r++) = *a; }
			a++;
		}
		*(r++) = '$'; 
		*r = '\0';

		regex_t re;
		int expbuf = regcomp(&re, reg, REG_EXTENDED|REG_NOSUB);
		struct dirent * ent; // member d_name refers to null-terminated file name component

		DIR * dir;
		if (prefix != NULL) dir = opendir(prefix);
		else if (suffix[0] == '/') dir = opendir("/");
		else dir = opendir(".");

		if (dir == NULL) {
			return;
		}

		while ((ent = readdir(dir)) != NULL) { // loop through all files in dir
			if (regexec(&re, ent->d_name, 1, NULL, 0) == 0) { // if file name matches regex
				if ((ent->d_name[0] == '.' && hide) || (ent->d_name[0] != '.')) {  
					std::string tmp = "";
					
					if (prefix == NULL) {
						if (suffix[0] == '/') tmp += '/';
						tmp += ent->d_name; 
					} else if (!strcmp(prefix, "/")) {
						tmp += '/';
						tmp += ent->d_name;
					} else {
						tmp += prefix;
						tmp += '/';
						tmp += ent->d_name;
					}
				
					char * new_prefix = strdup(tmp.c_str());
					expandWildcards(new_prefix, s);
					free(new_prefix);
				}
			}
		}

		free(reg);
		closedir(dir);
		regfree(&re);	
	}
}

void expandWildcardsIfNecessary(std::string * arg) {
	if (strchr(arg->c_str(), '*') == NULL) {
		Command::_currentSimpleCommand->insertArgument(arg);
	} else {
		expandWildcards(NULL, arg->data());
		std::sort(vec.begin(), vec.end(), cmp);	
		for (auto i : vec) {
			std::string * temp = new std::string(i);
			Command::_currentSimpleCommand->insertArgument(temp);
		}
		vec.clear();
		vec.shrink_to_fit();
	}
}

void yyerror(const char * s) {
	fprintf(stderr,"%s\n", s);
}
