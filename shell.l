/*
 * shell.l: lexical analyzer for shell
 */
%x s

%{

#include <cstring>
#include "y.tab.hh"
#include "shell.hh"
#include <unistd.h>

static void yyunput (int c,char *buf_ptr  );
void myunputc(int c) { unput(c); }

extern "C" char * read_line();

int mygetc(FILE * f) {
	static char *p;
	char ch;

	if (!isatty(0)) { // stdin is not a tty. Call real getc
    		return getc(f);
  	}

  	// stdin is a tty. Call our read_line.
  	if (p==NULL || *p == 0) {
    		char * s = read_line();
    		p = s;
  	}

  	ch = *p;
  	p++;

  	return ch;
}

#undef getc
#define getc(f) mygetc(f)

%}

%option noyywrap

%%

\n { return NEWLINE; } 
[ \t] { } // discard spaces and tabs 
">" { return GREAT; } 
"<" { return LESS; } 
">>" { return GREATGREAT; }
"|" { return PIPE; }
"&" { return AMPERSAND; } 
">&" { return GREATAMPERSAND; } 
">>&" { return GREATGREATAMPERSAND; } 
"2>" { return TWOGREAT; }
source {
	BEGIN(s);
}
<s>[ ] {}
<s>[^ \t\n]+ {
	FILE * f = fopen(yytext, "a+");
	if (f) {
		fputc('\n', f);
		yypush_buffer_state(yy_create_buffer(f, YY_BUF_SIZE));
	        Shell::_isSrc = true;
		BEGIN(INITIAL);
		yyparse();
		yypop_buffer_state();
		Shell::_isSrc = false;
		fclose(f);
	}
}
[~][a-zA-Z0-9/]* { // ~ expansion
	std::string * in = new std::string(yytext);
	std::string result;

	if (in->size() == 1) result = "/homes/chris288/";
	else if (in->at(1) != '/') result = "/homes/" + in->substr(1, -1);
	else result = "/homes/chris288" + in->substr(1, -1);

	delete in;
	yylval.cpp_string = new std::string(result);
	return WORD;
}
[$][(][^\n]*[)] { // subshell
	std::string * temp = new std::string(yytext);
	const char * cmd = temp->substr(2, temp->length() - 3).c_str();

	int std_in = dup(0);
	int std_out = dup(1);
	int pipe_to_chld[2];
	int pipe_to_prnt[2];

	pipe(pipe_to_chld);
	pipe(pipe_to_prnt);

	write(pipe_to_chld[1], cmd, strlen(cmd));
	write(pipe_to_chld[1], "\n", 1);
	close(pipe_to_chld[1]);

	temp->clear();
	delete temp;

	dup2(pipe_to_chld[0], 0);
	dup2(pipe_to_prnt[1], 1);
	close(pipe_to_chld[0]);
	close(pipe_to_prnt[1]);

	int ret = fork();
	
	if (ret == 0) {
		execvp("/proc/self/exe", NULL);
		perror("execvp");
		_exit(1);
	} else if (ret < 0) {
		perror("fork");
		exit(1);
	} else {
		dup2(std_in, 0);
		dup2(std_out, 1);
		close(std_in);
		close(std_out);

		int i = 0;
		char c;
		char buffer[4096];
		while (read(pipe_to_prnt[0], &c, 1)) {
			if (c == '\n') buffer[i++] = ' ';
			else buffer[i++] = c;
		}
		buffer[i] = '\0';
		
		for (int j = i - 1; j >= 0; j--) {
			unput(buffer[j]);
		}
	}
}
[^ \t\n]*[$][{][^ \n]+[}][^ \t\n]* { // for env variables
	std::string * in = new std::string(yytext);
	std::string result = "";
	
	int start_index = 0, index = 0;
	while (index != -1) {
		index = in->find("$", start_index);
		std::string sub = in->substr(start_index, index - start_index);
		result += sub;

		int index2 = in->find("}", index);
		std::string sub2 = in->substr(index + 2, index2 - index - 2);  
	
		if (sub2.compare("$") == 0) {
			int pid = getpid();
			result += std::to_string(pid);	
		} else if (sub2.compare("?") == 0) {
			result += std::to_string(Shell::_lastReturnCode);
		} else if (sub2.compare("!") == 0) {
			if (Shell::_backgroundPids.size()) result += std::to_string(Shell::_backgroundPids.back());
		} else if (sub2.compare("_") == 0) {
			result += Shell::_lastCmd;	
		} else if (sub2.compare("SHELL") == 0) {
			result += "/u/riker/u91/chris288/cs252/lab3-src/shell";
		} else {
			if (getenv(sub2.c_str())) result += getenv(sub2.c_str());
			else result += sub2;
		}

		start_index = index2 + 1;
		index = in->find("$", start_index); 
	}

	if (start_index < in->length()) {
		std::string sub = in->substr(start_index, - 1);
		result += sub;
	}

	delete in;

	yylval.cpp_string = new std::string(result);
	return WORD;
}
["][^"]*["] { // allow for quotes as input but remove them before returning word
	yylval.cpp_string = new std::string(yytext);
	*yylval.cpp_string = yylval.cpp_string->substr(1, yylval.cpp_string->length()-2);
	return WORD;
}
[^ \\\t\n]*(\\["][^\t\n]*\\["]|\\[^\n\t])+((\\["][^\t\n]*\\["]|\\[^\n\t])|[^ \\\t\n])* { // allow for escape characters
	char * str = strdup(yytext);
	char * str_ret = (char *)malloc(strlen(yytext));
	
	int i;
	for (i = 0; *str; i++, str++) {
		if (*str == '\\') str++;
		str_ret[i] = *str;
	}
	str_ret[i] = '\0';

	yylval.cpp_string = new std::string(str_ret);
	free(str_ret);
	
	return WORD;
}
[^ \|><\t\n][^ \|><\t\n]* { // assume that file names have only alpha chars
	yylval.cpp_string = new std::string(yytext);
	return WORD;
}



