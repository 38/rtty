#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "protocol.h"
#define PROT_NCCS 19
#define PL printf("%d\n",__LINE__);
const char* key = "习近平我肏你妈了个屄";
size_t read_encoded(int fd, void* buf, size_t size)
{
	static int position = 0;
	size_t ret = read(fd, buf, size);
	int i;
	for(i = 0; i < ret; i ++)
	{
		((char*)buf)[i] ^= key[position++];
		if(key[position] == 0) position = 0;
	}
}
size_t write_encoded(int fd, const void* buf, size_t size)
{
	static int position = 0;
	static char* send_buf = NULL;
	static size_t buf_size = 0;
	if(send_buf == NULL) send_buf = (char*)malloc(1024), buf_size = 1024;
	if(buf_size < size)
	{
		send_buf = realloc(send_buf, size * 2);
		buf_size = size * 2;
	}
	int i;
	for(i = 0; i < size; i ++)
	{
		send_buf[i] = (((const char*)buf)[i]) ^ key[position++];
		if(key[position] == 0) position = 0;
	}
	return write(fd, send_buf, size); 
}
size_t read_data( int fd , void * buf , size_t size )
{
	size_t sr = 0;
	
	while( sr < size )
	{
		size_t ret = read_encoded( fd , ((char*)buf)+sr , size - sr );
		if( ret == 0 ) break;
		sr += ret;
	}
	return sr;	
}
size_t read_termios( int fd , struct termios* buf )
{
	read_data( fd , buf , 32 );
	int i;
	for( i = 0 ; i < PROT_NCCS ; i ++ )
		read_data( fd , buf->c_cc + i , sizeof(cc_t) );
	return 32 + PROT_NCCS;
}
size_t write_termios( int fd , const struct termios *buf )
{
	write_encoded( fd , buf , 32 );
	int i;
	for( i = 0 ; i < PROT_NCCS ; i ++ )
		write_encoded( fd , buf->c_cc + i , 1 );
	return 32 + PROT_NCCS;
}
size_t read_vdata( int fd , char **buf)
{
	unsigned int sz;
	size_t ret = 0;
	read_data( fd , &sz , sizeof(sz) );
	*buf = (char*)malloc(sz);
	ret += read_data( fd , *buf , sz );
	return ret;
}
size_t write_vdata( int fd , char *buf , unsigned int size)
{
	size_t ret = write_encoded( fd , &size , sizeof(unsigned int) );
	ret += write_encoded( fd , buf , size );
	return ret;
}
size_t read_concmd( int fd , struct concmd *buf )
{
	size_t ret = 0;
	ret += read_data( fd , buf , sizeof(struct concmd) );
	return ret;
}
size_t write_concmd( int fd , struct concmd *buf )
{
	return write_encoded( fd , buf , sizeof(struct concmd) );
}

size_t read_shellparm( int fd , struct shellparm *buf )
{
	size_t ret = 0;
	ret += read_vdata( fd , &buf->sp_exec );
	ret += read_data( fd , &buf->sp_argc , sizeof(int) );
	int i;
	for( i = 0 ; i < buf->sp_argc ; i ++ )
	{
		ret += read_vdata( fd , &buf->sp_argv[i] );
	}
	return ret;
}
size_t write_shellparm( int fd , struct shellparm *buf )
{
	size_t ret = 0;
	ret += write_vdata( fd , buf->sp_exec , strlen(buf->sp_exec) + 1 );
	ret += write_encoded( fd , &buf->sp_argc , sizeof(int)  );
	int i;
	for( i = 0 ; i < buf->sp_argc ; i ++ )
	{
		ret += write_vdata( fd , buf->sp_argv[i] , strlen(buf->sp_argv[i]) + 1);
	}
	return ret;
}
size_t read_pushparm(int fd , struct pushparm* buf )
{
	size_t ret = 0;
	ret += read_vdata( fd , &buf->psp_basename );
	ret += read_vdata( fd , &buf->psp_path );
	ret += read_data( fd , &buf->psp_mode , sizeof(int) );
	ret += read_data( fd , &buf->psp_size , sizeof(unsigned int));
	return ret;
}
size_t write_pushpram(int fd , struct pushparm* buf)
{
	size_t ret = 0;
	ret += write_vdata( fd , buf->psp_basename , strlen(buf->psp_basename) + 1 );
	ret += write_vdata( fd , buf->psp_path , strlen(buf->psp_path)+1 );
	ret += write_encoded( fd , &buf->psp_mode , sizeof(int) );
	ret += write_encoded( fd , &buf->psp_size , sizeof(unsigned int) );
	return ret;
}
size_t read_iopacket(int fd , struct iopacket* buf )
{
	size_t ret = 0;
	ret += read_data( fd , &buf->ip_type , sizeof(unsigned int) );
	buf->ip_len = read_vdata( fd , (char**)&buf->ip_data );
	ret+= buf->ip_len;
	return ret;
}
size_t write_iopacket(int fd , struct iopacket* buf )
{
	size_t ret = 0;
	ret += write_encoded( fd , &buf->ip_type , sizeof(unsigned int) );
	ret += write_vdata( fd , buf->ip_data , buf->ip_len );
	return ret;
}
