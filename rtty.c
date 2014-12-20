#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "pts.h"
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#include "protocol.h"
#include <getopt.h>
#include <stdio.h>
#define RETERR return -__LINE__
struct termios saved_termios;
struct shellparm sp;
char host[20];
int port = -1;
int open_socket( char* host , int port)
{	
	struct sockaddr_in dest = {};
	int sock = socket(AF_INET , SOCK_STREAM , 0 );
	if( sock < 0 )
	{
		perror("socket");
		RETERR;
	}
	dest.sin_family = AF_INET;
	dest.sin_port   = htons(port);
	if( inet_aton( host , (struct in_addr*) &dest.sin_addr.s_addr ) == 0 )
	{
		perror(host);
		RETERR;
	}
	if( connect(sock , (struct sockaddr *)&dest , sizeof(dest) ) != 0 )
	{
		perror("connect");
		RETERR;
	}
	return sock;
}
int sock_fd;
void onwinch( int signo )
{
	struct iopacket ip;
	struct winsize wsz;
	ioctl( STDIN_FILENO , TIOCGWINSZ , wsz );
	ip.ip_type = IPT_WINCH;
	ip.ip_len  = sizeof(unsigned short)*2;
	ip.ip_data = (unsigned short*)&wsz;
	write_iopacket( sock_fd , &ip );
}
void term_loop(int fdm,char* signature)
{
	struct concmd cmd;
	cmd.cc_flag = CONFLAG;
	cmd.cc_cdword = SHELL;
	memcpy( cmd.cc_signature , signature , 20 );
	write_concmd( fdm , &cmd );

	write_shellparm( fdm , &sp );

	struct winsize wsz;
	tcgetattr( STDIN_FILENO , &saved_termios );
	ioctl( STDIN_FILENO , TIOCGWINSZ , &wsz );	
	write_encoded( fdm , &wsz , sizeof(wsz) );
	write_termios( fdm , &saved_termios );
	
	char buf[512];
	tty_raw(STDIN_FILENO);
	pid_t child;

	if( (child = fork()) == 0 )
	{
		sock_fd = fdm;
	        signal( SIGWINCH , onwinch );
		while(1)
		{
			size_t ret = read_encoded( STDIN_FILENO , buf , 512 );
			if(ret == 0 )
				break;
			struct iopacket ip;
			ip.ip_type = IPT_DATA;
			ip.ip_len  = ret;
			ip.ip_data = (void*)buf;
			write_iopacket( fdm , &ip );
		}
		exit(0);
	}
	while(1)
	{
		size_t ret = read_encoded( fdm , buf , 512 );
		if(ret == 0 )
			break;
		write_encoded(STDOUT_FILENO,buf,ret);
	}
	tcsetattr( STDIN_FILENO , TCSAFLUSH , &saved_termios);	
	kill( child , SIGKILL);
}
void print_help()
{
	puts(	"remove tty for android 1.0.0\n");
	puts(	"usage: rtty options action arg1 arg2 ... argN\n");
	puts(	"options:");
	puts(	"if any of the following options is given , the defualt setting will not be used");
	puts(	"-h<hostname>				-specify the hostname of the phone");
	puts(	"-p<port>				-specify the port that rttyd listening");
	puts(	"-s<signatrue>				-specify the signatrue");
	puts(	"");
	puts(	"actions:");
	puts(	"rtty execute <executable> [args]	-execute a executable");
	puts(	"					 NOTE:the execute action dose not exec shell first");
	puts(	"					      shell scripts can not be executed nomally");
	puts(	"					      if you want to exeucte a shell command ,");
	puts(	"					      you should use the action shell");
	puts(	"rtty shell				-execute remote shell");
	puts(	"rtty shell <shell-command>		-execute speicied shell-command");
	puts(	"rtty push <local> <remote>		-copy file to phone");
	puts(	"rtty pull <remote> <local>		-copy file from phone");
	puts(	"rtty install <package-file>		-push the package to phone and install it");
	puts(	"rtty unisntall [-k] <package>		-uninstall specified package");
	puts(	"					 the option -k will keep the data of the pacakge");
	exit(0);
}
void push_file(int fd , char* sour , char* dest , char* signature)
{
	struct concmd cmd;
	cmd.cc_flag = CONFLAG;
	cmd.cc_cdword = PUSH;
	memcpy( cmd.cc_signature , signature , 20 );
        write_concmd( fd , &cmd );
	struct pushparm pp;
	pp.psp_path = dest;
	struct stat st;
	if( lstat( sour , &st ) < 0 )
	{
		perror( "stat" );
		exit(-1);
	}
	if( S_ISDIR(st.st_mode) )
	{
		fputs( "sorry , we could not push a directory yet\n" , stderr );
		exit(-1);
	}
	pp.psp_mode = st.st_mode;
	pp.psp_size = st.st_size;
	char* ptr = sour + strlen(sour);
	while( ptr > sour && *ptr != '/' ) ptr--;
	if(*ptr=='/') ptr ++;
	puts(ptr);
	pp.psp_basename = ptr;
	write_pushpram( fd , &pp );
	int sfd = open( sour , O_RDONLY );
	if( sfd < 0 )
	{
		perror(sour);
		return;
	}
	char buffer[4096];
	size_t sz = st.st_size;
	printf("\033[s");
	int br = -1;
	while(1)
	{
		size_t ret = read_encoded( sfd , buffer , 4096 );
		if( ret == 0 )
			break;
		sz -= ret;
		int rate = 100-sz*100/pp.psp_size;
		if( br != rate )
		{
			printf("\033[u\033[s%d%% completed", br = rate);
			fflush(stdout);
		}
		write_encoded( fd , buffer , ret );
	}
	puts("");
	if( sz != 0 ) puts("File transmitted unsuccessful");
}
void pull_file(int fd , char* sour , char* dest , char* signature)
{
	char path[4096];
	char buffer[4096];
	struct stat st;
	int flag ;
	flag = access( dest , F_OK );
	if( flag == 0 && lstat( dest , &st ) < 0 )
	{
		perror("stat");
		return;
	}
	if( flag == 0 && S_ISDIR(st.st_mode) )
	{
		int i;
		for( i = 0 ; dest[i] ; i ++ )
			path[i] = dest[i];
		if(path[i]!='/') path[i++] = '/';
		//strncpy( path + i , pp.psp_basename , 4096 - i );
		char* ptr = sour + strlen(sour);
		while( ptr >= sour && *ptr != '/' ) ptr--;
		if(*ptr=='/') ptr ++;
		strncpy( path + i , ptr , 4096-i); 
	}
	else
		strncpy( path , dest , 4096 );
	
	int dfd = open( path , O_CREAT | O_WRONLY );
	if( dfd < 0 )
	{
		perror(path);
		return;
	}
	
	struct concmd cmd;
	cmd.cc_flag = CONFLAG;
	cmd.cc_cdword = PULL;
	memcpy( cmd.cc_signature , signature , 20 );
        write_concmd( fd , &cmd );
	write_vdata( fd , sour , strlen(sour) + 1 );
	struct pullret pr;
	read_data( fd , &pr , sizeof(pr));
	size_t sz = pr.pr_size;
	printf("\033[s");
	int br = -1;
	while(sz)
	{
		int ret = read_encoded( fd , buffer , 4096 );
		if( ret == 0 ) break;
		sz -= ret;
		write_encoded( dfd , buffer , ret );
		int rate = 100-sz*100/pr.pr_size;
		if( br != rate )
		{
			printf("\033[u\033[s%d%% completed",br = rate);
			fflush(stdout);
		}

	}
	fchmod( dfd , pr.pr_mode );
	close(dfd);
	puts("");
	if( sz != 0 ) puts("File transmitted unsuccessful");
}
void install_app( int sock , char* package , char* signature)
{
	char * remote_path = "/data/local/tmp/rtty_temp_package.apk";
	push_file( sock , package , remote_path , signature);
	close(sock);
	sock = open_socket( host , port );
	sp.sp_argv[0] = sp.sp_exec = "pm";
	sp.sp_argv[1] = "install";
	sp.sp_argv[2] = remote_path;
	sp.sp_argc = 3;
	term_loop( sock , signature );
	close(sock);
	sock = open_socket( host , port );
	sp.sp_argv[0] = sp.sp_exec = "rm";
	sp.sp_argv[1] = "-f";
	sp.sp_argv[2] = remote_path;
	sp.sp_argc = 3;
	term_loop( sock , signature );
	close(sock);
}

char signature[10];
char exec[4096];
int main(int argc , char** argv)
{
	int opt;
	int cnt = 1;
	while( cnt < argc && argv[cnt][0] == '-' )
		cnt++;
	while( (opt = getopt( cnt , argv , "h::p::s::") ) != -1 )
	{
		switch(opt)
		{
			case 'h':
				strncpy( host , optarg , 20 );
				break;
			case 'p':
				port = atoi(optarg);
				break;
			case 's':
				strncpy( signature , optarg , 20 );
				break;
			case '?':
				print_help();
				exit(0);
		}
	}
	FILE* cfile = fopen("/etc/rtty.cfg","r");
	sp.sp_argv[0] = sp.sp_exec = "sh";
	sp.sp_argv[1] = NULL;
	sp.sp_argc    = 1;
	if( cfile )
	{
		while( !feof(cfile) )
		{
			char key[20] , value[20] ;
			fscanf( cfile , "%s	%s",key,value );
			if( strcmp( key , "default-host") == 0 && !*host )
			{
				strncpy( host , value , 20 );
			}
			if( strcmp( key , "default-port") == 0 && port == -1 )
			{
				port = atoi(value);
			}
			if( strcmp( key , "signature" ) == 0 && !*signature )
			{
				strncpy( signature , value , 20 );
			}
			if( strcmp( key , "default-cmd" ) == 0 )
			{
				sp.sp_exec = exec;
				strncpy( sp.sp_exec , value , 4096 );
				sp.sp_argc = 1;
				sp.sp_argv[0] = sp.sp_exec;
				sp.sp_argv[1] = NULL;
			}	
		}
	}
	if( optind == argc )
	{
		print_help();
		exit(0);
	}
	char* cmd = argv[optind];
	if( strcmp( cmd , "execute" ) == 0 )
	{
		int sock = open_socket( host , port );
		if( optind + 1 < argc )
		{
			sp.sp_exec = exec;
			strncpy( sp.sp_exec , argv[optind+1] , 4096 );	
			sp.sp_argc = argc - optind  - 1;
			sp.sp_argv[0] = sp.sp_exec;
			int i;
			for( i = 2 ; i <= sp.sp_argc ; i ++ )
			{
				sp.sp_argv[i-1] = (char*)malloc(strlen(argv[optind+i])+1);
				strcpy( sp.sp_argv[i-1] , argv[optind+i] );
			}
		}
		term_loop( sock , signature );
	}
	else if( strcmp( cmd , "shell") == 0 )
	{
		if( optind + 1 < argc )
		{
			int i;
			static char buf[3] = "-c";
			static char cmdline[4096]={};
			sp.sp_argv[1] = buf;
			sp.sp_argv[2] = cmdline;
			sp.sp_argc = 3; 
			char * ptr = cmdline;
			for( i = optind + 1 ; i < argc; i ++ )
			{
				char * sp ;
				for( sp = argv[i] ; *sp ; sp++ )
					*ptr++ = *sp;
				*ptr++ = ' ';			
			}
			*ptr = 0;
		}
		int sock = open_socket( host , port );
		if( sock < 0 )
		{
			perror("socket");
			exit(-1);
		}
		term_loop( sock , signature );
	}
	else if( strcmp( cmd , "push" ) == 0 )
	{
		char* sour , *dest;
		if( argc - optind - 1 < 2 )
			print_help();
		sour = argv[optind+1];
		dest = argv[optind+2];
		int sock = open_socket( host , port );
		if( sock < 0 )
		{
			perror("socket");
			exit(-1);
		}
		push_file( sock, sour , dest ,signature);
	}
	else if( strcmp( cmd , "pull" ) == 0 )
	{
		char* sour , *dest;
		if( argc - optind - 1 < 2 )
			print_help();
		sour = argv[optind+1];
		dest = argv[optind+2];
		int sock = open_socket( host , port );
		if( sock < 0 )
		{
			perror("socket");
			exit(-1);
		}
		pull_file( sock, sour , dest ,signature);
	}
	else if( strcmp( cmd , "install" ) == 0 )
	{
		if( argc - optind - 1 < 1 )
			print_help();
		int sock = open_socket( host , port );
		if( sock < 0 )
		{
			perror("socket");
			exit(-1);
		}
		install_app( sock , argv[optind+1] , signature );
	}
	else if( strcmp( cmd , "uninstall" ) == 0 )
	{
		int i;
		if( argc - optind - 1 < 1 )
			print_help();
		int sock = open_socket( host , port );
		sp.sp_argv[0] = sp.sp_exec = "pm";
		sp.sp_argc = 1;
		for( i = optind ; i < argc ; i ++ )
			sp.sp_argv[ sp.sp_argc ++ ] = argv[i];
		term_loop( sock , signature );
	}
	return 0;
}
