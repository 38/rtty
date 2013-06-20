#ifndef __HEADER_NETWORK
#define __HEADER_NETWORK
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
int open_socket( int port , int qlen );
typedef void (*proc_on_accept)(int fd,struct sockaddr * addr,socklen_t len);
void loop( int fd , proc_on_accept proc);
#endif


