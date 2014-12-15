/*
 * server_emulator.c
 *
 *  Created on: Apr 7, 2013
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
#include <pthread.h>

#include "recbc_srv.h"

#define debug_printf(x...) printf(x)
#define COMMAND_SERVER "ffff"
#define COMMAND_HELLO "0001"
//#define COMMAND_SERVER "ffff"
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
	debug_printf("\n");
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
	debug_printf("\n");
	debug_printf("******************************************************************\n");
	debug_printf("\n");
}

//Retrieve a complete message from sock and put it in the buffer
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

void start(int sock)
{
	t_cmd cmd;
	size_t size;
	fill_cmd_header(&cmd, COMMAND_SERVER);
	add_attr_int(&cmd, "Start", 1);
	debug_printf("Msg: %s\n", cmd.fullbuf);
	char2hex(cmd.data_len, &size, DATA_LEN_SIZE);
	size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
	send(sock, cmd.fullbuf, size, 0);
	debug_printf("Serve sent start message\n");
}

void stop(int sock)
{
	t_cmd cmd;
	size_t size;
	fill_cmd_header(&cmd, COMMAND_SERVER);
	add_attr_int(&cmd, "Stop", 1);
	debug_printf("Msg: %s\n", cmd.fullbuf);
	char2hex(cmd.data_len, &size, DATA_LEN_SIZE);
	size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
	send(sock, cmd.fullbuf, size, 0);
	debug_printf("Serve sent stop message\n");
}

void pauseRecord(int sock)
{
	t_cmd cmd;
	size_t size;
	fill_cmd_header(&cmd, COMMAND_SERVER);
	add_attr_int(&cmd, "Pause", 1);
	debug_printf("Msg: %s\n", cmd.fullbuf);
	char2hex(cmd.data_len, &size, DATA_LEN_SIZE);
	size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
	send(sock, cmd.fullbuf, size, 0);
	debug_printf("Serve sent pause message\n");
}

void resumeRecord(int sock)
{
	t_cmd cmd;
	size_t size;
	fill_cmd_header(&cmd, COMMAND_SERVER);
	add_attr_int(&cmd, "Resume", 1);
	debug_printf("Msg: %s\n", cmd.fullbuf);
	char2hex(cmd.data_len, &size, DATA_LEN_SIZE);
	size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
	send(sock, cmd.fullbuf, size, 0);
	debug_printf("Serve sent resume message\n");
}

void switchtoVGA(int sock)
{
	t_cmd cmd;
	size_t size;
	fill_cmd_header(&cmd, COMMAND_SERVER);
	add_attr_char(&cmd, "SwitchVideo", "VGA");
	debug_printf("Msg: %s\n", cmd.fullbuf);
	char2hex(cmd.data_len, &size, DATA_LEN_SIZE);
	size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
	send(sock, cmd.fullbuf, size, 0);
}

void switchtoTeacher_closeup(int sock)
{
	t_cmd cmd;
	size_t size;
	fill_cmd_header(&cmd, COMMAND_SERVER);
	add_attr_char(&cmd, "SwitchVideo", "TeacherSpecial");
	debug_printf("Msg: %s\n", cmd.fullbuf);
	char2hex(cmd.data_len, &size, DATA_LEN_SIZE);
	size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
	send(sock, cmd.fullbuf, size, 0);
}

void switchtoStudent_closeup(int sock)
{
	t_cmd cmd;
	size_t size;
	fill_cmd_header(&cmd, COMMAND_SERVER);
	add_attr_char(&cmd, "SwitchVideo", "StudentSpecial");
	debug_printf("Msg: %s\n", cmd.fullbuf);
	char2hex(cmd.data_len, &size, DATA_LEN_SIZE);
	size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
	send(sock, cmd.fullbuf, size, 0);
}

void switchtoTeacher_panorama(int sock)
{
	t_cmd cmd;
	size_t size;
	fill_cmd_header(&cmd, COMMAND_SERVER);
	add_attr_char(&cmd, "SwitchVideo", "TeacherFull");
	debug_printf("Msg: %s\n", cmd.fullbuf);
	char2hex(cmd.data_len, &size, DATA_LEN_SIZE);
	size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
	send(sock, cmd.fullbuf, size, 0);
}

void switchtoStudent_panorama(int sock)
{
	t_cmd cmd;
	size_t size;
	fill_cmd_header(&cmd, COMMAND_SERVER);
	add_attr_char(&cmd, "SwitchVideo", "StudentFull");
	debug_printf("Msg: %s\n", cmd.fullbuf);
	char2hex(cmd.data_len, &size, DATA_LEN_SIZE);
	size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
	send(sock, cmd.fullbuf, size, 0);
}

void switchtoBlackboard_closeup(int sock)
{
	t_cmd cmd;
	size_t size;
	fill_cmd_header(&cmd, COMMAND_SERVER);
	add_attr_char(&cmd, "SwitchVideo", "Blackboard");
	debug_printf("Msg: %s\n", cmd.fullbuf);
	char2hex(cmd.data_len, &size, DATA_LEN_SIZE);
	size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
	send(sock, cmd.fullbuf, size, 0);
}

void switchtoVGAwithTeacher_closeup(int sock)
{
	t_cmd cmd;
	size_t size;
	fill_cmd_header(&cmd, COMMAND_SERVER);
	add_attr_char(&cmd, "SwitchVideo", "VGAWithTeacherSpecial");
	debug_printf("Msg: %s\n", cmd.fullbuf);
	char2hex(cmd.data_len, &size, DATA_LEN_SIZE);
	size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
	send(sock, cmd.fullbuf, size, 0);
}

void switchtoTeacherSpecialwithStudentSpecial(int sock)
{
	t_cmd cmd;
	size_t size;
	fill_cmd_header(&cmd, COMMAND_SERVER);
	add_attr_char(&cmd, "SwitchVideo", "TeacherSpecialWithStudentSpecial");
	debug_printf("Msg: %s\n", cmd.fullbuf);
	char2hex(cmd.data_len, &size, DATA_LEN_SIZE);
	size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
	send(sock, cmd.fullbuf, size, 0);
}

void SetRecordMode(int mode, int sock)
{
	t_cmd cmd;
	size_t size;
	fill_cmd_header(&cmd, COMMAND_SERVER);

	switch(mode){
	case 0:
		add_attr_char(&cmd, "SetRecordMode", "Manual");
		break;
	case 1:
		add_attr_char(&cmd, "SetRecordMode", "HalfAuto");
		break;
	case 2:
		add_attr_char(&cmd, "SetRecordMode", "Auto");
		break;
	}

	debug_printf("Msg: %s\n", cmd.fullbuf);
	char2hex(cmd.data_len, &size, DATA_LEN_SIZE);
	size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
	send(sock, cmd.fullbuf, size, 0);
}

void CamZoom(int direction, int who, int sock)
{
	t_cmd cmd;
	size_t size;
	fill_cmd_header(&cmd, COMMAND_SERVER);

	switch(direction){
	case 0:
		add_attr_char(&cmd, "CamZoom", "Wide");
		break;
	case 1:
		add_attr_char(&cmd, "CamZoom", "Tele");
		break;
	}

	switch(who){
	case 0:
		add_attr_char(&cmd, "Camera", "TeacherSpecial");
		break;
	case 1:
		add_attr_char(&cmd, "Camera", "StudentSpecial");
		break;
	}

	debug_printf("Msg: %s\n", cmd.fullbuf);
	char2hex(cmd.data_len, &size, DATA_LEN_SIZE);
	size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
	send(sock, cmd.fullbuf, size, 0);
}

void CamPanTilt(int direction, int who, int sock)
{
	t_cmd cmd;
	size_t size;
	fill_cmd_header(&cmd, COMMAND_SERVER);

	switch(direction){
	case 0:
		add_attr_char(&cmd, "CamPanTilt", "Left");
		break;
	case 1:
		add_attr_char(&cmd, "CamPanTilt", "Right");
		break;
	case 2:
		add_attr_char(&cmd, "CamPanTilt", "Up");
		break;
	case 3:
		add_attr_char(&cmd, "CamPanTilt", "Down");
		break;
	}

	switch(who){
	case 0:
		add_attr_char(&cmd, "Camera", "TeacherSpecial");
		break;
	case 1:
		add_attr_char(&cmd, "Camera", "StudentSpecial");
		break;
	}

	debug_printf("Msg: %s\n", cmd.fullbuf);
	char2hex(cmd.data_len, &size, DATA_LEN_SIZE);
	size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
	send(sock, cmd.fullbuf, size, 0);
}

void CamMemoryInq(int number, int who, int sock)
{
	t_cmd cmd;
	size_t size;
	fill_cmd_header(&cmd, COMMAND_SERVER);
	add_attr_int(&cmd, "CamMemoryInq", number);

	switch(who){
	case 0:
		add_attr_char(&cmd, "Camera", "TeacherSpecial");
		break;
	case 1:
		add_attr_char(&cmd, "Camera", "StudentSpecial");
		break;
	}

	debug_printf("Msg: %s\n", cmd.fullbuf);
	char2hex(cmd.data_len, &size, DATA_LEN_SIZE);
	size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
	send(sock, cmd.fullbuf, size, 0);
}

void CamMemorySave(int number, int who, int sock)
{
	t_cmd cmd;
	size_t size;
	fill_cmd_header(&cmd, COMMAND_SERVER);
	add_attr_int(&cmd, "CamMemorySave", number);

	switch(who){
	case 0:
		add_attr_char(&cmd, "Camera", "TeacherSpecial");
		break;
	case 1:
		add_attr_char(&cmd, "Camera", "StudentSpecial");
		break;
	}

	debug_printf("Msg: %s\n", cmd.fullbuf);
	char2hex(cmd.data_len, &size, DATA_LEN_SIZE);
	size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
	send(sock, cmd.fullbuf, size, 0);
}

int jpeg_cnt = 0;
FILE *jpeg_fd = NULL;

void MsgHandle(char *Msg)
{


	char cmd[CMD_SIZE+1];
	unsigned int len;
	char *ptr;

	memcpy(cmd, &Msg[VERSION_SIZE], CMD_SIZE);
	cmd[CMD_SIZE] = '\0';

	char2hex(&Msg[VERSION_SIZE+CMD_SIZE], &len, DATA_LEN_SIZE);
	//debug_printf("Total length of attributes: %d(%x)\n", len, len);

	ptr = &Msg[PACKET_HEADER_SIZE];

	if(!strcmp(cmd, COMMAND_EVENT_JPEG)){
		unsigned int channel = 0;
		while(len){

			unsigned int name_len, value_len;
			char attr_name[16];
			char attr_value[1536];
			t_frame_info frame_info;

			char2hex(ptr, &name_len, 1);
			ptr++;

			char2hex(ptr, &value_len, 4);
			ptr += 4;

			memcpy(attr_name, ptr, name_len);
			attr_name[name_len] = '\0';
			ptr += name_len;

			memcpy(attr_value, ptr, value_len);
			attr_value[value_len] = '\0';
			ptr += value_len;

			len -= (ARG_NAME_LEN_SIZE + ARG_LEN_SIZE + name_len + value_len);

			if(!strcmp(attr_name, "Info")){
				//debug_printf("attr Info found in length %d\n", value_len);
				//int j;
				//for(j=0; j<value_len; j++){
				//	debug_printf("%02x ", (unsigned char)attr_value[j]);
				//}
				//debug_printf("\n");
				memcpy(&frame_info, attr_value, value_len);
				//unsigned char *frm_info_ptr;
				//frm_info_ptr = (unsigned char *)&frame_info;
				//for(j=0; j<value_len; j++){
					//debug_printf("%02x ", (unsigned char)frm_info_ptr[j]);
				//}
				//debug_printf("\n");
				//debug_printf("frame_info.pkts_in_frame=%d\n", frame_info.pkts_in_frame);
				//debug_printf("frame_info.packet_num=%d\n", frame_info.packet_num);
				//debug_printf("frame_info.type=%d\n", frame_info.frame_type);
				//debug_printf("frame_info.frame_num=%d\n", frame_info.frame_num);
				//debug_printf("frame_info.frame_length=%d\n", frame_info.frame_length);
				//debug_printf("frame_info.timestamp=%d\n", frame_info.time_stamp);
			}

			if(!strcmp(attr_name, "StreamId")){
				char2hex(attr_value, &channel, value_len);
			}

			if(!strcmp(attr_name, "Data")){
				if(channel==0){
					if(jpeg_cnt==0){
						if(!jpeg_fd){
							jpeg_fd = fopen("vga.jpg", "w");
						}
						if(jpeg_fd){
							fwrite(attr_value, 1, value_len, jpeg_fd);
						}
						if(frame_info.packet_num==frame_info.pkts_in_frame-1){
							fclose(jpeg_fd);
							jpeg_fd = NULL;
							jpeg_cnt++;
							debug_printf("one jpeg saved\n");
						}
					}
				}
#if 0
				if(frame_info.packet_num==frame_info.pkts_in_frame-1){
					if(channel==0){
						debug_printf("VGA JPEG received\n");
					}

					if(channel==1){
						debug_printf("TeacherSpecial JPEG received\n");
					}

					if(channel==2){
						debug_printf("StudentSpecial JPEG received\n");
					}

					if(channel==3){
						debug_printf("TeacherPanorama JPEG received\n");
					}

					if(channel==4){
						debug_printf("StudentPanorama JPEG received\n");
					}

					if(channel==5){
						debug_printf("Blackboard JPEG received\n");
					}
				}
#endif
			}
		}
	}
}

int main(int argc, char **argv)
{
	int sock_instructor;
	//struct sockaddr_in server_addr, client_addr;
	struct sockaddr_in cli;
	//socklen_t sin_size = sizeof(cli);
	int opt=1;
	fd_set fdsr;
	//struct timeval tv;
	int ret;

	while(1){
		//open the server socket for listening
		if((sock_instructor=socket(AF_INET, SOCK_STREAM, 0))==-1){
			debug_printf("open socket error\n");
			exit(1);
		}

		debug_printf("socket fd = %d\n", sock_instructor);

		//set socket option
		if(setsockopt(sock_instructor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int))==-1){
			debug_printf("set socket option error\n");
			exit(1);
		}

		memset(&cli, 0, sizeof(cli));
		cli.sin_family = AF_INET;
		cli.sin_port = htons(3000);
		cli.sin_addr.s_addr = inet_addr("127.0.0.1");

		if(connect(sock_instructor, (struct sockaddr*)&cli, sizeof(cli))<0){
			debug_printf("connect error\n");
			close(sock_instructor);
			sleep(3);
		}
		else{
			break;
		}
	}

	debug_printf("connected to recbc server\n");

	//int msg_cnt = 0;
	printf("Command:\n");
	printf("[0] Exit program\n");
	printf("[1] Start\n");
	printf("[2] Stop\n");
	printf("[3] Pause\n");
	printf("[4] Resume\n");
	printf("[5] switch to VGA\n");
	printf("[6] switch to TeacherSpecial\n");
	printf("[7] switch to StudentSpecial\n");
	printf("[8] switch to TeacherFull\n");
	printf("[9] switch to StudentFull\n");
	printf("[A] switch to Blackboard\n");
	printf("[B] switch to VGAWithTeacherSpecial\n");
	printf("[C] switch to TeacherSpecialWithStudentSpecial\n");
	printf("[D] SetRecordMode to manual\n");
	printf("[E] SetRecordMode to semi-auto\n");
	printf("[F] SetRecordMode to auto\n");
	printf("[10] Teacher ZoomTele\n");
	printf("[11] Teacher ZoomWide\n");
	printf("[12] Teacher PanTilt left\n");
	printf("[13] Teacher PanTilt right\n");
	printf("[14] Teacher PanTilt up\n");
	printf("[15] Teacher PanTilt down\n");
	printf("[16] Teacher MemInq\n");
	printf("[17] Teacher MemSave\n");

	while(1){

		FD_ZERO(&fdsr);
		FD_SET(sock_instructor, &fdsr);
		FD_SET(0, &fdsr);

		int maxfd = (sock_instructor>0)?sock_instructor:0;
		//int maxfd = 0;

		//tv.tv_sec = 1;
		//tv.tv_usec = 0;

		ret = select(maxfd+1, &fdsr, NULL, NULL, NULL);

		if(ret<0){
			debug_printf("select error\n");
			exit(1);
		}

		if(ret==0){
			usleep(10000);
			continue;
		}

		if(FD_ISSET(sock_instructor, &fdsr)){
			char buf[CMD_BUFSIZE];
			int MsgGot = getMsg(sock_instructor, buf);

			if(MsgGot<0){
				debug_printf("disconnected to client\n");
				close(sock_instructor);
				break;
			}

			//normal handle
			if(MsgGot == 0){
				//debug_printf("Message from client uncompleted\n");
				usleep(1000);
			}

			if(MsgGot>0){
				//MsgDump(buf);
				//To do: add further processing here
				MsgHandle(buf);
			}
		}

		if(FD_ISSET(0, &fdsr)){
			//debug_printf("user select input\n");
			char sbuf[10];
			fgets(sbuf, 10, stdin);
			sbuf[strlen(sbuf)-1]='\0';

			if(!strcmp(sbuf, "0")){
				if(sock_instructor){
					close(sock_instructor);
					close(sock_instructor);
				}
				return 0;
			}

			if(!strcmp(sbuf, "1")){
				start(sock_instructor);
			}

			if(!strcmp(sbuf, "2")){
				stop(sock_instructor);
			}

			if(!strcmp(sbuf, "3")){
				pauseRecord(sock_instructor);
			}

			if(!strcmp(sbuf, "4")){
				resumeRecord(sock_instructor);
			}

			if(!strcmp(sbuf, "5")){
				switchtoVGA(sock_instructor);
			}

			if(!strcmp(sbuf, "6")){
				switchtoTeacher_closeup(sock_instructor);
			}

			if(!strcmp(sbuf, "7")){
				switchtoStudent_closeup(sock_instructor);
			}

			if(!strcmp(sbuf, "8")){
				switchtoTeacher_panorama(sock_instructor);
			}

			if(!strcmp(sbuf, "9")){
				switchtoStudent_panorama(sock_instructor);
			}

			if(!strcmp(sbuf, "A")){
				switchtoBlackboard_closeup(sock_instructor);
			}

			if(!strcmp(sbuf, "B")){
				switchtoVGAwithTeacher_closeup(sock_instructor);
			}

			if(!strcmp(sbuf, "C")){
				switchtoTeacherSpecialwithStudentSpecial(sock_instructor);
			}

			if(!strcmp(sbuf, "D")){
				SetRecordMode(0, sock_instructor);
			}

			if(!strcmp(sbuf, "E")){
				SetRecordMode(1, sock_instructor);
			}

			if(!strcmp(sbuf, "F")){
				SetRecordMode(2, sock_instructor);
			}

			if(!strcmp(sbuf, "10")){
				CamZoom(1, 0, sock_instructor);
			}

			if(!strcmp(sbuf, "11")){
				CamZoom(0, 0, sock_instructor);
			}

			if(!strcmp(sbuf, "12")){
				CamPanTilt(0, 0, sock_instructor);
			}

			if(!strcmp(sbuf, "13")){
				CamPanTilt(1, 0, sock_instructor);
			}

			if(!strcmp(sbuf, "14")){
				CamPanTilt(2, 0, sock_instructor);
			}

			if(!strcmp(sbuf, "15")){
				CamPanTilt(3, 0, sock_instructor);
			}

			if(!strcmp(sbuf, "16")){
				CamMemoryInq(0, 0, sock_instructor);
			}

			if(!strcmp(sbuf, "17")){
				CamMemorySave(0, 0, sock_instructor);
			}

			printf("Command:\n");
			printf("[0] Exit program\n");
			printf("[1] Start\n");
			printf("[2] Stop\n");
			printf("[3] Pause\n");
			printf("[4] Resume\n");
			printf("[5] switch to VGA\n");
			printf("[6] switch to TeacherSpecial\n");
			printf("[7] switch to StudentSpecial\n");
			printf("[8] switch to TeacherFull\n");
			printf("[9] switch to StudentFull\n");
			printf("[A] switch to Blackboard\n");
			printf("[B] switch to VGAWithTeacherSpecial\n");
			printf("[C] switch to TeacherSpecialWithStudentSpecial\n");
			printf("[D] SetRecordMode to manual\n");
			printf("[E] SetRecordMode to semi-auto\n");
			printf("[F] SetRecordMode to auto\n");
			printf("[10] Teacher ZoomTele\n");
			printf("[11] Teacher ZoomWide\n");
			printf("[12] Teacher PanTilt left\n");
			printf("[13] Teacher PanTilt right\n");
			printf("[14] Teacher PanTilt up\n");
			printf("[15] Teacher PanTilt down\n");
			printf("[16] Teacher MemInq\n");
			printf("[17] Teacher MemSave\n");
		}
	}
	return 0;
}

