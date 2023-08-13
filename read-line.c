#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

#define MAX_BUFFER_LINE 2048

extern void tty_raw_mode(void);

// buffer where line is stored
int line_length;
int right_length;
char line_buffer[MAX_BUFFER_LINE];
char right_buffer[MAX_BUFFER_LINE];

void read_line_print_usage() {
	char * usage = "\n"
    	" ctrl-?       Print usage\n"
    	" Backspace    Deletes last character\n"
    	" up arrow     See last command in the history\n";

  	write(1, usage, strlen(usage));
}

// input a line with some basic editing
char * read_line() {
	struct termios orig_attr; // save terminal
	tcgetattr(0, &orig_attr);

	// set terminal in raw mode
	tty_raw_mode();

	line_length = 0;
	right_length = 0;

	// read one line until enter is typed
	while (1) {
		char ch;
		read(0, &ch, 1);
		if (ch >= 32 && ch != 127) { // it is a printable character
			write(1, &ch, 1);

			// if max number of character reached return
			if (line_length==MAX_BUFFER_LINE - 2) break; 

			// add char to buffer
			line_buffer[line_length] = ch;
			line_length++;

			if (right_length != 0) {
				for (int i = right_length - 1; i >= 0; i--) {
					write(1, &right_buffer[i], 1);
				}

      				for (int i=0; i<right_length; i++) {
        				ch = 8;
        				write(1, &ch, 1);
      				}	
			}
		} else if (ch == 10) { // <Enter> was typed -> return line
			if (right_length != 0) {
				for (int i = right_length - 1; i >= 0; i--) {
          				line_buffer[line_length++]=right_buffer[i];
				}
			}
			
			right_length = 0;
			write(1, &ch, 1);

			break;
		} else if (ch == 31) { // ctrl-?
			read_line_print_usage();
			line_buffer[0] = 0;

			break;
		} else if (ch == 1) { // home key (ctrl-a) -> move to beginning of line
			int amount = line_length;
      			for (int i = 0; i < amount; i++) {
        			char c = 8;
        			write(1, &c, 1);
        			right_buffer[right_length++] = line_buffer[line_length - 1];
        			line_length--;
      			}
		} else if (ch == 5) { // end key (ctrl-e) -> move to end of line 
			for (int i = right_length - 1; i >= 0; i--) {
        			ch = 27;
				write(1,&ch,1);
				ch = 91;
				write(1,&ch,1);
				ch = 67;
				write(1,&ch,1);	

        			line_buffer[line_length++] = right_buffer[right_length - 1];
        			right_length--;
      			}
		} else if (ch == 127 || ch == 8) { // <backspace> was typed -> remove previous character read
			if (line_length != 0) { 
				ch = 8; // go back a char
				write(1,&ch,1);

				for(int i = right_length - 1; i >= 0; i--) {
        				write(1,&right_buffer[i],1);
      				}

				// write a space to erase the last character read
				ch = ' ';
				write(1, &ch, 1);

				for (int i = 0; i < right_length + 1; i++) {
        				ch = 8;
        				write(1, &ch, 1);
				}
				
				line_length--;
      			}
		} else if (ch == 4) { // delete (ctrl-d) -> remove char at cursor and shift left
			if (right_length != 0 && line_length != 0) {
				for (int i = right_length - 2; i >= 0; i--) {
					write(1, &right_buffer[i], 1);
				}

				ch = ' ';
				write(1,&ch,1);

				for (int i = 0;i < right_length; i++) {
					char c = 8;
					write(1, &c, 1);
				}

				right_length--;
			}
		} else if (ch == 27) { // escape sequence -> read two chars more
			char ch1; 
			char ch2;
			read(0, &ch1, 1);
			read(0, &ch2, 1);

			if (ch1 == 91 && ch2 == 65) { // up arrow -> future history
			
			} else if (ch1 == 91 && ch2 == 66) { // down arrow -> prior history
			
			} else if (ch1 == 91 && ch2 == 67) { // right arrow -> shift right
			        if (right_length != 0) {
					ch = 27;
					write(1,&ch,1);
					ch = 91;
					write(1,&ch,1);
					ch = 67;
					write(1,&ch,1);		

        				line_buffer[line_length++] = right_buffer[right_length - 1];
       					right_length--;	
				}
			} else if (ch1 == 91 && ch2 == 68) { // left arrow -> shift left
				if (line_length != 0) {
					ch = 8;
					write(1,&ch,1);
					
					right_buffer[right_length++] = line_buffer[line_length - 1];
        				line_length--;	
				}
			}
		}
	}

	// add eol and null char at the end of string
	line_buffer[line_length]=10;
	line_length++;
	line_buffer[line_length]=0;

	tcsetattr(0, TCSANOW, &orig_attr); // set terminal back 

	return line_buffer;
}

