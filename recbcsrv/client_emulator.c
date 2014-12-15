/*
 * client_emulator.c
 *
 *  Created on: Apr 1, 2013
 *      Author: spark
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "recbc_srv.h"

#define debug_printf(x...) printf(x)

#define COMMAND_HELLO "0001"
#define COMMAND_SERVER "ffff"
#define COMMAND_CONFIG "0006"
#define COMMAND_SWITCHCTRL "0008"
#define COMMAND_GETDATA "0010"
#define COMMAND_GETDATA_ACK "0011"
#define COMMAND_EVENT_VID "0021"
#define COMMAND_EVENT_VGA "0023"
#define COMMAND_EVENT_AUX "0025"
#define COMMAND_CAMHLD_BYPASS "0012"
#define COMMAND_BYP_RSP "0013"
#define COMMAND_EVENT_UCI "0027"
#define COMMAND_EVENT_JPEG "0029"
#define COMMAND_EVENT_PIP "0031"
#define COMMAND_INTERNAL "0000"

#define TYPE_VIDEO 0
#define TYPE_AUDIO 1
//functions
static int char2hex(char *src, unsigned int *hex, int len)
{
	if(len > 8)
	{
		debug_printf("the length of char is too large\n");
		return -1;
	}
	int i,j;
	*hex = 0;
	for(i = 0;i < len; i++)
	{
        j = len - i - 1;
		*hex &= (~(0xf << (j*4)));
		if((src[i]>='0') && (src[i]<='9'))
		{
			*hex |= (src[i]-0x30) << (j*4);
		}
		else if((src[i]>='a') && (src[i]<='z'))
		{
			*hex |= (src[i]-0x57) << (j*4);
		}
		else if((src[i]>='A') && (src[i]<='Z'))
		{
			*hex |= (src[i]-0x37) << (j*4);
		}
	}
//	debug_printf("get char:%s h:%x\n", src, *hex);
	return 1;
}

static void add_attr_char(t_cmd *cmd, char *name, char *value)
{
	char datalen[DATA_LEN_SIZE+1];
    sprintf(cmd->cmd_p, "%01x%04x%s%s", strlen(name), strlen(value), name, value);
    //debug_printf("attr: %s\n", cmd->cmd_p);
    cmd->cmd_p += (ARG_NAME_LEN_SIZE + ARG_LEN_SIZE + strlen(name) + strlen(value));
    sprintf(datalen, "%04x", cmd->cmd_p-cmd->data);
    memcpy(cmd->data_len, datalen, DATA_LEN_SIZE);
}

static void add_attr_int(t_cmd *cmd,  char *name, unsigned int value)
{
	char c[10];
	memset(c, 0, sizeof(c));
	sprintf(c, "%x",value);
	add_attr_char(cmd, name, c);
}

static void add_attr_bin(t_cmd *cmd, char *name, char *value, unsigned int valuelen)
{
	char datalen[DATA_LEN_SIZE+1];
    sprintf(cmd->cmd_p, "%01x%04x%s", strlen(name), valuelen, name);
    //debug_printf("attr: %s\n", cmd->cmd_p);
    cmd->cmd_p += (ARG_NAME_LEN_SIZE + ARG_LEN_SIZE + strlen(name));
    memcpy(cmd->cmd_p, value, valuelen);
    cmd->cmd_p += valuelen;
    sprintf(datalen, "%04x", cmd->cmd_p-cmd->data);
    memcpy(cmd->data_len, datalen, DATA_LEN_SIZE);
}

static void fill_cmd_header(t_cmd *cmd, char *cmd_p)
{
	cmd->version = cmd->fullbuf;
	cmd->cmd = &cmd->fullbuf[2];
	cmd->data_len = &cmd->fullbuf[6];
	cmd->data = &cmd->fullbuf[10];
	cmd->cmd_p =cmd->data;
	memset(cmd, 0, sizeof(cmd));
	memcpy(cmd->version, COMMAND_VERSION, VERSION_SIZE);
	memcpy(cmd->cmd, cmd_p, CMD_SIZE);
}

//This function is usually for debug purpose
void MsgDump(char *msg)
{
	char buf[CMD_BUFSIZE];
	unsigned int len;
	char *ptr, *version, *command, *data_len;

	debug_printf("\n");
	debug_printf("******************************************************************\n");
	//debug_printf("\n");
	version = msg;
	command = &msg[2];
	data_len = &msg[6];
	ptr = &msg[10];

	memcpy(buf, version, VERSION_SIZE);
	buf[VERSION_SIZE]='\0';
	if(strcmp(buf, "01")){
		debug_printf("Wrong msg format\n");
		return;
	}
	debug_printf("Msg version: %s\n", buf);

	memcpy(buf, command, CMD_SIZE);
	buf[CMD_SIZE] = '\0';
	debug_printf("Command: %s\n", buf);

	char2hex(data_len, &len, DATA_LEN_SIZE);
	debug_printf("Total length of attributes: %d(%x)\n", len, len);

	while(len){
		unsigned int name_len, value_len;

		char2hex(ptr, &name_len, 1);
		ptr++;

		char2hex(ptr, &value_len, 4);
		ptr += 4;

		memcpy(buf, ptr, name_len);
		buf[name_len] = '\0';
		debug_printf("%s(in length of %d): ", buf, name_len);
		ptr += name_len;

		memcpy(buf, ptr, value_len);
		buf[value_len] = '\0';
		ptr += value_len;
		debug_printf("%s(in length of %d)\n", buf, value_len);

		len -= (ARG_NAME_LEN_SIZE + ARG_LEN_SIZE + name_len + value_len);
	}
	//debug_printf("\n");
	debug_printf("******************************************************************\n");
	debug_printf("\n");
}

//Retrieve a complete message from sock and put it in the buffer
#if 0
int getMsg(int sock, char* buf)
{
	int ret;
	unsigned len;

	ret = recv(sock, buf, 10, MSG_PEEK);

	if(ret<=0){
		return -1;
	}

	if(ret<10){
		return 0;
	}

	char2hex(&buf[6], &len, DATA_LEN_SIZE);
	len += PACKET_HEADER_SIZE;

	ret = recv(sock, buf, len, MSG_PEEK);

	if(ret<len){
		return 0;
	}

	recv(sock, buf, len, 0);

	return len;
}
#else
int getMsg(int sock, char* buf, int *number_in_buf)
{
	int ret;
	unsigned len;

	if(*number_in_buf<10){
		int nbuffered = *number_in_buf;
		int ntoread = 10-nbuffered;
		ret = recv(sock, &buf[nbuffered], ntoread, 0);
		if(ret<=0){
			*number_in_buf = 0;
			return -1;
		}
		else{
			*number_in_buf+=ret;
		}
	}

	if(*number_in_buf>=10){
		char2hex(&buf[6], &len, DATA_LEN_SIZE);
		len += PACKET_HEADER_SIZE;
		int nbuffered = *number_in_buf;
		int ntoread = len-nbuffered;
		ret = recv(sock, &buf[nbuffered], ntoread, 0);
		if(ret<=0){
			*number_in_buf = 0;
			return -1;
		}
		else{
			*number_in_buf+=ret;
		}
	}

	if(*number_in_buf<10){
		return 0;
	}
	else{
		char2hex(&buf[6], &len, DATA_LEN_SIZE);
		len += PACKET_HEADER_SIZE;
		if(*number_in_buf<len){
			return 0;
		}
		else{
			*number_in_buf = 0;
			return len;
		}
	}
}
#endif


void EventVGA(int change, int sock)
{
	t_cmd cmd;
	size_t size;
	fill_cmd_header(&cmd, COMMAND_EVENT_VGA);
	add_attr_int(&cmd, "Changed", change);
	debug_printf("Msg: %s\n", cmd.fullbuf);
	//debug_printf("Msg: %s\n", cmd.data);
	char2hex(cmd.data_len, &size, DATA_LEN_SIZE);
	size += PACKET_HEADER_SIZE;
	send(sock, cmd.fullbuf, size, 0);
	debug_printf("EventVGA msg sent\n");
}

void EventVid(int number, int sock)
{
	t_cmd cmd;
	size_t size;
	fill_cmd_header(&cmd, COMMAND_EVENT_VID);
	add_attr_int(&cmd, "Detected", number);

	if(number){
		add_attr_int(&cmd, "AvgTop", 36);
		add_attr_int(&cmd, "AvgLeft", 100);
		add_attr_int(&cmd, "AvgBottom", 108);
		add_attr_int(&cmd, "AvgRight", 152);
	}

	if(number==2){
		add_attr_int(&cmd, "MaxTop", 36);
		add_attr_int(&cmd, "MaxLeft", 48);
		add_attr_int(&cmd, "MaxBottom", 108);
		add_attr_int(&cmd, "MaxRight", 204);
	}
	debug_printf("Msg: %s\n", cmd.fullbuf);
	//debug_printf("Msg: %s\n", cmd.data);
	char2hex(cmd.data_len, &size, DATA_LEN_SIZE);
	size += PACKET_HEADER_SIZE;
	send(sock, cmd.fullbuf, size, 0);
	debug_printf("EventVid msg sent\n");
}

void EventAux(int number, int sock)
{
	t_cmd cmd;
	size_t size;
	fill_cmd_header(&cmd, COMMAND_EVENT_AUX);

    add_attr_int(&cmd, "Channel", 0);
    add_attr_int(&cmd, "Targets", number);
    if(!number){
        add_attr_int(&cmd, "MaxTop", 0);
        add_attr_int(&cmd, "MaxLeft", 0);
        add_attr_int(&cmd, "MaxBottom", 0);
        add_attr_int(&cmd, "MaxRight", 0);
    }
    else if(number==1){
        add_attr_int(&cmd, "MaxTop", 72);
        add_attr_int(&cmd, "MaxLeft", 420);
        add_attr_int(&cmd, "MaxBottom", 144);
        add_attr_int(&cmd, "MaxRight", 420);
    }
    else{
        add_attr_int(&cmd, "MaxTop", 72);
        add_attr_int(&cmd, "MaxLeft", 320);
        add_attr_int(&cmd, "MaxBottom", 144);
        add_attr_int(&cmd, "MaxRight", 420);
    }

    add_attr_int(&cmd, "Channel", 1);
    add_attr_int(&cmd, "Targets", number);
    if(!number){
        add_attr_int(&cmd, "MaxTop", 0);
        add_attr_int(&cmd, "MaxLeft", 0);
        add_attr_int(&cmd, "MaxBottom", 0);
        add_attr_int(&cmd, "MaxRight", 0);
    }
    else if(number==1){
        add_attr_int(&cmd, "MaxTop", 72);
        add_attr_int(&cmd, "MaxLeft", 360);
        add_attr_int(&cmd, "MaxBottom", 144);
        add_attr_int(&cmd, "MaxRight", 360);
    }
    else{
        add_attr_int(&cmd, "MaxTop", 72);
        add_attr_int(&cmd, "MaxLeft", 360);
        add_attr_int(&cmd, "MaxBottom", 144);
        add_attr_int(&cmd, "MaxRight", 440);
    }

	debug_printf("Msg: %s\n", cmd.fullbuf);
	//debug_printf("Msg: %s\n", cmd.data);
	char2hex(cmd.data_len, &size, DATA_LEN_SIZE);
	size += PACKET_HEADER_SIZE;
	send(sock, cmd.fullbuf, size, 0);
	debug_printf("EventVid msg sent\n");
}

void CamhldRsp(char *byte, unsigned int len, int sock)
{
	t_cmd cmd;
	size_t size;

	fill_cmd_header(&cmd, COMMAND_BYP_RSP);
	add_attr_int(&cmd, "Channel", 0);
	add_attr_bin(&cmd, "RawBytes", byte, len);
	debug_printf("Msg: %s\n", cmd.fullbuf);
	//debug_printf("Msg: %s\n", cmd.data);
	char2hex(cmd.data_len, &size, DATA_LEN_SIZE);
	size += PACKET_HEADER_SIZE;
	send(sock, cmd.fullbuf, size, 0);
	debug_printf("EventVid msg sent\n");
}

void CamhldAck(int sock)
{
	char byte[] = {0x90, 0x41, 0xff};
	CamhldRsp(byte, 3, sock);
}

void CamhldCpl(int sock)
{
	char byte[] = {0x90, 0x51, 0xff};
	CamhldRsp(byte, 3, sock);
}


int main(int argc, char **argv)
{
	int port = 2000;
	int sock_client;
	//int len = 0;
	struct sockaddr_in cli;
	fd_set fdsr;
	//struct timeval tv;
	int ret;

	if(argc!=2){
		debug_printf("use: %s IP_addr\n", argv[0]);
		return -1;
	}

	cli.sin_family = AF_INET;
	cli.sin_port = htons(port);
	cli.sin_addr.s_addr = inet_addr(argv[1]);

	sock_client = socket(AF_INET, SOCK_STREAM, 0);
	if(sock_client<0){
		debug_printf("open socket error\n");
		return -1;
	}

	debug_printf("connecting to server...");

	if(connect(sock_client, (struct sockaddr*)&cli, sizeof(cli))<0){
		debug_printf("connect error\n");
		return -1;
	}
	debug_printf("connected\n");

	char buf[CMD_BUFSIZE];
	int number_in_buf=0;

	while(1){

		printf("Command:\n");
		printf("[0] Exit program\n");
		printf("[1] VGAUNCHG\n");
		printf("[2] VGACHG\n");
		printf("[3] TEACHER_NONE\n");
		printf("[4] TEACHER_ONE\n");
		printf("[5] TEACHER_MULTIPLE\n");
		printf("[6] STUDENT_NONE\n");
		printf("[7] STUDENT_ONE\n");
		printf("[8] STUDENT_MULTIPLE\n");
		printf("[9] CAM FEED BACK\n");

		FD_ZERO(&fdsr);
		FD_SET(sock_client, &fdsr);
		FD_SET(0, &fdsr);

		int maxfd = (sock_client>0)?sock_client:0;
		//int maxfd = 0;

		//tv.tv_sec = 1;
		//tv.tv_usec = 0;

		ret = select(maxfd+1, &fdsr, NULL, NULL, NULL);

		if(ret<0){
			debug_printf("select error\n");
			exit(1);
		}

		if(ret==0){
			continue;
		}

		if(FD_ISSET(sock_client, &fdsr)){
			debug_printf("socket readable\n");
			//char buf[CMD_BUFSIZE];
			int MsgGot;

			MsgGot = getMsg(sock_client, buf, &number_in_buf);

			if(MsgGot<0){
				debug_printf("socket recv error\n");
				close(sock_client);
				sock_client = 0;
				//continue;
				return -1;
			}

			if(MsgGot == 0){
				debug_printf("uncompleted message\n");
			}

			MsgDump(buf);
		}


		if(FD_ISSET(0, &fdsr)){
			//debug_printf("user select input\n");
			char sbuf[10];
			fgets(sbuf, 10, stdin);
			sbuf[strlen(sbuf)-1]='\0';

			if(!strcmp(sbuf, "0")){
				if(sock_client){
					close(sock_client);
				}
				return 0;
			}

			if(!strcmp(sbuf, "1")){
				EventVGA(0, sock_client);
			}

			if(!strcmp(sbuf, "2")){
				EventVGA(1, sock_client);
			}

			if(!strcmp(sbuf, "3")){
				EventVid(0, sock_client);
			}

			if(!strcmp(sbuf, "4")){
				EventVid(1, sock_client);
			}

			if(!strcmp(sbuf, "5")){
				EventVid(2, sock_client);
			}

			if(!strcmp(sbuf, "6")){
				EventAux(0, sock_client);
			}

			if(!strcmp(sbuf, "7")){
				EventAux(1, sock_client);
			}

			if(!strcmp(sbuf, "8")){
				EventAux(2, sock_client);
			}

			if(!strcmp(sbuf, "9")){
				CamhldAck(sock_client);
				CamhldCpl(sock_client);
			}
		}
	}

	return 0;
}

