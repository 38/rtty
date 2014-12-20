#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <errno.h>

#define _HAS_GRANTPT

#define _HAS_UNLOCKPT

#ifdef TARGET_ANDROID
#ifndef _HAS_OPENPT
int posix_openpt(int oflag)
{
	return open( "/dev/ptmx" , oflag );
}
#endif
#ifndef _HAS_PTSNAME
char * ptsname(int fdm)
{
	int minor;
	static char pts_name[16];
	if( ioctl( fdm , TIOCGPTN , &minor ) )
		return NULL;
	snprintf( pts_name ,  sizeof(pts_name) , "/dev/pts/%d" , minor );
	return pts_name;
}
#endif
#ifndef _HAS_GRANTPT
int grantpt(int fdm)
{
	char* pts_name;
	pts_name = ptsname(fdm);
	return chmod(pts_name , S_IRUSR|S_IWUSR|S_IWGRP);
}
#endif
#ifndef _HAS_UNLOCKPT
int unlockpt(int fdm)
{
	int lock = 0;
	return ioctl(fdm,TIOCSPTLCK,&lock);
}
#endif
int ptym_open( char* pts_name , int pts_namesz)
{
	char* ptr;
	int fdm;
	pts_name[pts_namesz-1] = 0;
	fdm = posix_openpt(O_RDWR);
	if( fdm < 0 )
		return -1;
	if( grantpt( fdm ) < 0 )
		return -2;
	if( unlockpt( fdm ) < 0 )
		return -3;
	if( (ptr = ptsname(fdm) ) == NULL )
		return -4;
	strncpy( pts_name , ptr,pts_namesz );
	pts_name[pts_namesz-1] = 0;
	return fdm;
}
int ptys_open(char* pts_name)
{
	int fds;
	if( ( fds = open( pts_name , O_RDWR ) ) < 0 )
	       return -5;
	return fds;	       
}
pid_t pty_fork( int *ptrfdm , char *slave_name , int slave_namesz , const struct termios *slave_termios,
		const struct winsize *slave_winsize)
{
	char pts_name[20];
	int fdm = ptym_open(pts_name , sizeof(pts_name) );
	int fds;
	pid_t pid;
	if( fdm < 0 )
		return -__LINE__;
	if( slave_name != NULL )
	{
		strncpy( slave_name , pts_name , slave_namesz );
		slave_name[slave_namesz - 1] = 0;
	}
	if( ( pid = fork() ) < 0  )
	{
		return -__LINE__;
	}
	else if( pid == 0 )
	{
		if(setsid()<0)
			return -__LINE__;
		if( (fds = ptys_open(pts_name)) < 0 )
			return -__LINE__;
		close(fdm);
		if( ioctl( fds , TIOCSCTTY , (char*)0 ) < 0 )
			return -__LINE__;
		if( slave_termios )
		{
			if( tcsetattr( fds , TCSANOW ,slave_termios) < 0 )
				return -__LINE__;
		}
		if( slave_winsize )
		{
			if( ioctl( fds , TIOCSWINSZ , slave_winsize ) < 0 )
				return -__LINE__;
		}
		if( dup2( fds , STDIN_FILENO ) < 0 ) return -__LINE__;
		if( dup2( fds , STDOUT_FILENO) < 0 ) return -__LINE__;
		if( dup2( fds , STDERR_FILENO) < 0 ) return -__LINE__;
		if( fds != STDIN_FILENO && fds != STDOUT_FILENO && fds != STDERR_FILENO )
			close(fds);
		return 0;
		
	}
	else
	{
		*ptrfdm = fdm;
		return pid;
	}
}
#endif
int tty_raw(int fd)
{
	int	err;
	struct termios	buf;

	if (tcgetattr(fd, &buf) < 0)
		return(-1);
	struct termios save_termios = buf;	/* structure copy */
	buf.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	buf.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	buf.c_cflag &= ~(CSIZE | PARENB);
	buf.c_cflag |= CS8;
	buf.c_oflag &= ~(OPOST);
	buf.c_cc[VMIN] = 1;
	buf.c_cc[VTIME] = 0;
	if (tcsetattr(fd, TCSAFLUSH, &buf) < 0)
		return(-1);
	if (tcgetattr(fd, &buf) < 0) {
		err = errno;
		tcsetattr(fd, TCSAFLUSH, &save_termios);
		errno = err;
		return(-1);
	}
	if ((buf.c_lflag & (ECHO | ICANON | IEXTEN | ISIG)) ||
			(buf.c_iflag & (BRKINT | ICRNL | INPCK | ISTRIP | IXON)) ||
			(buf.c_cflag & (CSIZE | PARENB | CS8)) != CS8 ||
			(buf.c_oflag & OPOST) || buf.c_cc[VMIN] != 1 ||
			buf.c_cc[VTIME] != 0) {
		tcsetattr(fd, TCSAFLUSH, &save_termios);
		errno = EINVAL;
		return(-1);
	}

	return(0);
}

