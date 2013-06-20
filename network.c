#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
int concnt = 0;
void sigchld(int signo)
{
	wait(NULL);
	concnt --;
}
int open_socket( int port , int qlen )
{
	int reuse = 1;
	int ret = socket( PF_INET , SOCK_STREAM , IPPROTO_IP );
	if(  setsockopt( ret , SOL_SOCKET , SO_REUSEADDR , &reuse , sizeof(int) ) < 0 )
		return -__LINE__;
	if( ret < 0 )
		return -__LINE__;
	struct sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	if( bind( ret , (const struct sockaddr *)&addr , sizeof(addr) ) == -1 )
	{
		close(ret);
		return -__LINE__;
	}
	signal( SIGPIPE , SIG_IGN );
	signal( SIGCHLD , sigchld );
	if( listen( ret , qlen ) < 0 )
	{
		close(ret);
		return -__LINE__;
	
	}
	return ret;
}
typedef void (*proc_on_accept)(int fd,struct sockaddr *addr,socklen_t len);
void loop( int fd , proc_on_accept proc)
{
	for(;;)
	{
		struct sockaddr addr;
		socklen_t  len;
		int clfd = accept( fd , &addr , &len );
		if( concnt >= 10 )
		{
			close(clfd);
			continue;
		}
		if( clfd < 0 )
		{
			perror("Warning");
			close(clfd);
			continue;
		}
		if( fork() > 0 )
		{
			concnt ++;
			close(clfd);
			continue;
		}
		if( proc )proc( clfd , &addr , len );
		close(clfd);
		exit(0);
	}
}
/*void test( int fd , struct sockaddr addr , socklen_t len )
{
	write( fd , "fuckyou\n",8);
}
int main()
{
	int fd = open_socket( 12345 , 10 );
	printf("%d\n",fd);
	loop( fd ,test );

}*/
