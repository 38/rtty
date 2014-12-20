#include "pts.h"
#include "network.h"
#include <signal.h>
#include <sys/wait.h>
#include "protocol.h"
int fdm;
int sockfd;
pid_t epid;//exec pid
pid_t pid; //child pid
int end_flag = 0;
char signature[20];
void sigint(int signo)
{
	//puts("child term");
	close(sockfd);
	exit(0);
}
void sig_chld(int signo)
{
	int info;
	if(wait(&info)==epid)
	{
		printf("terminated with exitcode %d\n",info);
		//puts("exec term");
		end_flag = 1;
		//puts("parent term");
		kill( pid , SIGINT);
		kill( epid , SIGKILL);
		close(fdm);
		close(sockfd);
		exit(0);
	}
}
void do_shell( int fd )
{
	char slave[1000];
	signal( SIGCHLD , sig_chld );
	struct winsize wsz;
	struct termios tio = {};
	struct shellparm sp = {};
	printf("shell command: ");
	read_shellparm( fd , &sp );
	int j;
	for( j = 0 ; j < sp.sp_argc ; j ++ )
		printf("%s ",sp.sp_argv[j]);
	puts("");
	read_data( fd , &wsz , sizeof(wsz) );
	read_termios( fd , &tio );
	epid = pty_fork( &fdm , slave , 1000 , &tio , &wsz); 
	if(epid < 0)
	{
		perror("fork");
		return;
	}
	if( epid == 0 )
	{
		execvp( sp.sp_exec , sp.sp_argv );
		perror("exec");
		exit(1);
	}
	//IO procedure
	size_t sz;
	//int buf[1024];
	struct iopacket ip = {};
	if( ( pid = fork() ) != 0 )
	{/*parent */
		
		while( ( sz = read_iopacket( fd , &ip ) ) && !end_flag )
		{
			if( ip.ip_type == IPT_DATA )
				write_encoded( fdm , ip.ip_data , ip.ip_len );
			else if( ip.ip_type == IPT_WINCH )
			{
				unsigned short *pwsz = (unsigned short*)ip.ip_data;
				struct winsize wsz = {pwsz[0] , pwsz[1]};
				ioctl( fdm , TIOCSWINSZ , &wsz );
				printf("terminal resized\n");
			}
			if(ip.ip_data) free(ip.ip_data);
		}
		//puts("closed");
		kill( pid , SIGINT);
		kill( epid , SIGKILL);
		close(fdm);
		close(fd);
		exit(0);
	}
	signal( SIGINT , sigint );
	sockfd = fd;
	while(1)
	{
		char buf[1024];
		size_t ret = read_encoded( fdm , buf , 1024 );
		if( write_encoded( fd , buf , ret ) != ret )
		{
			perror("Write Error");
			break;
		}
	}
	exit(0);
}
void do_push( int fd )
{
	struct pushparm pp;
	read_pushparm( fd , &pp );
	printf("request to push a file to path ");
	puts(pp.psp_path);
	size_t rsz = pp.psp_size;
	char buffer[4096];
	struct stat st;
	int flag;
	char path[4096];
	flag = access( pp.psp_path , F_OK );
	if( flag == 0  && lstat( pp.psp_path , &st ) < 0 )
	{
		perror("lstat");
		return;
	}
	if( flag == 0 && S_ISDIR( st.st_mode ) )
	{
		int i;
		for( i = 0 ; pp.psp_path[i] ; i ++ )
			path[i] = pp.psp_path[i];
		if(path[i-1]!='/') path[i++] = '/';
		strncpy( path + i , pp.psp_basename , 4096 - i );
	}
	else
		strncpy( path , pp.psp_path , 4096 );
	int dfd = open( path , O_CREAT | O_WRONLY );
	if( dfd < 0 ) 
	{	
		perror(path);
		return;
	}
	while(rsz)
	{
		size_t sz = read_encoded( fd , buffer , 4096 );
		if( sz == 0 )
			break;
		write_encoded( dfd , buffer , sz );
		rsz -= sz;
	}
	fchmod( dfd , pp.psp_mode );
	close(dfd);
}
void do_pull(int fd)
{
	char* path;
	read_vdata( fd , &path );
	printf("sending file %s\n", path );
	struct stat st;
	if( lstat( path , &st ) < 0 )
	{
		perror("stat");
		return;
	}
	if( S_ISDIR(st.st_mode) )
	{
		puts("sorry, we cannot pull a directory yet");
		return;
	}
	struct pullret pr;
	pr.pr_mode = st.st_mode;
	pr.pr_size = st.st_size;
	write_encoded(fd,&pr,sizeof(pr));
	int sfd = open( path , O_RDONLY );
	if( sfd < 0 )
	{
		perror(path);
		return;
	}
	char buffer[4096];
	size_t sz = st.st_size;
	while(1)
	{
		size_t ret = read_encoded( sfd , buffer , 4096 );
		if( ret == 0 )
			break;
		sz -= ret;
		write_encoded( fd , buffer , ret );
	}
	if( sz )
		puts("oh no , something may be wrong ....");
}
void on_accept( int fd , struct sockaddr *addr , socklen_t len )
{
	struct sockaddr_in* add = (struct sockaddr_in*) addr ;
	unsigned char* ch = (unsigned char*)&add->sin_addr.s_addr;
	printf("rttyd : Data from  %d.%d.%d.%d:%d\n",ch[0],ch[1],ch[2],ch[3],add->sin_port);
	struct concmd cmd;
	read_concmd( fd , &cmd );
	if( strcmp( cmd.cc_signature , signature ) != 0 )
		return;
	if( cmd.cc_cdword == SHELL )
		do_shell(fd);
	if( cmd.cc_cdword == PUSH )
		do_push(fd);
	if( cmd.cc_cdword == PULL )
		do_pull(fd);
		//puts("Not implemented yet"); 
}
int port = -1;
int main()
{
	FILE* cfile = fopen("rttyd.cfg","r");
	if( cfile )
	{
		while( !feof(cfile) )
		{
			char key[20] , value[20] ;
			fscanf( cfile , "%s	%s",key,value );
			if( strcmp( key , "signature" ) == 0 && !*signature )
			{
				strncpy( signature , value , 20 );
			}
			else if( strcmp( key , "port" ) == 0 )
			{
				port = atoi( value );
			}
		}
	}
	if(cfile) fclose(cfile);
	if( port == -1 )
	{
		printf("no port number specified\nexiting...");
		exit(-1);
	}
	int sock = open_socket( port , 10 );
	loop( sock , on_accept );
}

