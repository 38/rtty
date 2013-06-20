#ifndef __HEADER_PROTOCOL
#define __HEADER_PROTOCOL
#include <termios.h>
#define SHELL 0
#define PUSH  1
#define PULL  2
#define PROT_NCCS 19
#define CONFLAG 0x12345678
struct concmd
{
	int cc_flag ;
	int cc_cdword;
	char cc_signature[20];
};
//SHELL param
struct shellparm
{
	char*  sp_exec;
	int   sp_argc;
	char*  sp_argv[255];
};
//PUSH parm
struct pushparm
{
	char* psp_basename;
	char* psp_path;
	int   psp_mode;
	unsigned int psp_size;
};
//PULL return
struct pullret
{
	unsigned int pr_size;
	unsigned int pr_mode;
};
//IO Packet
#define IPT_DATA  0
#define IPT_WINCH 1
struct iopacket
{
	unsigned int ip_type;
	unsigned int ip_len;
	void* ip_data;
};
size_t read_data( int fd , void * buf , size_t size );
size_t read_termios( int fd , struct termios *buf );
size_t write_termios( int fd , const struct termios *buf );
size_t read_vdata( int fd , char** buf  );
size_t write_vdata( int fd ,char* buf  , unsigned int sz);
size_t read_concmd( int fd , struct concmd* buf);
size_t write_concmd(int fd , struct concmd* buf);
size_t read_shellparm(int fd , struct shellparm* buf);
size_t write_shellparm(int fd, struct shellparm* buf);
size_t read_pushparm(int fd , struct pushparm* buf );
size_t write_pushpram(int fd , struct pushparm* buf);
size_t read_iopacket( int fd , struct iopacket* buf );
size_t write_iopacket(int fd , struct iopacket* buf );
#endif
