#ifndef _HEADER_PTSFUNCTIONS
#define _HEADER_PTSFUNCTIONS
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#ifdef TARGET_ANDROID
int posix_openpt(int oflag);
char * ptsname(int fdm);
int grantpt(int fdm);
int unlockpt(int fdm);
int ptym_open( char* pts_name , int pts_namesz);
int ptys_open(char* pts_name);
pid_t pty_fork( int *ptrfdm , char *slave_name , int slave_namesz , const struct termios *slave_termios,
		const struct winsize *slave_winsize);
#endif
int tty_raw(int fd);

#endif
