/*
 * recbc_srv.c
 *
 *  Created on: Mar 18, 2013
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
#include <sys/time.h>
#include <math.h>
#include <sys/stat.h>
#include <signal.h>

#include "recbc_srv.h"
#include "datafifo.h"
#include "streaming_srv.h"

#include "PC_CENTRIST/svm.h"
//#include "PC_CENTRIST/common.h"
#include "PC_CENTRIST/DetectFrame.h"

#include "jpeglib.h"
#include <memory.h>

#include "xmlparser.h"

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
#define COMMAND_SET_LED "0014"

#if 0
#define debug_printf(x...) printf(x)
#else
#define debug_printf(x...) do{\
	char m_buf[1024];\
	char m_buf1[1024];\
	time_t m_time;\
	m_time = time(NULL);\
	sprintf(m_buf, "%s", ctime(&m_time));\
	sprintf(m_buf1, x);\
	strcat(m_buf, m_buf1);\
	printf("%s\n", m_buf);\
	}while(0)
#endif

#define SYS_CONFIG_FILE_NAME	"../tomcat/apache-tomcat-6.0.39/webapps/ROOT/conf/sys-config.xml"
#define PTZ_CONFIG_FILE_NAME	"../tomcat/apache-tomcat-6.0.39/webapps/ROOT/conf/ptz-config.xml"
#define	POLICY_FILE_NAME	"../tomcat/apache-tomcat-6.0.39/webapps/ROOT/conf/policy/policy.xml"
//global resources

char CfgDir[256]= "";

const char *MODEL_FILE = "PC_CENTRIST/model_int_s";
const char *FEATURE_FILE = "PC_CENTRIST/FeatureRange";

struct svm_model *model;

int *Feature_Min = NULL;
int *Feature_Max = NULL;


//global varibles
t_video_map g_vmap_table[6]; //VGA is not in this table because its map is fixed
char *g_vmap_file = "./configure/vmap_file";
char *g_config_file = "configure/config_file";
t_recbc_cfg g_cfg;
t_forward_table g_forward_table;

int g_reg_09 = 0;
int g_camholder_still = 1;
int g_analyze_frame_num = 0;
int g_num_in_group = 0;
pthread_mutex_t g_capture_sync_mutex = PTHREAD_MUTEX_INITIALIZER;

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
	memcpy(cmd->data_len, "0000", DATA_LEN_SIZE);
}

//This function is usually for debug purpose
void MsgDump(char *msg)
{
	char buf[CMD_BUFSIZE];
	unsigned int len;
	char *ptr, *version, *command, *data_len;

	printf("\n");
	printf("******************************************************************\n");
	//printf("\n");
	version = msg;
	command = &msg[VERSION_SIZE];
	data_len = &msg[VERSION_SIZE+CMD_SIZE];
	ptr = &msg[PACKET_HEADER_SIZE];

	memcpy(buf, version, VERSION_SIZE);
	buf[VERSION_SIZE]='\0';
	if(strcmp(buf, "01")){
		printf("Wrong msg format\n");
		return;
	}
	printf("Msg version: %s\n", buf);

	memcpy(buf, command, CMD_SIZE);
	buf[CMD_SIZE] = '\0';
	printf("Command: %s\n", buf);

	char2hex(data_len, &len, DATA_LEN_SIZE);
	printf("Total length of attributes: %d(%x)\n", len, len);

	while(len){
		unsigned int name_len, value_len;

		char2hex(ptr, &name_len, 1);
		ptr++;

		char2hex(ptr, &value_len, 4);
		ptr += 4;

		memcpy(buf, ptr, name_len);
		buf[name_len] = '\0';
		printf("%s(in length of %d): ", buf, name_len);
		ptr += name_len;

		memcpy(buf, ptr, value_len);
		buf[value_len] = '\0';
		ptr += value_len;
		printf("%s(in length of %d)\n", buf, value_len);

		len -= (ARG_NAME_LEN_SIZE + ARG_LEN_SIZE + name_len + value_len);
	}
	//printf("\n");
	printf("******************************************************************\n");
	printf("\n");
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


void LoadVideoCfg()
{
	//now we can use default configuration, so do nothing

}

void ParseVideoCfg()
{

}


void thread_videotracking_setmode(t_SwitchTracking_thread_param *p_camhldctrl, int mode)
{

}
#if 0
void * thread_videotracking(void * arg)
{
	//int i;
	t_SwitchTracking_thread_param  *p_camhldctrl = (t_SwitchTracking_thread_param *)arg;
	char RspBuf[16];
	char *RspBuf_p = RspBuf;
	int InqPT_pending = 0;
	int InqZ_pending = 0;
	int AbsPosPending = 0;
	unsigned int RspLen;
	int PanPos, TiltPos, ZoomPos;
	int in_processing = 0;
	t_DetectResult DetectResult;
	unsigned int acc_of_lost = 0;
	int limit_left = -400;
	int limit_right = 400;

	pthread_detach(pthread_self());

	//p_camhldctrl->init = 0;

	debug_printf("Thread camholder contrl running...\n");
	while(1){

		//check for exit
		if(p_camhldctrl->thread_canceled){
			break;
		}

		//check for deinit
		if(p_camhldctrl->init == 0){
			Recall(p_camhldctrl, 0);
			p_camhldctrl->init = -1;
		}
		//check for init
		if(p_camhldctrl->init == 1){
			InqPT_pending = 0;
			InqZ_pending = 0;
			PanPos = 0;
			TiltPos = 0;
			ZoomPos = 0;
			//empty the rsp_fifo when necessary

			//to send recall 0 command
			Recall(p_camhldctrl, 0);
			//change init state to inited
			p_camhldctrl->init = 2;
		}

		//check for response
		if(datafifo_head(&p_camhldctrl->rsp_fifo)){
			debug_printf("%x", datafifo_head(&p_camhldctrl->rsp_fifo)->arg.data[0]);

			//save one byte to the RspBuf at a time and remove it from fifo
			*RspBuf_p = datafifo_head(&p_camhldctrl->rsp_fifo)->arg.data[0];
			RspBuf_p++;
			datafifo_consumer_remove_head(&p_camhldctrl->rsp_fifo);

			//handle the response when it ends with 0xff
			if(RspBuf[RspBuf_p - RspBuf - 1]==(char)0xff){
				debug_printf("\n");
				debug_printf("uart response received\n");
				//fflush(stdout);
				RspLen = RspBuf_p - RspBuf;
				RspBuf_p = RspBuf; //complete a rsp

				if(RspLen==11){//PT_pos response
					if(RspBuf[0]==(char)0x90 && RspBuf[1]==(char)0x50){
						PanPos = byte2int(&RspBuf[2]);
						TiltPos = byte2int(&RspBuf[6]);
						if(InqPT_pending == 1){
							InqPT_pending = 2;
							debug_printf("PT inq returned\n");
						}
					}
				}

				if(RspLen == 7){//Zoom_pos response
					if(RspBuf[0]==(char)0x90 && RspBuf[1]==(char)0x50){
						ZoomPos = byte2int(&RspBuf[2]);
						if(InqZ_pending == 1){
							InqZ_pending = 2;
							debug_printf("Zoom inq returned\n");
						}
					}
				}

				if(RspLen == 3){
					if(RspBuf[0]==(char)0x90 && RspBuf[1]==(char)0x51){
						if(AbsPosPending == 1){
							AbsPosPending = 2;
						}
					}
				}
				if(RspLen == 4){
					if(RspBuf[0]==(char)0x90 && RspBuf[1]==(char)0x60){
						AbsPosPending = 0;
						InqZ_pending = 0;
						InqPT_pending = 0;
						in_processing = 0;
					}
				}
			}

			continue;
		}

		if(InqZ_pending == 2 && InqPT_pending == 2){
			InqZ_pending = 0;
			InqPT_pending = 0;

			//further processing: calculate send abs_pos command
			int offset = (int)(DetectResult.avg_left + DetectResult.avg_right -256)/2;
			debug_printf("left = %d, right = %d\n", DetectResult.avg_left, DetectResult.avg_right);
			float view_angle = -(52*ZoomPos/16384)+55.2;
			float pan_angle = atan(tan(view_angle*M_PI/360)*offset/100);
			int relative_pos = 1600*pan_angle*180/M_PI/120;
			int destpan_pos = PanPos + relative_pos;
			debug_printf("offset = %d, view_angle = %f, pan_angle = %f\n", offset, view_angle, pan_angle*180/M_PI);
			debug_printf("relative_pos=%d, PanPos=%d\n", relative_pos, PanPos);
			if(destpan_pos>limit_right){
				destpan_pos = limit_right;
			}

			if(destpan_pos<limit_left){
				destpan_pos = limit_left;
			}

			debug_printf("send AbsPos command with destpan_pos = 0x%x\n", destpan_pos);
			AbsPos(p_camhldctrl, destpan_pos, TiltPos);
			AbsPosPending = 1;

			//set completion flag
			in_processing = 2;
			continue;
		}

		if(AbsPosPending == 2 && in_processing == 2){
			AbsPosPending = 0;
			in_processing = 3;
		}

		//check for new event
		if(datafifo_head(&p_camhldctrl->req_fifo)){

#if 0
			if(p_camhldctrl->init != 2){//just remove the event from req_fifo if not initialized because now there is no manually control
				datafifo_consumer_remove_head(&p_camhldctrl->req_fifo);
				continue;
			}
#endif

			//To do: further processing
			//get a new message from fifo if there is no previous message in pending state
			if(in_processing == 0){
				debug_printf("Thread camhldctrl get a message from fifo\n");
				MsgDump((char *)datafifo_head(&p_camhldctrl->req_fifo)->arg.data);

				in_processing = 1;

				//get detection result
				GetDetectResult(&DetectResult, (char *)datafifo_head(&p_camhldctrl->req_fifo)->arg.data);

				//get_zoom and get_pos command if necessary
				if(DetectResult.detected){
					acc_of_lost = 0;
					GetZoom(p_camhldctrl);
					InqZ_pending = 1;
					GetPos(p_camhldctrl);
					InqPT_pending = 1;
				}
				else{
					acc_of_lost++;
					if(acc_of_lost == 10){
						Recall(p_camhldctrl, 0);
						//AbsPosPending = 1;
						acc_of_lost = 0;
						//in_processing = 2;
						in_processing = 3;
					}
					else{
						in_processing = 3;
					}
				}
			}
			else{
				debug_printf("camholder in processing\n");
			}

			datafifo_consumer_remove_head(&p_camhldctrl->req_fifo);
			if(in_processing == 3){//remove the message from fifo when it is completely handled
				in_processing = 0;
			}
			continue;
		}
		//debug_printf("This is from thread camhldctrl\n");
		usleep(100*1000);
	}

	debug_printf("thread camholder control exit\n");
	p_camhldctrl->thread_canceled = 2;

	pthread_exit(0);
}
#endif

void AppInit()
{
  /* --------------- load model ------------------ */
   if ((model = svm_load_model(MODEL_FILE)) == 0) {
      fprintf(stderr, "can't open model file %s\n", MODEL_FILE);
      exit(1);
       }

   loadFeature(FEATURE_FILE);
}

void AppDeinit()
{
  svm_destroy_model(model);
  if (Feature_Max != NULL) free(Feature_Max);
  if (Feature_Min != NULL) free(Feature_Min);
}

void EventVID2df(t_datafifo* df, int *PtrInt)
{
      int rect = PtrInt[0];
      int AvgTop = PtrInt[1];
      int AvgLeft = PtrInt[2];
      int AvgBottom = PtrInt[3];
      int AvgRight = PtrInt[4];
      int MaxTop = PtrInt[5];
      int MaxLeft = PtrInt[6];
      int MaxBottom = PtrInt[7];
      int MaxRight = PtrInt[8];

      printf("--> rect %d : (%d, %d), (%d, %d)\n", rect, AvgTop, AvgLeft, AvgBottom, AvgRight);

      t_cmd cmd;
      size_t size;
      fill_cmd_header(&cmd, COMMAND_EVENT_VID);
      if (rect > 0)
      {
          add_attr_int(&cmd, "Detected", 1);
          add_attr_int(&cmd, "AvgTop", AvgTop);
          add_attr_int(&cmd, "AvgLeft", AvgLeft);
          add_attr_int(&cmd, "AvgBottom", AvgBottom);
          add_attr_int(&cmd, "AvgRight", AvgRight);

          add_attr_int(&cmd, "MaxTop", MaxTop);
          add_attr_int(&cmd, "MaxLeft", MaxLeft);
          add_attr_int(&cmd, "MaxBottom", MaxBottom);
          add_attr_int(&cmd, "MaxRight", MaxRight);

      }
      else
      {
          add_attr_int(&cmd, "Detected", 0);
      }

      debug_printf("Msg: %s\n", cmd.fullbuf);
      // debug_printf("Msg: %s\n", cmd.data);
      char2hex(cmd.data_len, &size, DATA_LEN_SIZE);
      size += PACKET_HEADER_SIZE;
      // send(sock, cmd.fullbuf, size, 0);

      int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
      datafifo_producer_data_add(df, (unsigned char *)cmd.fullbuf, (int)size, flags, 0);

      debug_printf("EventVid msg sent\n");
}

void CamZoom2internal(char *Msg, t_cmd *outmsg)
{
	t_internal_abbr_cmd internal_cam_cmd;
	memset(&internal_cam_cmd, 0, sizeof(internal_cam_cmd));
	char abbr_name[16];

	char cmd[CMD_SIZE+1];
	unsigned int len;
	char *ptr;
	//int flags;

	memcpy(cmd, &Msg[VERSION_SIZE], CMD_SIZE);
	cmd[CMD_SIZE] = '\0';

	if(strcmp(cmd, COMMAND_SERVER)){
		debug_printf("Illegal command\n");
		return;
	}

	char2hex(&Msg[VERSION_SIZE+CMD_SIZE], &len, DATA_LEN_SIZE);
	debug_printf("Total length of attributes: %d(%x)\n", len, len);

	ptr = &Msg[PACKET_HEADER_SIZE];

	while(len){
		unsigned int name_len, value_len;
		char attr_name[16];
		char attr_value[256];

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

		if(!strcmp(attr_name, "CamZoom")){
			if(!strcmp(attr_value, "Wide")){
				strcpy(internal_cam_cmd.cmd, "CZW");
			}
			if(!strcmp(attr_value, "Tele")){
				strcpy(internal_cam_cmd.cmd, "CZT");
			}
		}

		if(!strcmp(attr_name, "Camera")){
			if(!strcmp(attr_value, "TeacherSpecial")){
				strcpy(internal_cam_cmd.para1, "T");
			}
			if(!strcmp(attr_value, "StudentSpecial")){
				strcpy(internal_cam_cmd.para1, "S");
			}
		}
	}

	strcpy(abbr_name, internal_cam_cmd.cmd);
	strcat(abbr_name, internal_cam_cmd.para1);
	debug_printf("%s\n", abbr_name);

	fill_cmd_header(outmsg, COMMAND_INTERNAL);
	add_attr_int(outmsg, abbr_name, 0);
}

void CamPanTilt2internal(char *Msg, t_cmd *outmsg)
{
	t_internal_abbr_cmd internal_cam_cmd;
	memset(&internal_cam_cmd, 0, sizeof(internal_cam_cmd));
	char abbr_name[16];

	char cmd[CMD_SIZE+1];
	unsigned int len;
	char *ptr;
	//int flags;

	memcpy(cmd, &Msg[VERSION_SIZE], CMD_SIZE);
	cmd[CMD_SIZE] = '\0';

	if(strcmp(cmd, COMMAND_SERVER)){
		debug_printf("Illegal command\n");
		return;
	}

	char2hex(&Msg[VERSION_SIZE+CMD_SIZE], &len, DATA_LEN_SIZE);
	debug_printf("Total length of attributes: %d(%x)\n", len, len);

	ptr = &Msg[PACKET_HEADER_SIZE];

	while(len){
		unsigned int name_len, value_len;
		char attr_name[16];
		char attr_value[256];

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

		if(!strcmp(attr_name, "CamPanTilt")){
			if(!strcmp(attr_value, "Left")){
				strcpy(internal_cam_cmd.cmd, "CPTL");
			}
			if(!strcmp(attr_value, "Right")){
				strcpy(internal_cam_cmd.cmd, "CPTR");
			}
			if(!strcmp(attr_value, "Up")){
				strcpy(internal_cam_cmd.cmd, "CPTU");
			}
			if(!strcmp(attr_value, "Down")){
				strcpy(internal_cam_cmd.cmd, "CPTD");
			}
		}

		if(!strcmp(attr_name, "Camera")){
			if(!strcmp(attr_value, "TeacherSpecial")){
				strcpy(internal_cam_cmd.para1, "T");
			}
			if(!strcmp(attr_value, "StudentSpecial")){
				strcpy(internal_cam_cmd.para1, "S");
			}
		}
	}

	strcpy(abbr_name, internal_cam_cmd.cmd);
	strcat(abbr_name, internal_cam_cmd.para1);
	debug_printf("%s\n", abbr_name);

	fill_cmd_header(outmsg, COMMAND_INTERNAL);
	add_attr_int(outmsg, abbr_name, 0);
}

void CamMemoryInq2internal(char *Msg, t_cmd *outmsg)
{
	t_internal_abbr_cmd internal_cam_cmd;
	memset(&internal_cam_cmd, 0, sizeof(internal_cam_cmd));
	char abbr_name[16];

	char cmd[CMD_SIZE+1];
	unsigned int len;
	char *ptr;
	//int flags;

	memcpy(cmd, &Msg[VERSION_SIZE], CMD_SIZE);
	cmd[CMD_SIZE] = '\0';

	if(strcmp(cmd, COMMAND_SERVER)){
		debug_printf("Illegal command\n");
		return;
	}

	char2hex(&Msg[VERSION_SIZE+CMD_SIZE], &len, DATA_LEN_SIZE);
	debug_printf("Total length of attributes: %d(%x)\n", len, len);

	ptr = &Msg[PACKET_HEADER_SIZE];

	while(len){
		unsigned int name_len, value_len;
		char attr_name[16];
		char attr_value[256];

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

		if(!strcmp(attr_name, "CamMemoryInq")){
			strcpy(internal_cam_cmd.cmd, "CMI");
			strcpy(internal_cam_cmd.intvalue, attr_value);
		}

		if(!strcmp(attr_name, "Camera")){
			if(!strcmp(attr_value, "TeacherSpecial")){
				strcpy(internal_cam_cmd.para1, "T");
			}
			if(!strcmp(attr_value, "StudentSpecial")){
				strcpy(internal_cam_cmd.para1, "S");
			}
		}
	}

	strcpy(abbr_name, internal_cam_cmd.cmd);
	strcat(abbr_name, internal_cam_cmd.para1);
	debug_printf("%s\n", abbr_name);

	fill_cmd_header(outmsg, COMMAND_INTERNAL);
	add_attr_char(outmsg, abbr_name, internal_cam_cmd.intvalue);
}

void CamMemorySave2internal(char *Msg, t_cmd *outmsg)
{
	t_internal_abbr_cmd internal_cam_cmd;
	memset(&internal_cam_cmd, 0, sizeof(internal_cam_cmd));
	char abbr_name[16];

	char cmd[CMD_SIZE+1];
	unsigned int len;
	char *ptr;
	//int flags;

	memcpy(cmd, &Msg[VERSION_SIZE], CMD_SIZE);
	cmd[CMD_SIZE] = '\0';

	if(strcmp(cmd, COMMAND_SERVER)){
		debug_printf("Illegal command\n");
		return;
	}

	char2hex(&Msg[VERSION_SIZE+CMD_SIZE], &len, DATA_LEN_SIZE);
	debug_printf("Total length of attributes: %d(%x)\n", len, len);

	ptr = &Msg[PACKET_HEADER_SIZE];

	while(len){
		unsigned int name_len, value_len;
		char attr_name[16];
		char attr_value[256];

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

		if(!strcmp(attr_name, "CamMemorySave")){
			strcpy(internal_cam_cmd.cmd, "CMS");
			strcpy(internal_cam_cmd.intvalue, attr_value);
		}

		if(!strcmp(attr_name, "Camera")){
			if(!strcmp(attr_value, "TeacherSpecial")){
				strcpy(internal_cam_cmd.para1, "T");
			}
			if(!strcmp(attr_value, "StudentSpecial")){
				strcpy(internal_cam_cmd.para1, "S");
			}
		}
	}

	strcpy(abbr_name, internal_cam_cmd.cmd);
	strcat(abbr_name, internal_cam_cmd.para1);
	debug_printf("%s\n", abbr_name);

	fill_cmd_header(outmsg, COMMAND_INTERNAL);
	add_attr_char(outmsg, abbr_name, internal_cam_cmd.intvalue);
}

void InstructorMsgHandle(t_instructor_thread_param *instructor_thread_param, char *Msg)
{
	char cmd[CMD_SIZE+1];
	unsigned int len;
	char *ptr;
	int flags;

	memcpy(cmd, &Msg[VERSION_SIZE], CMD_SIZE);
	cmd[CMD_SIZE] = '\0';

	if(strcmp(cmd, COMMAND_SERVER)){
		debug_printf("Illegal command\n");
		return;
	}

	char2hex(&Msg[VERSION_SIZE+CMD_SIZE], &len, DATA_LEN_SIZE);
	debug_printf("Total length of attributes: %d(%x)\n", len, len);

	ptr = &Msg[PACKET_HEADER_SIZE];

	while(len){
		unsigned int name_len, value_len;
		char attr_name[16];
		char attr_value[256];

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

		if(!strcmp(attr_name, "Start")){
			if(instructor_thread_param->status == RECBC_IDLE){
				size_t size;
				t_cmd outmsg;

				unsigned int RegVal;
				RegVal = 0;
				if(instructor_thread_param->coreA_thread_param->thread_listen_param.socket != -1){
					RegVal |= 1;
				}
				if(instructor_thread_param->coreB_thread_param->thread_listen_param.socket != -1){
					RegVal |= 2;
				}
				if(instructor_thread_param->coreC_thread_param->thread_listen_param.socket != -1){
					RegVal |= 4;
				}
				if(instructor_thread_param->coreD_thread_param->thread_listen_param.socket != -1){
					RegVal |= 8;
				}
				debug_printf("RegVal=%x\n", RegVal);

				fill_cmd_header(&outmsg, COMMAND_SET_LED);
				add_attr_int(&outmsg, "Reg", RegVal);
				add_attr_int(&outmsg, "Reg_09", g_reg_09);
				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
				size += PACKET_HEADER_SIZE;
				flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
				datafifo_producer_data_add(instructor_thread_param->producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);

				fill_cmd_header(&outmsg, COMMAND_INTERNAL);
				add_attr_int(&outmsg, "Start", 1);
				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
				size += PACKET_HEADER_SIZE;
				//send(rundev->sock_dev, outmsg.fullbuf, size, 0);
				flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
				datafifo_producer_data_add(instructor_thread_param->producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);

				fill_cmd_header(&outmsg, COMMAND_CONFIG);
				//add_attr_char(&outmsg, "StreamId", "00");
				//add_attr_int(&outmsg, "EncSrc", 2); //Assumption: 0:port0(mux) 1:port1(VGA)
				add_attr_int(&outmsg, "FrameWidth", 1920);
				add_attr_int(&outmsg, "FrameHeight", 1080);
				add_attr_int(&outmsg, "FrameRate", g_cfg.frame_rate);
				add_attr_int(&outmsg, "BitRate", g_cfg.bitrate);
				add_attr_int(&outmsg, "LBitRate", g_cfg.live_bitrate);
				//add_attr_int(&outmsg, "ABitRate", 96);
				//add_attr_int(&outmsg, "SampleRate", 44100);
				//add_attr_int(&outmsg, "AnaSrc", 0);
				//add_attr_int(&outmsg, "AnaMode", 0);
				//add_attr_int(&outmsg, "Video", 1);
				//add_attr_int(&outmsg, "Audio", 1);
				//add_attr_int(&outmsg, "Analyze", 1);

				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
				//size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
				size += PACKET_HEADER_SIZE;
				//send(rundev->sock_dev, outmsg.fullbuf, size, 0);
				flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
				datafifo_producer_data_add(instructor_thread_param->producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);

				fill_cmd_header(&outmsg, COMMAND_GETDATA);
				add_attr_char(&outmsg, "Type", "All");

				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
				size += PACKET_HEADER_SIZE;
				//send(rundev->sock_dev, outmsg.fullbuf, size, 0);
				flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
				datafifo_producer_data_add(instructor_thread_param->producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
				instructor_thread_param->status = RECBC_WORKING;
			}
		}

		if(!strcmp(attr_name, "Stop")){
			if(instructor_thread_param->status == RECBC_WORKING){
				size_t size;
				t_cmd outmsg;

				unsigned int RegVal = 0;
				fill_cmd_header(&outmsg, COMMAND_SET_LED);
				add_attr_int(&outmsg, "Reg", RegVal);
				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
				size += PACKET_HEADER_SIZE;
				flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
				datafifo_producer_data_add(instructor_thread_param->producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);

				fill_cmd_header(&outmsg, COMMAND_INTERNAL);
				add_attr_int(&outmsg, "Stop", 1);
				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
				size += PACKET_HEADER_SIZE;
				//send(rundev->sock_dev, outmsg.fullbuf, size, 0);
				flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
				datafifo_producer_data_add(instructor_thread_param->producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);

				fill_cmd_header(&outmsg, COMMAND_GETDATA);
				add_attr_char(&outmsg, "Type", "None");

				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
				size += PACKET_HEADER_SIZE;
				//send(rundev->sock_dev, outmsg.fullbuf, size, 0);
				flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
				datafifo_producer_data_add(instructor_thread_param->producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
				instructor_thread_param->status = RECBC_IDLE;
			}
		}

		if(!strcmp(attr_name, "Pause")){
			if(instructor_thread_param->status == RECBC_WORKING){
				size_t size;
				t_cmd outmsg;

				fill_cmd_header(&outmsg, COMMAND_INTERNAL);
				add_attr_int(&outmsg, "Pause", 1);

				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
				size += PACKET_HEADER_SIZE;
				//send(rundev->sock_dev, outmsg.fullbuf, size, 0);
				flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
				datafifo_producer_data_add(instructor_thread_param->producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
			}
		}

		if(!strcmp(attr_name, "Resume")){
			if(instructor_thread_param->status == RECBC_WORKING){
				size_t size;
				t_cmd outmsg;

				fill_cmd_header(&outmsg, COMMAND_INTERNAL);
				add_attr_int(&outmsg, "Resume", 1);

				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
				size += PACKET_HEADER_SIZE;
				//send(rundev->sock_dev, outmsg.fullbuf, size, 0);
				flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
				datafifo_producer_data_add(instructor_thread_param->producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
			}
		}

		if(!strcmp(attr_name, "SwitchVideo")){
			size_t size;
			t_cmd outmsg;

			fill_cmd_header(&outmsg, COMMAND_INTERNAL);
			add_attr_char(&outmsg, attr_name, attr_value);

			char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
			size += PACKET_HEADER_SIZE;
			//send(rundev->sock_dev, outmsg.fullbuf, size, 0);
			flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
			datafifo_producer_data_add(instructor_thread_param->producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
		}

		if(!strcmp(attr_name, "SetRecordMode")){
			size_t size;
			t_cmd outmsg;

			fill_cmd_header(&outmsg, COMMAND_INTERNAL);
			add_attr_char(&outmsg, attr_name, attr_value);

			char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
			size += PACKET_HEADER_SIZE;
			//send(rundev->sock_dev, outmsg.fullbuf, size, 0);
			flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
			datafifo_producer_data_add(instructor_thread_param->producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
		}

		if(!strcmp(attr_name, "CamZoom")){
			size_t size;
			t_cmd outmsg;

			CamZoom2internal(Msg, &outmsg);

			char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
			size += PACKET_HEADER_SIZE;
			//send(rundev->sock_dev, outmsg.fullbuf, size, 0);
			flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
			datafifo_producer_data_add(instructor_thread_param->producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
		}

		if(!strcmp(attr_name, "CamPanTilt")){
			size_t size;
			t_cmd outmsg;

			CamPanTilt2internal(Msg, &outmsg);

			char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
			size += PACKET_HEADER_SIZE;
			//send(rundev->sock_dev, outmsg.fullbuf, size, 0);
			flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
			datafifo_producer_data_add(instructor_thread_param->producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
		}

		if(!strcmp(attr_name, "CamMemoryInq")){
			size_t size;
			t_cmd outmsg;

			CamMemoryInq2internal(Msg, &outmsg);

			char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
			size += PACKET_HEADER_SIZE;
			//send(rundev->sock_dev, outmsg.fullbuf, size, 0);
			flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
			datafifo_producer_data_add(instructor_thread_param->producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
		}

		if(!strcmp(attr_name, "CamMemorySave")){
			size_t size;
			t_cmd outmsg;

			CamMemorySave2internal(Msg, &outmsg);

			char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
			size += PACKET_HEADER_SIZE;
			//send(rundev->sock_dev, outmsg.fullbuf, size, 0);
			flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
			datafifo_producer_data_add(instructor_thread_param->producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
		}
	}

	debug_printf("Server message handled\n");
}

void DevAMsgHandle(t_coreA_thread_param *thread_param, char *Msg)
{
	char cmd[CMD_SIZE+1];
	unsigned int len;
	char *ptr;

	memcpy(cmd, &Msg[VERSION_SIZE], CMD_SIZE);
	cmd[CMD_SIZE] = '\0';

	char2hex(&Msg[VERSION_SIZE+CMD_SIZE], &len, DATA_LEN_SIZE);
	//debug_printf("Total length of attributes: %d(%x)\n", len, len);

	ptr = &Msg[PACKET_HEADER_SIZE];

	if(!strcmp(cmd, COMMAND_EVENT_UCI)){
		//debug_printf("COMMAND_EVENT_UCI reveived\n");
		int channel = 0;
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
				if(!strcmp(attr_value, "00")){
					channel = 0;
				}
				else{
					channel =1;
				}
			}

			if(!strcmp(attr_name, "Data")){
				int flags = 0;

				if(frame_info.packet_num == 0){
					flags |= FRAME_FLAG_NEWFRAME;
					if(frame_info.frame_num == 0){
						flags |= FRAME_FLAG_FIRSTFRAME;
					}
				}

				if(frame_info.packet_num == frame_info.pkts_in_frame-1){
					flags |= FRAME_FLAG_FRAMEEND;
				}

				int ret;
				if(channel){
					ret = datafifo_producer_data_add(thread_param->ImgM_producer_fifo, (unsigned char*)attr_value, (int)value_len, flags, 0);
				}
				else{
					ret = datafifo_producer_data_add(thread_param->ImgV_producer_fifo, (unsigned char*)attr_value, (int)value_len, flags, 0);
				}

				if(ret < 0){
					debug_printf("uci add fail:%d, %x, %d\n", ret, flags, value_len);
				}
			}
		}
	}

	if(!strcmp(cmd, COMMAND_GETDATA_ACK)){
		int channel = 0;
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

			if(!strcmp(attr_name, "StreamId")){
				if(!strcmp(attr_value, "00")){
					channel = 0;
				}
				else{
					channel = 1;
				}
			}

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

			if(!strcmp(attr_name, "Data")){
				int flags = 0;

				if(frame_info.frame_type == FRMTYPE_AUDIO)
				{
					flags |= FRAME_FLAG_AUDIOFRM;
					if(frame_info.packet_num == 0) //first packet
					{
						flags |= FRAME_FLAG_NEWFRAME;
						if(frame_info.frame_num == 0) //first frame
						{
							flags |= FRAME_FLAG_FIRSTFRAME;
						}
					}
					if(frame_info.packet_num == frame_info.pkts_in_frame-1){
						flags |= FRAME_FLAG_FRAMEEND;
						//debug_printf("the last packet received, pkt_num = %d\n", frame_info.packet_num);
					}
				}
				else
				{
					if(frame_info.packet_num == 0) //first packet
					{
						flags |= FRAME_FLAG_NEWFRAME;
						if(frame_info.frame_num == 0) //first frame
						{
							flags |= FRAME_FLAG_FIRSTFRAME;
						}
						if((frame_info.frame_type == FRMTYPE_VIDEO_I) ||(frame_info.frame_type == FRMTYPE_VIDEO_IDR))
						{
							flags |= FRAME_FLAG_KEYFRAME;
							//debug_printf("key frame present\n");
						}
						//debug_printf("frame_type=%d\n", frame_info.frame_type);
					}
					if(frame_info.packet_num == frame_info.pkts_in_frame-1){
						flags |= FRAME_FLAG_FRAMEEND;
						//debug_printf("the last packet received, pkt_num = %d\n", frame_info.packet_num);
					}
				}

				if(flags & FRAME_FLAG_AUDIOFRM){
					//int ret;
					int ret = datafifo_producer_data_add(thread_param->Audio_producer_fifo, (unsigned char*)attr_value, (int)value_len, flags, (int)frame_info.time_stamp);
					if(ret < 0){
						debug_printf("adata add fail:%d, %x, %d\n", ret, flags, value_len);
					}
				}
				else{
					//int ret;
					int ret;
					if(channel==0){
						ret = datafifo_producer_data_add(thread_param->Vstore_producer_fifo, (unsigned char*)attr_value, (int)value_len, flags, (int)frame_info.time_stamp);
					}
					else{
						ret = datafifo_producer_data_add(thread_param->Vlive_producer_fifo, (unsigned char*)attr_value, (int)value_len, flags, (int)frame_info.time_stamp);
					}
					if(ret < 0){
						debug_printf("vdata add fail:%d, %x, %d\n", ret, flags, value_len);
					}
				}
			}
		}
	}

	if(!strcmp(cmd, COMMAND_EVENT_VID)){
		int flags;
		debug_printf("Event video received\n");
		len += PACKET_HEADER_SIZE;
		flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
		datafifo_producer_data_add(thread_param->Event_producer_fifo, (unsigned char *)Msg, (int)len, flags, 0);
	}

	if(!strcmp(cmd, COMMAND_EVENT_VGA)){
		int flags;
		debug_printf("Event VGA received\n");
		len += PACKET_HEADER_SIZE;
		flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
		datafifo_producer_data_add(thread_param->Event_producer_fifo, (unsigned char *)Msg, (int)len, flags, 0);
	}

	if(!strcmp(cmd, COMMAND_EVENT_AUX)){
		int flags;
		debug_printf("Event AUX received\n");
		len += PACKET_HEADER_SIZE;
		flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
		datafifo_producer_data_add(thread_param->Event_producer_fifo, (unsigned char *)Msg, (int)len, flags, 0);
	}

	if(!strcmp(cmd, COMMAND_BYP_RSP)){
		//debug_printf("COMMAND_BYP_RSP received\n");
		int channel = 0;
		while(len){
			unsigned int name_len, value_len;
			char attr_name[16];
			char attr_value[1536];

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

			if(!strcmp(attr_name, "Channel")){
				if(!strcmp(attr_value, "0")){
					channel = 0;
				}
				else{
					channel = 1;
				}
			}

			if(!strcmp(attr_name, "RawBytes")){
				int i;
				if(channel){
					for(i=0; i<value_len; i++){
						datafifo_producer_data_add(thread_param->UART_RX_producer_fifo_2, (unsigned char *)&attr_value[i], 1, FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND, 0);
					}
				}
				else{
					for(i=0; i<value_len; i++){
						datafifo_producer_data_add(thread_param->UART_RX_producer_fifo_1, (unsigned char *)&attr_value[i], 1, FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND, 0);
					}
				}
			}
		}
	}

	//debug_printf("Device message handled\n");
}

void DevBCMsgHandle(t_coreBC_thread_param *thread_param, char *Msg)
{
	char cmd[CMD_SIZE+1];
	unsigned int len;
	char *ptr;

	memcpy(cmd, &Msg[VERSION_SIZE], CMD_SIZE);
	cmd[CMD_SIZE] = '\0';

	char2hex(&Msg[VERSION_SIZE+CMD_SIZE], &len, DATA_LEN_SIZE);
	//debug_printf("Total length of attributes: %d(%x)\n", len, len);

	ptr = &Msg[PACKET_HEADER_SIZE];

	if(!strcmp(cmd, COMMAND_EVENT_UCI)){
		int channel = 0;
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
				if(!strcmp(attr_value, "00")){
					channel = 0;
				}
				else{
					channel =1;
				}
			}

			if(!strcmp(attr_name, "Data")){
				int flags = 0;

				if(frame_info.packet_num == 0){
					flags |= FRAME_FLAG_NEWFRAME;
					if(frame_info.frame_num == 0){
						flags |= FRAME_FLAG_FIRSTFRAME;
					}
				}

				if(frame_info.packet_num == frame_info.pkts_in_frame-1){
					flags |= FRAME_FLAG_FRAMEEND;
				}

				int ret=0;
				if(channel){
					ret = datafifo_producer_data_add(thread_param->Img2_producer_fifo, (unsigned char*)attr_value, (int)value_len, flags, 0);
				}
				else{
					ret = datafifo_producer_data_add(thread_param->Img1_producer_fifo, (unsigned char*)attr_value, (int)value_len, flags, 0);
				}

				if(ret < 0){
					debug_printf("uci add fail:%d, %x, %d\n", ret, flags, value_len);
				}
			}
		}
	}

	if(!strcmp(cmd, COMMAND_GETDATA_ACK)){
		int channel = 0;
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

			if(!strcmp(attr_name, "StreamId")){
				if(!strcmp(attr_value, "00")){
					channel = 0;
				}
				else{
					channel = 1;
				}
			}

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

			if(!strcmp(attr_name, "Data")){
				int flags = 0;

				if(frame_info.frame_type == FRMTYPE_AUDIO)
				{
					flags |= FRAME_FLAG_AUDIOFRM;
					if(frame_info.packet_num == 0) //first packet
					{
						flags |= FRAME_FLAG_NEWFRAME;
						if(frame_info.frame_num == 0) //first frame
						{
							flags |= FRAME_FLAG_FIRSTFRAME;
						}
					}
					if(frame_info.packet_num == frame_info.pkts_in_frame-1){
						flags |= FRAME_FLAG_FRAMEEND;
						//debug_printf("the last packet received, pkt_num = %d\n", frame_info.packet_num);
					}
				}
				else
				{
					if(frame_info.packet_num == 0) //first packet
					{
						flags |= FRAME_FLAG_NEWFRAME;
						if(frame_info.frame_num == 0) //first frame
						{
							flags |= FRAME_FLAG_FIRSTFRAME;
						}
						if((frame_info.frame_type == FRMTYPE_VIDEO_I) ||(frame_info.frame_type == FRMTYPE_VIDEO_IDR))
						{
							flags |= FRAME_FLAG_KEYFRAME;
							//debug_printf("key frame present\n");
						}
						//debug_printf("frame_type=%d\n", frame_info.frame_type);
					}
					if(frame_info.packet_num == frame_info.pkts_in_frame-1){
						flags |= FRAME_FLAG_FRAMEEND;
						//debug_printf("the last packet received, pkt_num = %d\n", frame_info.packet_num);
					}
				}

				if(flags & FRAME_FLAG_AUDIOFRM){
					//impossible, so do nothing
				}
				else{
					//int ret;
					int ret;
					if(channel){
						ret = datafifo_producer_data_add(thread_param->V2_producer_fifo, (unsigned char*)attr_value, (int)value_len, flags, (int)frame_info.time_stamp);
					}
					else{
						ret = datafifo_producer_data_add(thread_param->V1_producer_fifo, (unsigned char*)attr_value, (int)value_len, flags, (int)frame_info.time_stamp);
					}
					if(ret < 0){
						debug_printf("vdata add fail:%d, %x, %d\n", ret, flags, value_len);
					}
				}
			}
		}
	}

	if(!strcmp(cmd, COMMAND_EVENT_VID)){
		int flags;
		debug_printf("Event video received\n");
		len += PACKET_HEADER_SIZE;
		flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
		datafifo_producer_data_add(thread_param->Event_producer_fifo, (unsigned char *)Msg, (int)len, flags, 0);
	}

	if(!strcmp(cmd, COMMAND_EVENT_VGA)){
		int flags;
		debug_printf("Event VGA received\n");
		len += PACKET_HEADER_SIZE;
		flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
		datafifo_producer_data_add(thread_param->Event_producer_fifo, (unsigned char *)Msg, (int)len, flags, 0);
	}

	if(!strcmp(cmd, COMMAND_EVENT_AUX)){
		int flags;
		debug_printf("Event AUX received\n");
		len += PACKET_HEADER_SIZE;
		flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
		datafifo_producer_data_add(thread_param->Event_producer_fifo, (unsigned char *)Msg, (int)len, flags, 0);
	}

	if(!strcmp(cmd, COMMAND_BYP_RSP)){
		//debug_printf("COMMAND_BYP_RSP received\n");
		int channel = 0;
		while(len){
			unsigned int name_len, value_len;
			char attr_name[16];
			char attr_value[1536];

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

			if(!strcmp(attr_name, "Channel")){
				if(!strcmp(attr_value, "0")){
					channel = 0;
				}
				else{
					channel = 1;
				}
			}

			if(!strcmp(attr_name, "RawBytes")){
				int i;
				if(channel){
					for(i=0; i<value_len; i++){
						datafifo_producer_data_add(thread_param->UART_RX_producer_fifo_2, (unsigned char *)&attr_value[i], 1, FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND, 0);
					}
				}
				else{
					for(i=0; i<value_len; i++){
						datafifo_producer_data_add(thread_param->UART_RX_producer_fifo_1, (unsigned char *)&attr_value[i], 1, FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND, 0);
					}
				}
			}
		}
	}

	//debug_printf("Device message handled\n");
}

void DevDMsgHandle(t_coreD_thread_param *thread_param, char *Msg)
{
	char cmd[CMD_SIZE+1];
	unsigned int len;
	char *ptr;

	memcpy(cmd, &Msg[VERSION_SIZE], CMD_SIZE);
	cmd[CMD_SIZE] = '\0';

	char2hex(&Msg[VERSION_SIZE+CMD_SIZE], &len, DATA_LEN_SIZE);
	//debug_printf("Total length of attributes: %d(%x)\n", len, len);

	ptr = &Msg[PACKET_HEADER_SIZE];

	if(!strcmp(cmd, COMMAND_EVENT_UCI)){
		int channel = 0;
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
				if(!strcmp(attr_value, "00")){
					channel = 0;
				}
				else{
					channel =1;
				}
			}

			if(!strcmp(attr_name, "Data")){
				int flags = 0;

				if(frame_info.packet_num == 0){
					flags |= FRAME_FLAG_NEWFRAME;
					if(frame_info.frame_num == 0){
						flags |= FRAME_FLAG_FIRSTFRAME;
					}
				}

				if(frame_info.packet_num == frame_info.pkts_in_frame-1){
					flags |= FRAME_FLAG_FRAMEEND;
				}

				int ret;
				if(channel){
					ret = datafifo_producer_data_add(thread_param->Img2_producer_fifo, (unsigned char*)attr_value, (int)value_len, flags, 0);
				}
				else{
					ret = datafifo_producer_data_add(thread_param->Img1_producer_fifo, (unsigned char*)attr_value, (int)value_len, flags, 0);
				}

				if(ret < 0){
					debug_printf("uci add fail:%d, %x, %d\n", ret, flags, value_len);
				}
			}
		}
	}

	if(!strcmp(cmd, COMMAND_GETDATA_ACK)){
		int channel = 0;
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

			if(!strcmp(attr_name, "StreamId")){
				if(!strcmp(attr_value, "00")){
					channel = 0;
				}
				else{
					channel = 1;
				}
			}

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

			if(!strcmp(attr_name, "Data")){
				int flags = 0;

				if(frame_info.frame_type == FRMTYPE_AUDIO)
				{
					flags |= FRAME_FLAG_AUDIOFRM;
					if(frame_info.packet_num == 0) //first packet
					{
						flags |= FRAME_FLAG_NEWFRAME;
						if(frame_info.frame_num == 0) //first frame
						{
							flags |= FRAME_FLAG_FIRSTFRAME;
						}
					}
					if(frame_info.packet_num == frame_info.pkts_in_frame-1){
						flags |= FRAME_FLAG_FRAMEEND;
						//debug_printf("the last packet received, pkt_num = %d\n", frame_info.packet_num);
					}
				}
				else
				{
					if(frame_info.packet_num == 0) //first packet
					{
						flags |= FRAME_FLAG_NEWFRAME;
						if(frame_info.frame_num == 0) //first frame
						{
							flags |= FRAME_FLAG_FIRSTFRAME;
						}
						if((frame_info.frame_type == FRMTYPE_VIDEO_I) ||(frame_info.frame_type == FRMTYPE_VIDEO_IDR))
						{
							flags |= FRAME_FLAG_KEYFRAME;
							//debug_printf("key frame present\n");
						}
						//debug_printf("frame_type=%d\n", frame_info.frame_type);
					}
					if(frame_info.packet_num == frame_info.pkts_in_frame-1){
						flags |= FRAME_FLAG_FRAMEEND;
						//debug_printf("the last packet received, pkt_num = %d\n", frame_info.packet_num);
					}
				}

				if(flags & FRAME_FLAG_AUDIOFRM){
					//impossible, so do nothing
				}
				else{
					//int ret;
					int ret;
					if(channel){
						ret = datafifo_producer_data_add(thread_param->V2_producer_fifo, (unsigned char*)attr_value, (int)value_len, flags, (int)frame_info.time_stamp);
					}
					else{
						ret = datafifo_producer_data_add(thread_param->V1_producer_fifo, (unsigned char*)attr_value, (int)value_len, flags, (int)frame_info.time_stamp);
					}
					if(ret < 0){
						debug_printf("vdata add fail:%d, %x, %d\n", ret, flags, value_len);
					}
				}
			}
		}
	}

	if(!strcmp(cmd, COMMAND_EVENT_VID)){
		int flags;
		debug_printf("Event video received\n");
		len += PACKET_HEADER_SIZE;
		flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
		datafifo_producer_data_add(thread_param->Event_producer_fifo, (unsigned char *)Msg, (int)len, flags, 0);
	}

	if(!strcmp(cmd, COMMAND_EVENT_VGA)){
		int flags;
		debug_printf("Event VGA received\n");
		len += PACKET_HEADER_SIZE;
		flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
		datafifo_producer_data_add(thread_param->Event_producer_fifo, (unsigned char *)Msg, (int)len, flags, 0);
	}

	if(!strcmp(cmd, COMMAND_EVENT_AUX)){
		int flags;
		debug_printf("Event AUX received\n");
		len += PACKET_HEADER_SIZE;
		flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
		datafifo_producer_data_add(thread_param->Event_producer_fifo, (unsigned char *)Msg, (int)len, flags, 0);
	}

	//debug_printf("Device message handled\n");
}

//The main function that runs an individual device
#if 0
void RunDevice()
{
	//int i;
	int port = 3000;
	//int sock_client=0;
	struct sockaddr_in cli;
	fd_set fdsr;
	struct timeval tv;
	int ret;
	int device_disconnected=0, server_disconnected =0;

	t_rundevice rundev;
	t_localsave localsave;
	t_camholderctrl camhldctrl;
	t_videoanalyze videoanalyze;

	rundev.switched = 0;
	rundev.p_camhldctrl = &camhldctrl;

	pthread_t pt_localsave_id;
	pthread_t pt_camhldctrl_id;
	pthread_t pt_videoanalyze_id;

	//init rundev context
	rundev.sock_dev = sock_list[0];

	//create fifo graph
	datafifo_init(&rundev.mux_fifo, VINBUF_SIZE, VINBUF_CNT);
	datafifo_init(&rundev.audio_fifo, AINBUF_SIZE, AINBUF_CNT);
	datafifo_init(&rundev.camhldReq_fifo, EINBUF_SIZE, EINBUF_CNT);
	datafifo_init(&rundev.camhldRsp_fifo, BYPBUF_SIZE, BYPBUF_CNT);
	datafifo_init(&rundev.uci_fifo, VINBUF_SIZE*36, EINBUF_CNT);
	datafifo_init(&camhldctrl.ctrl_fifo, EINBUF_SIZE, EINBUF_CNT);
	datafifo_init(&videoanalyze.analyze_producer_fifo, EINBUF_SIZE, EINBUF_CNT);

	datafifo_init(&rundev.consumer_fifo, 0, EINBUF_CNT);
	datafifo_connect(&camhldctrl.ctrl_fifo, &rundev.consumer_fifo);

	datafifo_init(&localsave.av_fifo, 0, VINBUF_CNT+AINBUF_CNT);
	datafifo_connect(&rundev.mux_fifo, &localsave.av_fifo);
	datafifo_connect(&rundev.audio_fifo, &localsave.av_fifo);

	datafifo_init(&camhldctrl.req_fifo, 0, EINBUF_CNT);
	datafifo_connect(&rundev.camhldReq_fifo, &camhldctrl.req_fifo);
	datafifo_connect(&videoanalyze.analyze_producer_fifo, &camhldctrl.req_fifo);

	datafifo_init(&camhldctrl.rsp_fifo, 0, BYPBUF_CNT);
	datafifo_connect(&rundev.camhldRsp_fifo, &camhldctrl.rsp_fifo);

	datafifo_init(&videoanalyze.uci_consumer_fifo, 0, EINBUF_CNT);
	datafifo_connect(&rundev.uci_fifo, &videoanalyze.uci_consumer_fifo);

	//create threads
	localsave.thread_canceled = 0;
	localsave.df_a = &rundev.audio_fifo;
	localsave.df_v = &rundev.mux_fifo;
	localsave.sock = -1;
	ret = pthread_create(&pt_localsave_id, NULL, thread_localsave, (void *)&localsave);
	if(ret<0){
		debug_printf("create thread_localsave failed\n");
		exit(0);
	}

	camhldctrl.thread_canceled = 0;
	camhldctrl.init = 0;
	ret = pthread_create(&pt_camhldctrl_id, NULL, thread_videotracking, (void *)&camhldctrl);
	if(ret<0){
		debug_printf("create thread_camhldctrl failed\n");
		exit(0);
	}

	videoanalyze.thread_canceled = 0;
	ret = pthread_create(&pt_videoanalyze_id, NULL, thread_analyze, (void *)&videoanalyze);
	if(ret<0){
		debug_printf("create thread_videoanalyze failed\n");
		exit(0);
	}

	while(1){
		cli.sin_family = AF_INET;
		cli.sin_port = htons(3001);
		cli.sin_addr.s_addr = inet_addr("127.0.0.1");

		localsave.sock = socket(AF_INET, SOCK_STREAM, 0);
		if(localsave.sock<0){
			debug_printf("open socket error\n");
			exit(1);
		}
		debug_printf("connecting to streaming server...");

		//set the send buf of the socket to ensure there is enough buffer to contain the data sent at a time
		int sndbufsize = 500*1024;
		if(setsockopt(localsave.sock, SOL_SOCKET, SO_SNDBUF, (const void *)&sndbufsize, sizeof(sndbufsize))==-1){
			debug_printf("setsockeopt failed\n");
		}

		if(connect(localsave.sock, (struct sockaddr*)&cli, sizeof(cli))<0){
			debug_printf("could not connect to streaming server\n");
			close(localsave.sock);
			localsave.sock = -1;
		}
		else{
			debug_printf("connected\n");
			char pkt_buf[32];
			t_streaming_packet_header *ptr = (t_streaming_packet_header *)pkt_buf;

			ptr->size = strlen(CfgDir)+1;
			ptr->type = PACKET_TYPE_ID;
			ptr->timestamp = 0;
			ptr->flag = 0;
			strcpy(&pkt_buf[STREAMING_PACKET_HEADER_LENGTH], CfgDir);
			send(localsave.sock, pkt_buf, ptr->size + STREAMING_PACKET_HEADER_LENGTH, 0);
		}

		//connect to the server platform. retry after 30s when fail
		while(1){

			cli.sin_family = AF_INET;
			cli.sin_port = htons(port);
			cli.sin_addr.s_addr = inet_addr("127.0.0.1");

			rundev.sock_srv = socket(AF_INET, SOCK_STREAM, 0);
			if(rundev.sock_srv<0){
				debug_printf("open socket error\n");
				exit(1);
			}

			debug_printf("connecting to server...");


			if(connect(rundev.sock_srv, (struct sockaddr*)&cli, sizeof(cli))<0){
				debug_printf("connect error\n");
				close(rundev.sock_srv);
				sleep(30);
				//continue;
			}
			else{
				break;
			}
		}
		debug_printf("connected\n");

		LoadVideoCfg();
		//ParseVideoCfg();

		//main message processing loop.
		//break when disconnect to server and device
		while(1){
			//check if to switch back from VGA
			if(rundev.switched){
				struct timeval swi_tv;
				gettimeofday(&swi_tv, NULL);
				int det = (swi_tv.tv_sec - rundev.tv.tv_sec)+(swi_tv.tv_usec - rundev.tv.tv_usec)/1000000;
				debug_printf("det = %d\n", det);
				if(det>=7){
					size_t size;
					t_cmd outmsg;
					fill_cmd_header(&outmsg, COMMAND_MERGECTRL);
					add_attr_int(&outmsg, "MergeSrc", 0); //assumption: VGA is routed to port 0

					char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
					size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
					send(rundev.sock_dev, outmsg.fullbuf, size, 0);

					rundev.switched = 0;
					debug_printf("switch back from VGA\n");
				}
			}

			//to do: send the out bound message if available
			while(1){
				if(datafifo_head(&rundev.consumer_fifo) == NULL){
					break;
				}
				else{
					debug_printf("send message from consumer fifo to socket\n");
					send(rundev.sock_dev, datafifo_head(&rundev.consumer_fifo)->arg.data, datafifo_head(&rundev.consumer_fifo)->arg.len, 0);
					datafifo_consumer_remove_head(&rundev.consumer_fifo);
				}
			}

			//check the incoming message
			FD_ZERO(&fdsr);
			FD_SET(rundev.sock_srv, &fdsr);
			FD_SET(rundev.sock_dev, &fdsr);

			int maxfd = (rundev.sock_srv>rundev.sock_dev)?rundev.sock_srv:rundev.sock_dev;
			tv.tv_sec = 0;
			tv.tv_usec = 1000;
			ret = select(maxfd+1, &fdsr, NULL, NULL, &tv);
			//debug_printf("select return\n");

			if(ret<0){
				debug_printf("select error\n");
				device_disconnected = 1;
				break;
			}

			if(ret==0){
				//debug_printf("select return 0\n");
				continue;
			}

			if(FD_ISSET(rundev.sock_srv, &fdsr)){
				int MsgGot;
				//debug_printf("sock_client readable\n");
				char buf[CMD_BUFSIZE];

				MsgGot = getMsg(rundev.sock_srv, buf);

				//exception handle
				if(MsgGot<0){
					debug_printf("disconnected to server\n");
					close(rundev.sock_srv);
					server_disconnected = 1;
					break;
				}

				//normal handle
				if(MsgGot == 0){
					debug_printf("Message from server uncompleted\n");
					usleep(1000);
				}

				if(MsgGot>0){
					debug_printf("Message got from server\n");
					MsgDump(buf);
					//To do: add further processing here
					SrvMsgHandle(&rundev, buf);
				}


			}

			if(FD_ISSET(rundev.sock_dev, &fdsr)){
				int MsgGot;
				//debug_printf("sock_device readable\n");
				char buf[CMD_BUFSIZE];

				MsgGot = getMsg(rundev.sock_dev, buf);

				//exception handle
				if(MsgGot<0){
					debug_printf("disconnected to device\n");
					device_disconnected = 1;
					break;
				}

				//normal handle
				if(MsgGot == 0){
					//debug_printf("Message from device uncompleted\n");
					usleep(1000);
				}

				if(MsgGot>0){
					//debug_printf("Message got from device\n");
					//MsgDump(buf);
					//To do: add further processing here
					DevMsgHandle(&rundev, buf);
				}


			}

		}

		//handle the server disconnected matter
		if(server_disconnected){
			server_disconnected = 0;
		}

		//handle the device disconnected matter
		if(device_disconnected){
			break;
		}
	}

	//ask the thread to exit then destroy the fifo graph
	int camhldctrl_exited = 0;
	int localsave_exited = 0;
	int videoanalyze_exited = 0;

	while(1){
		//cancel stream thread first
		if(!camhldctrl_exited){
			thread_videotracking_askexit(&camhldctrl);
			camhldctrl_exited = thread_videotracking_exited(&camhldctrl);
			continue;
		}

		if(!localsave_exited){
			thread_localsave_askexit(&localsave);
			localsave_exited = thread_localsave_exited(&localsave);
			continue;
		}

		if(!videoanalyze_exited){
			thread_analyze_askexit(&videoanalyze);
			videoanalyze_exited = thread_analyze_exited(&videoanalyze);
		}
		break;
	}

	debug_printf("process exit\n");
	exit(0);

#if 0
	sleep(30);
	for(i=0; i<4; i++){
		if(sock_list[i]){
			close(sock_list[i]);
			debug_printf("close the socket\n");
		}
	}
	sleep(30);
	exit(0);

#endif
}
#endif

int vmap_table_init(char *vmap_file)
{
	int i;
	for(i=0; i<6; i++){
		g_vmap_table[i].index = i+1;
		g_vmap_table[i].pip = 0;
		//vmap_table[i].analyze_type = 0;
		g_vmap_table[i].uart_channel = 0;
		g_vmap_table[i].function = i+1;
		if(g_vmap_table[i].function == 1){
			g_vmap_table[i].uart_channel = 1;
		}
		if(g_vmap_table[i].function == 2){
			g_vmap_table[i].uart_channel = 3;
		}
		g_vmap_table[i].cap_id = i+1;
	}
	g_vmap_table[5].cap_id = 0xB;
	return 0;
}

int vmap_table_get_capid_by_function(int function)
{
	int i;
	for(i=0; i<6; i++){
		if(g_vmap_table[i].function == function){
			return g_vmap_table[i].cap_id&0xf;
		}
	}
	return -1;
}

int vmap_table_get_index_by_function(int function)
{
	int i;
	for(i=0; i<6; i++){
		if(g_vmap_table[i].function == function){
			return i;
		}
	}
	return -1;
}

void vmap_table_reset_pip()
{
	int i;
	for(i=0; i<6; i++){
		if(g_vmap_table[i].pip){
			g_vmap_table[i].pip = 0;
		}
	}
}

void vmap_table_set_pip_by_function(int function)
{
	vmap_table_reset_pip();
	int i;
	for(i=0; i<6; i++){
		if(g_vmap_table[i].function == function){
			g_vmap_table[i].pip = 1;
			break;
		}
	}
}

int config_init(char *config_file)
{
	g_cfg.frame_width = 1920;
	g_cfg.frame_height = 1080;
	g_cfg.frame_rate = 30;
	g_cfg.bitrate = 1024;
	g_cfg.live_bitrate = 1024;
	g_cfg.IsResouceMode = 1;
	g_cfg.IsAutoInstuct = 0;
	g_cfg.num_of_CoreBoard = 4;
	g_cfg.VGADuration = 6;
	return 0;
}

void thread_listen_askexit(t_listen_thread_param *ptr)
{
	if(ptr->thread_canceled == 0){
		ptr->thread_canceled = 1;
	}
}

int thread_listen_exited(t_listen_thread_param *ptr)
{
	return ptr->thread_canceled==2?1:0;
}

void * thread_listen(void *param)
{
	pthread_detach(pthread_self());

	t_listen_thread_param *plisten = (t_listen_thread_param *)param;
	int sock_listen;
	struct sockaddr_in server_addr, client_addr;
	int opt;
	socklen_t sin_size;

	while(1){
		if(plisten->thread_canceled){
			break;
		}

		if(plisten->socket<0){
			plisten->senderr = 0;
			plisten->recverr = 0;
			opt = 1;
			sin_size = sizeof(client_addr);

			//open the server socket for listening
			if((sock_listen=socket(AF_INET, SOCK_STREAM, 0))==-1){
				debug_printf("open socket error\n");
				exit(1);
			}

			//set socket option
			if(setsockopt(sock_listen, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int))==-1){
				debug_printf("set socket option error\n");
				exit(1);
			}

			//memset(&server_addr, 0, sizeof(server_addr));

			server_addr.sin_family = AF_INET;
			server_addr.sin_port = htons(plisten->port);
			server_addr.sin_addr.s_addr = INADDR_ANY;
			memset(server_addr.sin_zero, '\0', sizeof(server_addr.sin_zero));

			//bind to specified port
			if(bind(sock_listen, (struct sockaddr *)&server_addr, sizeof(server_addr))==-1){
				debug_printf("bind error %d\n", plisten->port);
				exit(1);
			}

			//put the socket to listen
			if(listen(sock_listen, SOMAXCONN)==-1){
				debug_printf("listen error\n");
				exit(1);
			}

			//wait for connection
			if((plisten->socket=accept(sock_listen, (struct sockaddr *)&client_addr, &sin_size))==-1){
				debug_printf("accept error\n");
				exit(1);
			}

			if(plisten->port==2000){
				debug_printf("coreA accepted\n");
			}
			if(plisten->port==2001){
				debug_printf("coreB accepted\n");
			}
			if(plisten->port==2002){
				debug_printf("coreC accepted\n");
			}
			if(plisten->port==2003){
				debug_printf("coreD accepted\n");
			}
			if(plisten->port==3000){
				debug_printf("instructor accepted\n");
			}

			close(sock_listen);
		}
		else{
			sleep(1);
		}
	}
	plisten->thread_canceled = 2;
	pthread_exit(0);
}

void thread_coreA_askexit(t_coreA_thread_param *ptr)
{
	if(ptr->thread_canceled == 0){
		ptr->thread_canceled = 1;
	}
}

int thread_coreA_exited(t_coreA_thread_param *ptr)
{
	return ptr->thread_canceled==2?1:0;
}

void * thread_coreA_send(void *param)
{
	//debug_printf("thread_coreA_send running\n");
	pthread_detach(pthread_self());

	t_coreA_thread_param *thread_param = (t_coreA_thread_param *)param;
	int ret;
	int busy;

	while(1){
		busy = 0;
		//check exit
		if(thread_param->thread_canceled){
			break;
		}

		//check outbound message
#if 1
		if(datafifo_head(thread_param->Ctrl_consumer_fifo)){

			//debug_printf("send message from consumer fifo to socket\n");
			char cmd[CMD_SIZE+1];
			memcpy(cmd, &datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.data[VERSION_SIZE], CMD_SIZE);
			cmd[CMD_SIZE] = '\0';

			if(strcmp(cmd, COMMAND_INTERNAL)){
				if(thread_param->thread_listen_param.socket>=0 && (!thread_param->thread_listen_param.senderr)){
					ret=send(thread_param->thread_listen_param.socket, datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.data, datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.len, 0);
					if(ret<0){
						thread_param->thread_listen_param.senderr = 1;
					}
				}
			}
			datafifo_consumer_remove_head(thread_param->Ctrl_consumer_fifo);
			busy++;
		}
#else
		while(1){
			if(datafifo_head(thread_param->Ctrl_consumer_fifo) == NULL){
				break;
			}
			else{
				//debug_printf("send message from consumer fifo to socket\n");
				char cmd[CMD_SIZE+1];
				memcpy(cmd, &datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.data[VERSION_SIZE], CMD_SIZE);
				cmd[CMD_SIZE] = '\0';

				if(strcmp(cmd, COMMAND_INTERNAL)){
					if(thread_param->thread_listen_param.socket>=0){
						ret=send(thread_param->thread_listen_param.socket, datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.data, datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.len, 0);
						if(ret<0){
							close(thread_param->thread_listen_param.socket);
							thread_param->thread_listen_param.socket = -1;
						}
					}
				}
				datafifo_consumer_remove_head(thread_param->Ctrl_consumer_fifo);
			}
		}
#endif

#if 1
		if(datafifo_head(thread_param->UART_TX_consumer_fifo)){

			//debug_printf("send message from consumer fifo to socket\n");
			if(thread_param->thread_listen_param.socket>=0 && (!thread_param->thread_listen_param.senderr)){
				ret=send(thread_param->thread_listen_param.socket, datafifo_head(thread_param->UART_TX_consumer_fifo)->arg.data, datafifo_head(thread_param->UART_TX_consumer_fifo)->arg.len, 0);
				if(ret<0){
					thread_param->thread_listen_param.senderr = 1;
				}
			}
			datafifo_consumer_remove_head(thread_param->UART_TX_consumer_fifo);
			busy++;
		}
#else
		while(1){
			if(datafifo_head(thread_param->UART_TX_consumer_fifo) == NULL){
				break;
			}
			else{
				//debug_printf("send message from consumer fifo to socket\n");
				if(thread_param->thread_listen_param.socket>=0){
					ret=send(thread_param->thread_listen_param.socket, datafifo_head(thread_param->UART_TX_consumer_fifo)->arg.data, datafifo_head(thread_param->UART_TX_consumer_fifo)->arg.len, 0);
					if(ret<0){
						close(thread_param->thread_listen_param.socket);
						thread_param->thread_listen_param.socket = -1;
					}
				}
				datafifo_consumer_remove_head(thread_param->UART_TX_consumer_fifo);
			}
		}
#endif
		//check socket errors
		if(thread_param->thread_listen_param.recverr){
			thread_param->thread_listen_param.senderr = 1;
		}
		if(!busy){
			usleep(1000);
		}
	}
	pthread_exit(0);
}
void * thread_coreA(void * param)
{
	pthread_detach(pthread_self());

	t_coreA_thread_param *thread_param = (t_coreA_thread_param *)param;
	thread_param->thread_listen_param.socket = -1;
	thread_param->thread_listen_param.port = 2000;
	thread_param->thread_listen_param.thread_canceled = 0;
	thread_param->thread_listen_param.senderr = 0;
	thread_param->thread_listen_param.recverr = 0;

	//start thread_listen
	int ret;
	pthread_t thread_id;

	ret = pthread_create(&thread_id, NULL, thread_coreA_send, (void *)thread_param);
	if(ret<0){
		debug_printf("create thread_coreA_send failed\n");
		exit(0);
	}

	ret = pthread_create(&thread_id, NULL, thread_listen, (void *)&thread_param->thread_listen_param);
	if(ret<0){
		debug_printf("create thread_listen for coreA failed\n");
		exit(0);
	}

	fd_set fdsr;
	struct timeval tv;
	int busy;

	while(1){
		busy = 0;
		//check exit
		if(thread_param->thread_canceled){
			break;
		}

		//check inbound message
#if 1

		if(thread_param->thread_listen_param.socket>=0 && thread_param->thread_listen_param.recverr==0){
			FD_ZERO(&fdsr);
			FD_SET(thread_param->thread_listen_param.socket, &fdsr);

			int maxfd = thread_param->thread_listen_param.socket;
			tv.tv_sec = 0;
			tv.tv_usec = 0;
			ret = select(maxfd+1, &fdsr, NULL, NULL, &tv);
			//debug_printf("select return\n");

			if(ret<0){
				debug_printf("coreA select error\n");
				thread_param->thread_listen_param.recverr = 1;
				//break;
			}

			if(ret==0){
				//debug_printf("select return 0\n");
				//break;
			}

			if(FD_ISSET(thread_param->thread_listen_param.socket, &fdsr)){
				int MsgGot;
				//debug_printf("sock_client readable\n");
				char buf[CMD_BUFSIZE];

				MsgGot = getMsg(thread_param->thread_listen_param.socket, buf);

				//exception handle
				if(MsgGot<0){
					debug_printf("coreA disconnected to server\n");
					thread_param->thread_listen_param.recverr = 1;
					//break;
				}

				//normal handle
				if(MsgGot == 0){
					//debug_printf("Message from server uncompleted\n");
					//usleep(1000);
					//break;
				}

				if(MsgGot>0){
					//debug_printf("Message got from server\n");
					//MsgDump(buf);
					//To do: add further processing here
					DevAMsgHandle(thread_param, buf);
					busy++;
				}
			}
		}

		//check errors and close the socket
		if(thread_param->thread_listen_param.senderr && thread_param->thread_listen_param.recverr){
			close(thread_param->thread_listen_param.socket);
			thread_param->thread_listen_param.socket = -1;
		}

#else
		while(1){
			if(thread_param->thread_listen_param.socket>=0){
				FD_ZERO(&fdsr);
				FD_SET(thread_param->thread_listen_param.socket, &fdsr);

				int maxfd = thread_param->thread_listen_param.socket;
				tv.tv_sec = 0;
				tv.tv_usec = 1000;
				ret = select(maxfd+1, &fdsr, NULL, NULL, &tv);
				//debug_printf("select return\n");

				if(ret<0){
					debug_printf("select error\n");
					close(thread_param->thread_listen_param.socket);
					thread_param->thread_listen_param.socket = -1;
					break;
				}

				if(ret==0){
					//debug_printf("select return 0\n");
					break;
				}

				if(FD_ISSET(thread_param->thread_listen_param.socket, &fdsr)){
					int MsgGot;
					//debug_printf("sock_client readable\n");
					char buf[CMD_BUFSIZE];

					MsgGot = getMsg(thread_param->thread_listen_param.socket, buf);

					//exception handle
					if(MsgGot<0){
						debug_printf("disconnected to server\n");
						close(thread_param->thread_listen_param.socket);
						thread_param->thread_listen_param.socket = -1;
						break;
					}

					//normal handle
					if(MsgGot == 0){
						//debug_printf("Message from server uncompleted\n");
						usleep(1000);
						break;
					}

					if(MsgGot>0){
						//debug_printf("Message got from server\n");
						//MsgDump(buf);
						//To do: add further processing here
						DevAMsgHandle(thread_param, buf);
					}
				}
			}
			else{
				usleep(1000);
				break;
			}
		}
#endif

		if(!busy){
			usleep(1000);
		}
	}

	while(!thread_listen_exited(&thread_param->thread_listen_param)){
		thread_listen_askexit(&thread_param->thread_listen_param);
	}

	if(thread_param->thread_listen_param.socket>=0){
		close(thread_param->thread_listen_param.socket);
		thread_param->thread_listen_param.socket = -1;
	}
	thread_param->thread_canceled = 2;
	pthread_exit(0);
}

void thread_coreB_askexit(t_coreBC_thread_param *ptr)
{
	if(ptr->thread_canceled == 0){
		ptr->thread_canceled = 1;
	}
}

int thread_coreB_exited(t_coreBC_thread_param *ptr)
{
	return ptr->thread_canceled==2?1:0;
}

void * thread_coreB(void * param)
{
	pthread_detach(pthread_self());

	t_coreBC_thread_param *thread_param = (t_coreBC_thread_param *)param;
	thread_param->thread_listen_param.socket = -1;
	thread_param->thread_listen_param.port = 2001;
	thread_param->thread_listen_param.thread_canceled = 0;

	//start thread_listen
	int ret;
	pthread_t thread_id;
	ret = pthread_create(&thread_id, NULL, thread_listen, (void *)&thread_param->thread_listen_param);
	if(ret<0){
		debug_printf("create thread_listen for coreB failed\n");
		exit(0);
	}

	fd_set fdsr;
	struct timeval tv;
	int busy;

	while(1){
		busy = 0;
		//check exit
		if(thread_param->thread_canceled){
			break;
		}

		//check inbound message
#if 1

		if(thread_param->thread_listen_param.socket>=0){
			FD_ZERO(&fdsr);
			FD_SET(thread_param->thread_listen_param.socket, &fdsr);

			int maxfd = thread_param->thread_listen_param.socket;
			tv.tv_sec = 0;
			tv.tv_usec = 0;
			ret = select(maxfd+1, &fdsr, NULL, NULL, &tv);
			//debug_printf("select return\n");

			if(ret<0){
				debug_printf("coreB select error\n");
				close(thread_param->thread_listen_param.socket);
				thread_param->thread_listen_param.socket = -1;
				//break;
			}

			if(ret==0){
				//debug_printf("select return 0\n");
				//break;
			}

			if(FD_ISSET(thread_param->thread_listen_param.socket, &fdsr)){
				int MsgGot;
				//debug_printf("sock_client readable\n");
				char buf[CMD_BUFSIZE];

				MsgGot = getMsg(thread_param->thread_listen_param.socket, buf);

				//exception handle
				if(MsgGot<0){
					debug_printf("coreB disconnected to server\n");
					close(thread_param->thread_listen_param.socket);
					thread_param->thread_listen_param.socket = -1;
					//break;
				}

				//normal handle
				if(MsgGot == 0){
					//debug_printf("Message from server uncompleted\n");
					//usleep(1000);
					//break;
				}

				if(MsgGot>0){
					//debug_printf("Message got from server\n");
					//MsgDump(buf);
					//To do: add further processing here
					DevBCMsgHandle(thread_param, buf);
					busy++;
				}
			}
		}

#else
		while(1){
			if(thread_param->thread_listen_param.socket>=0){
				FD_ZERO(&fdsr);
				FD_SET(thread_param->thread_listen_param.socket, &fdsr);

				int maxfd = thread_param->thread_listen_param.socket;
				tv.tv_sec = 0;
				tv.tv_usec = 1000;
				ret = select(maxfd+1, &fdsr, NULL, NULL, &tv);
				//debug_printf("select return\n");

				if(ret<0){
					debug_printf("select error\n");
					close(thread_param->thread_listen_param.socket);
					thread_param->thread_listen_param.socket = -1;
					break;
				}

				if(ret==0){
					//debug_printf("select return 0\n");
					break;
				}

				if(FD_ISSET(thread_param->thread_listen_param.socket, &fdsr)){
					int MsgGot;
					//debug_printf("sock_client readable\n");
					char buf[CMD_BUFSIZE];

					MsgGot = getMsg(thread_param->thread_listen_param.socket, buf);

					//exception handle
					if(MsgGot<0){
						debug_printf("disconnected to server\n");
						close(thread_param->thread_listen_param.socket);
						thread_param->thread_listen_param.socket = -1;
						break;
					}

					//normal handle
					if(MsgGot == 0){
						//debug_printf("Message from server uncompleted\n");
						usleep(1000);
						break;
					}

					if(MsgGot>0){
						//debug_printf("Message got from server\n");
						//MsgDump(buf);
						//To do: add further processing here
						DevBCMsgHandle(thread_param, buf);
					}
				}
			}
			else{
				usleep(1000);
				break;
			}
		}
#endif

#if 1
		if(datafifo_head(thread_param->Ctrl_consumer_fifo)){

			//debug_printf("send message from consumer fifo to socket\n");
			char cmd[CMD_SIZE+1];
			memcpy(cmd, &datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.data[VERSION_SIZE], CMD_SIZE);
			cmd[CMD_SIZE] = '\0';

			if(strcmp(cmd, COMMAND_INTERNAL)){
				if(thread_param->thread_listen_param.socket>=0){
					ret=send(thread_param->thread_listen_param.socket, datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.data, datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.len, 0);
					if(ret<0){
						close(thread_param->thread_listen_param.socket);
						thread_param->thread_listen_param.socket = -1;
					}
				}
			}
			datafifo_consumer_remove_head(thread_param->Ctrl_consumer_fifo);
			busy++;
		}
#else
		//check outbound message
		while(1){
			if(datafifo_head(thread_param->Ctrl_consumer_fifo) == NULL){
				break;
			}
			else{
				//debug_printf("send message from consumer fifo to socket\n");
				char cmd[CMD_SIZE+1];
				memcpy(cmd, &datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.data[VERSION_SIZE], CMD_SIZE);
				cmd[CMD_SIZE] = '\0';

				if(strcmp(cmd, COMMAND_INTERNAL)){
					if(thread_param->thread_listen_param.socket>=0){
						ret=send(thread_param->thread_listen_param.socket, datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.data, datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.len, 0);
						if(ret<0){
							close(thread_param->thread_listen_param.socket);
							thread_param->thread_listen_param.socket = -1;
						}
					}
				}
				datafifo_consumer_remove_head(thread_param->Ctrl_consumer_fifo);
			}
		}
#endif

#if 1
		if(datafifo_head(thread_param->UART_TX_consumer_fifo)){

			//debug_printf("send message from consumer fifo to socket\n");
			if(thread_param->thread_listen_param.socket>=0){
				ret=send(thread_param->thread_listen_param.socket, datafifo_head(thread_param->UART_TX_consumer_fifo)->arg.data, datafifo_head(thread_param->UART_TX_consumer_fifo)->arg.len, 0);
				if(ret<0){
					close(thread_param->thread_listen_param.socket);
					thread_param->thread_listen_param.socket = -1;
				}
			}
			datafifo_consumer_remove_head(thread_param->UART_TX_consumer_fifo);
			busy++;
		}
#else
		while(1){
			if(datafifo_head(thread_param->UART_TX_consumer_fifo) == NULL){
				break;
			}
			else{
				//debug_printf("send message from consumer fifo to socket\n");
				if(thread_param->thread_listen_param.socket>=0){
					ret=send(thread_param->thread_listen_param.socket, datafifo_head(thread_param->UART_TX_consumer_fifo)->arg.data, datafifo_head(thread_param->UART_TX_consumer_fifo)->arg.len, 0);
					if(ret<0){
						close(thread_param->thread_listen_param.socket);
						thread_param->thread_listen_param.socket = -1;
					}
				}
				datafifo_consumer_remove_head(thread_param->UART_TX_consumer_fifo);
			}
		}
#endif
		if(!busy){
			usleep(1000);
		}
	}

	while(!thread_listen_exited(&thread_param->thread_listen_param)){
		thread_listen_askexit(&thread_param->thread_listen_param);
	}

	if(thread_param->thread_listen_param.socket>=0){
		close(thread_param->thread_listen_param.socket);
		thread_param->thread_listen_param.socket = -1;
	}

	thread_param->thread_canceled = 2;
	pthread_exit(0);
}

void thread_coreC_askexit(t_coreBC_thread_param *ptr)
{
	if(ptr->thread_canceled == 0){
		ptr->thread_canceled = 1;
	}
}

int thread_coreC_exited(t_coreBC_thread_param *ptr)
{
	return ptr->thread_canceled==2?1:0;
}

void * thread_coreC(void * param)
{
	pthread_detach(pthread_self());

	t_coreBC_thread_param *thread_param = (t_coreBC_thread_param *)param;
	thread_param->thread_listen_param.socket = -1;
	thread_param->thread_listen_param.port = 2002;
	thread_param->thread_listen_param.thread_canceled = 0;

	//start thread_listen
	int ret;
	pthread_t thread_id;
	ret = pthread_create(&thread_id, NULL, thread_listen, (void *)&thread_param->thread_listen_param);
	if(ret<0){
		debug_printf("create thread_listen for coreC failed\n");
		exit(0);
	}

	fd_set fdsr;
	struct timeval tv;
	int busy;

	while(1){
		busy = 0;
		//check exit
		if(thread_param->thread_canceled){
			break;
		}

		//check inbound message
#if 1

		if(thread_param->thread_listen_param.socket>=0){
			FD_ZERO(&fdsr);
			FD_SET(thread_param->thread_listen_param.socket, &fdsr);

			int maxfd = thread_param->thread_listen_param.socket;
			tv.tv_sec = 0;
			tv.tv_usec = 0;
			ret = select(maxfd+1, &fdsr, NULL, NULL, &tv);
			//debug_printf("select return\n");

			if(ret<0){
				debug_printf("coreC select error\n");
				close(thread_param->thread_listen_param.socket);
				thread_param->thread_listen_param.socket = -1;
				//break;
			}

			if(ret==0){
				//debug_printf("select return 0\n");
				//break;
			}

			if(FD_ISSET(thread_param->thread_listen_param.socket, &fdsr)){
				int MsgGot;
				//debug_printf("sock_client readable\n");
				char buf[CMD_BUFSIZE];

				MsgGot = getMsg(thread_param->thread_listen_param.socket, buf);

				//exception handle
				if(MsgGot<0){
					debug_printf("coreC disconnected to server\n");
					close(thread_param->thread_listen_param.socket);
					thread_param->thread_listen_param.socket = -1;
					//break;
				}

				//normal handle
				if(MsgGot == 0){
					//debug_printf("Message from server uncompleted\n");
					//usleep(1000);
					//break;
				}

				if(MsgGot>0){
					//debug_printf("Message got from server\n");
					//MsgDump(buf);
					//To do: add further processing here
					DevBCMsgHandle(thread_param, buf);
					busy++;
				}
			}
		}

#else
		while(1){
			if(thread_param->thread_listen_param.socket>=0){
				FD_ZERO(&fdsr);
				FD_SET(thread_param->thread_listen_param.socket, &fdsr);

				int maxfd = thread_param->thread_listen_param.socket;
				tv.tv_sec = 0;
				tv.tv_usec = 1000;
				ret = select(maxfd+1, &fdsr, NULL, NULL, &tv);
				//debug_printf("select return\n");

				if(ret<0){
					debug_printf("select error\n");
					close(thread_param->thread_listen_param.socket);
					thread_param->thread_listen_param.socket = -1;
					break;
				}

				if(ret==0){
					//debug_printf("select return 0\n");
					break;
				}

				if(FD_ISSET(thread_param->thread_listen_param.socket, &fdsr)){
					int MsgGot;
					//debug_printf("sock_client readable\n");
					char buf[CMD_BUFSIZE];

					MsgGot = getMsg(thread_param->thread_listen_param.socket, buf);

					//exception handle
					if(MsgGot<0){
						debug_printf("disconnected to server\n");
						close(thread_param->thread_listen_param.socket);
						thread_param->thread_listen_param.socket = -1;
						break;
					}

					//normal handle
					if(MsgGot == 0){
						//debug_printf("Message from server uncompleted\n");
						usleep(1000);
						break;
					}

					if(MsgGot>0){
						//debug_printf("Message got from server\n");
						//MsgDump(buf);
						//To do: add further processing here
						DevBCMsgHandle(thread_param, buf);
					}
				}
			}
			else{
				usleep(1000);
				break;
			}
		}
#endif
		//check outbound message
#if 1

		if(datafifo_head(thread_param->Ctrl_consumer_fifo)){

			//debug_printf("send message from consumer fifo to socket\n");
			if(thread_param->thread_listen_param.socket>=0){
				ret=send(thread_param->thread_listen_param.socket, datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.data, datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.len, 0);
				if(ret<0){
					close(thread_param->thread_listen_param.socket);
					thread_param->thread_listen_param.socket = -1;
				}
			}
			datafifo_consumer_remove_head(thread_param->Ctrl_consumer_fifo);
			busy ++;
		}

#else
		while(1){
			if(datafifo_head(thread_param->Ctrl_consumer_fifo) == NULL){
				break;
			}
			else{
				//debug_printf("send message from consumer fifo to socket\n");
				if(thread_param->thread_listen_param.socket>=0){
					ret=send(thread_param->thread_listen_param.socket, datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.data, datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.len, 0);
					if(ret<0){
						close(thread_param->thread_listen_param.socket);
						thread_param->thread_listen_param.socket = -1;
					}
				}
				datafifo_consumer_remove_head(thread_param->Ctrl_consumer_fifo);
			}
		}
#endif

#if 1
		if(datafifo_head(thread_param->UART_TX_consumer_fifo)){

			//debug_printf("send message from consumer fifo to socket\n");
			char cmd[CMD_SIZE+1];
			memcpy(cmd, &datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.data[VERSION_SIZE], CMD_SIZE);
			cmd[CMD_SIZE] = '\0';

			if(strcmp(cmd, COMMAND_INTERNAL)){
				if(thread_param->thread_listen_param.socket>=0){
					ret=send(thread_param->thread_listen_param.socket, datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.data, datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.len, 0);
					if(ret<0){
						close(thread_param->thread_listen_param.socket);
						thread_param->thread_listen_param.socket = -1;
					}
				}
			}
			datafifo_consumer_remove_head(thread_param->UART_TX_consumer_fifo);
			busy++;
		}
#else
		while(1){
			if(datafifo_head(thread_param->UART_TX_consumer_fifo) == NULL){
				break;
			}
			else{
				//debug_printf("send message from consumer fifo to socket\n");
				char cmd[CMD_SIZE+1];
				memcpy(cmd, &datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.data[VERSION_SIZE], CMD_SIZE);
				cmd[CMD_SIZE] = '\0';

				if(strcmp(cmd, COMMAND_INTERNAL)){
					if(thread_param->thread_listen_param.socket>=0){
						ret=send(thread_param->thread_listen_param.socket, datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.data, datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.len, 0);
						if(ret<0){
							close(thread_param->thread_listen_param.socket);
							thread_param->thread_listen_param.socket = -1;
						}
					}
				}
				datafifo_consumer_remove_head(thread_param->UART_TX_consumer_fifo);
			}
		}
#endif
		if(!busy){
			usleep(1000);
		}
	}

	while(!thread_listen_exited(&thread_param->thread_listen_param)){
		thread_listen_askexit(&thread_param->thread_listen_param);
	}

	if(thread_param->thread_listen_param.socket>=0){
		close(thread_param->thread_listen_param.socket);
		thread_param->thread_listen_param.socket = -1;
	}

	thread_param->thread_canceled = 2;

	pthread_exit(0);
}

void thread_coreD_askexit(t_coreD_thread_param *ptr)
{
	if(ptr->thread_canceled == 0){
		ptr->thread_canceled = 1;
	}
}

int thread_coreD_exited(t_coreD_thread_param *ptr)
{
	return ptr->thread_canceled==2?1:0;
}

void * thread_coreD(void * param)
{
	pthread_detach(pthread_self());

	t_coreD_thread_param *thread_param = (t_coreD_thread_param *)param;
	thread_param->thread_listen_param.socket = -1;
	thread_param->thread_listen_param.port = 2003;
	thread_param->thread_listen_param.thread_canceled = 0;

	//start thread_listen
	int ret;
	pthread_t thread_id;
	ret = pthread_create(&thread_id, NULL, thread_listen, (void *)&thread_param->thread_listen_param);
	if(ret<0){
		debug_printf("create thread_listen for coreD failed\n");
		exit(0);
	}

	fd_set fdsr;
	struct timeval tv;
	int busy;

	while(1){
		busy = 0;
		//check exit
		if(thread_param->thread_canceled){
			break;
		}

		//check inbound message
#if 1

		if(thread_param->thread_listen_param.socket>=0){
			FD_ZERO(&fdsr);
			FD_SET(thread_param->thread_listen_param.socket, &fdsr);

			int maxfd = thread_param->thread_listen_param.socket;
			tv.tv_sec = 0;
			tv.tv_usec = 0;
			ret = select(maxfd+1, &fdsr, NULL, NULL, &tv);
			//debug_printf("select return\n");

			if(ret<0){
				debug_printf("coreD select error\n");
				close(thread_param->thread_listen_param.socket);
				thread_param->thread_listen_param.socket = -1;
				//break;
			}

			if(ret==0){
				//debug_printf("select return 0\n");
				//break;
			}

			if(FD_ISSET(thread_param->thread_listen_param.socket, &fdsr)){
				int MsgGot;
				//debug_printf("sock_client readable\n");
				char buf[CMD_BUFSIZE];

				MsgGot = getMsg(thread_param->thread_listen_param.socket, buf);

				//exception handle
				if(MsgGot<0){
					debug_printf("coreD disconnected to server\n");
					close(thread_param->thread_listen_param.socket);
					thread_param->thread_listen_param.socket = -1;
					//break;
				}

				//normal handle
				if(MsgGot == 0){
					//debug_printf("Message from server uncompleted\n");
					//usleep(1000);
					//break;
				}

				if(MsgGot>0){
					//debug_printf("Message got from server\n");
					//MsgDump(buf);
					//To do: add further processing here
					DevDMsgHandle(thread_param, buf);
					busy++;
				}
			}
		}
#else
		while(1){
			if(thread_param->thread_listen_param.socket>=0){
				FD_ZERO(&fdsr);
				FD_SET(thread_param->thread_listen_param.socket, &fdsr);

				int maxfd = thread_param->thread_listen_param.socket;
				tv.tv_sec = 0;
				tv.tv_usec = 1000;
				ret = select(maxfd+1, &fdsr, NULL, NULL, &tv);
				//debug_printf("select return\n");

				if(ret<0){
					debug_printf("select error\n");
					close(thread_param->thread_listen_param.socket);
					thread_param->thread_listen_param.socket = -1;
					break;
				}

				if(ret==0){
					//debug_printf("select return 0\n");
					break;
				}

				if(FD_ISSET(thread_param->thread_listen_param.socket, &fdsr)){
					int MsgGot;
					//debug_printf("sock_client readable\n");
					char buf[CMD_BUFSIZE];

					MsgGot = getMsg(thread_param->thread_listen_param.socket, buf);

					//exception handle
					if(MsgGot<0){
						debug_printf("disconnected to server\n");
						close(thread_param->thread_listen_param.socket);
						thread_param->thread_listen_param.socket = -1;
						break;
					}

					//normal handle
					if(MsgGot == 0){
						//debug_printf("Message from server uncompleted\n");
						usleep(1000);
						break;
					}

					if(MsgGot>0){
						//debug_printf("Message got from server\n");
						//MsgDump(buf);
						//To do: add further processing here
						DevDMsgHandle(thread_param, buf);
					}
				}
			}
			else{
				usleep(1000);
				break;
			}
		}
#endif
		//check outbound message
#if 1
		if(datafifo_head(thread_param->Ctrl_consumer_fifo)){
			//debug_printf("send message from consumer fifo to socket\n");
			char cmd[CMD_SIZE+1];
			memcpy(cmd, &datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.data[VERSION_SIZE], CMD_SIZE);
			cmd[CMD_SIZE] = '\0';

			if(strcmp(cmd, COMMAND_INTERNAL)){
				if(thread_param->thread_listen_param.socket>=0){
					ret=send(thread_param->thread_listen_param.socket, datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.data, datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.len, 0);
					if(ret<0){
						close(thread_param->thread_listen_param.socket);
						thread_param->thread_listen_param.socket = -1;
					}
				}
			}
			datafifo_consumer_remove_head(thread_param->Ctrl_consumer_fifo);
			busy++;
		}
#else
		while(1){
			if(datafifo_head(thread_param->Ctrl_consumer_fifo) == NULL){
				break;
			}
			else{
				//debug_printf("send message from consumer fifo to socket\n");
				char cmd[CMD_SIZE+1];
				memcpy(cmd, &datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.data[VERSION_SIZE], CMD_SIZE);
				cmd[CMD_SIZE] = '\0';

				if(strcmp(cmd, COMMAND_INTERNAL)){
					if(thread_param->thread_listen_param.socket>=0){
						ret=send(thread_param->thread_listen_param.socket, datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.data, datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.len, 0);
						if(ret<0){
							close(thread_param->thread_listen_param.socket);
							thread_param->thread_listen_param.socket = -1;
						}
					}
				}
				datafifo_consumer_remove_head(thread_param->Ctrl_consumer_fifo);
			}
		}
#endif
		if(!busy){
			usleep(1000);
		}
	}

	while(!thread_listen_exited(&thread_param->thread_listen_param)){
		thread_listen_askexit(&thread_param->thread_listen_param);
	}

	if(thread_param->thread_listen_param.socket>=0){
		close(thread_param->thread_listen_param.socket);
		thread_param->thread_listen_param.socket = -1;
	}

	thread_param->thread_canceled = 2;
	pthread_exit(0);
}

void thread_analyze_askexit(t_videoanalyze_thread_param *ptr)
{
	if(ptr->thread_canceled == 0){
		ptr->thread_canceled = 1;
	}
}

int thread_analyze_exited(t_coreD_thread_param *p_core)
{
	return p_core->thread_canceled==2?1:0;
}

void * thread_analyze(void *param)
{
	pthread_detach(pthread_self());

	t_videoanalyze_thread_param *thread_param = (t_videoanalyze_thread_param *)param;
	unsigned char* p_map = NULL;

	FILE *fd = NULL;
	FILE *fd1 = NULL;
	int seq_num = 0;
#if 1
	fd = fopen("sample.raw", "w");
	if(!fd){
		debug_printf("could not open the file sample.raw\n");
	}

	fd1 = fopen("sample.seq", "w");
	if(!fd1){
		debug_printf("could not open the file sample.seq\n");
	}
#endif

	AppInit();

	while(1){
		if(thread_param->thread_canceled){
			break;
		}

		if(datafifo_head(thread_param->consumer_fifo)){
			if(fd){
				fwrite(datafifo_head(thread_param->consumer_fifo)->arg.data, 1, datafifo_head(thread_param->consumer_fifo)->arg.len, fd);
			}
			p_map = datafifo_head(thread_param->consumer_fifo)->arg.data;
			int frame_num = datafifo_head(thread_param->consumer_fifo)->arg.timestamp;
			if(fd1){
				fprintf(fd1, "%d:%d\n", seq_num, frame_num);
			}
			seq_num++;
			DetectFrame((unsigned char *)p_map, frame_num);

		    int *pPtr = (int *)p_map;
		  	//printf("rect %d, (%d, %d) (%d, %d)\n", pPtr[0], pPtr[1], pPtr[2], pPtr[3], pPtr[4]);
		  	EventVID2df(thread_param->producer_fifo, pPtr);
			datafifo_consumer_remove_head(thread_param->consumer_fifo);
		}
		usleep(1000);
	}

	AppDeinit();
	//fclose(fd);
	thread_param->thread_canceled=2;
	pthread_exit(0);
}

void * thread_mp4format(void *arg)
{
	char cmdstr[256];


	t_mp4format *p_mp4format = (t_mp4format *)arg;
	pthread_detach(pthread_self());

	//add handling here
	strcpy(cmdstr, "MP4Box -add ");
	strcat(cmdstr, p_mp4format->filename);
	strcat(cmdstr, ".264 -fps ");
	strcat(cmdstr, p_mp4format->fps);
	if(p_mp4format->has_audio){
		strcat(cmdstr, " -add ");
		strcat(cmdstr, p_mp4format->filename);
		strcat(cmdstr, ".aac");
	}
	strcat(cmdstr, " -new ");
	strcat(cmdstr, p_mp4format->filename);
	strcat(cmdstr, ".mp4");
	//debug_printf("%s\n", cmdstr);

	free(arg);

	int ret;
	ret = system(cmdstr);
	if(ret<0){
		printf("error in calling system\n");
	}
	else{
		printf("success\n");
	}
	pthread_exit(0);
}

void * thread_connect_liveserver(void *param)
{
	pthread_detach(pthread_self());

	t_localsave_thread_param *p_localsave = (t_localsave_thread_param *)param;
	struct sockaddr_in cli;

	while(1){
		if(p_localsave->sock==-1){
			cli.sin_family = AF_INET;
			cli.sin_port = htons(8888);
			cli.sin_addr.s_addr = inet_addr("127.0.0.1");

			int sock=-1;
			sock = socket(AF_INET, SOCK_STREAM, 0);
			if(sock<0){
				debug_printf("open socket error\n");
				exit(1);
			}
			//debug_printf("connecting to streaming server...");

			//set the send buf of the socket to ensure there is enough buffer to contain the data sent at a time
			int sndbufsize = 500*1024;
			if(setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const void *)&sndbufsize, sizeof(sndbufsize))==-1){
				debug_printf("setsockeopt failed\n");
			}

			if(connect(sock, (struct sockaddr*)&cli, sizeof(cli))<0){
				//debug_printf("could not connect to streaming server\n");
				close(sock);
				sock = -1;
			}
			else{
				debug_printf("connected\n");
				p_localsave->sock = sock;
			}
		}
		sleep(1);
	}

	pthread_exit(0);
}

void thread_localsave_askexit(t_localsave_thread_param *ptr)
{
	if(ptr->thread_canceled == 0){
		ptr->thread_canceled = 1;
	}
}

int thread_localsave_exited(t_localsave_thread_param *ptr)
{
	return ptr->thread_canceled==2?1:0;
}

int streamframe(t_localsave_thread_param *p_localsave, int flag, unsigned int *frame_num)
{
	if(p_localsave->sock != -1){
		char pkt_buf[STREAMING_PACKET_HEADER_LENGTH];
		t_streaming_packet_header *ptr = (t_streaming_packet_header *)pkt_buf;

		ptr->flag = flag;
		ptr->size = datafifo_head(p_localsave->av_consumer_fifo)->arg.len;
		ptr->type = PACKET_TYPE_VIDEO;
		//ptr->timestamp = datafifo_head(p_localsave->av_consumer_fifo)->arg.timestamp;
		ptr->timestamp = (*frame_num)*1000/g_cfg.frame_rate;
		int ret = send(p_localsave->sock, pkt_buf, STREAMING_PACKET_HEADER_LENGTH, 0);
		if(ret<STREAMING_PACKET_HEADER_LENGTH){
			close(p_localsave->sock);
			p_localsave->sock = -1;
		}
		if(p_localsave->sock != -1){
			ret = send(p_localsave->sock, datafifo_head(p_localsave->av_consumer_fifo)->arg.data, datafifo_head(p_localsave->av_consumer_fifo)->arg.len, 0);
			if(ret<datafifo_head(p_localsave->av_consumer_fifo)->arg.len){
				close(p_localsave->sock);
				p_localsave->sock = -1;
			}
			(*frame_num)++;
		}
	}
	return 0;
}

void * thread_localsave(void * param)
{
	pthread_detach(pthread_self());
	debug_printf("Thread localsave running...\n");
	//int i;
	t_localsave_thread_param *p_localsave = (t_localsave_thread_param *)param;
	unsigned int frame_count = 0, aframe_count = 0;
	unsigned int v1_frames = 0, v2_frames = 0, v3_frames = 0, v4_frames = 0, v5_frames = 0;
	//unsigned int begin_ts, timestamp;
	FILE *vfd = NULL, *afd = NULL, *pfd = NULL;
	FILE *VGA_fd = NULL, *V1_fd = NULL, *V2_fd = NULL, *V3_fd = NULL, *V4_fd = NULL, *V5_fd = NULL, *V6_fd = NULL;
	char filename[256], tmpfilename[256], save_dir[256], VGA_dir[256], MUX_dir[256], V1_dir[256], V2_dir[256], V3_dir[256], V4_dir[256], V5_dir[256], V6_dir[256];
	//int timeout = 0;
	unsigned int live_vframes, live_aframes;
#if 0
	struct sockaddr_in cli;

	cli.sin_family = AF_INET;
	cli.sin_port = htons(8888);
	cli.sin_addr.s_addr = inet_addr("127.0.0.1");

	p_localsave->sock = socket(AF_INET, SOCK_STREAM, 0);
	if(p_localsave->sock<0){
		debug_printf("open socket error\n");
		exit(1);
	}
	debug_printf("connecting to streaming server...");

	//set the send buf of the socket to ensure there is enough buffer to contain the data sent at a time
	int sndbufsize = 500*1024;
	if(setsockopt(p_localsave->sock, SOL_SOCKET, SO_SNDBUF, (const void *)&sndbufsize, sizeof(sndbufsize))==-1){
		debug_printf("setsockeopt failed\n");
	}

	if(connect(p_localsave->sock, (struct sockaddr*)&cli, sizeof(cli))<0){
		debug_printf("could not connect to streaming server\n");
		close(p_localsave->sock);
		p_localsave->sock = -1;
	}
	else{
		debug_printf("connected\n");
	}
#else
	pthread_t pt_connect_liveserver_id;
	pthread_create(&pt_connect_liveserver_id, NULL, thread_connect_liveserver, (void *)param);
#endif

	p_localsave->Vstore_status = LOCALSAVE_IDLE;
	p_localsave->Vlive_status = LOCALSAVE_IDLE;
	p_localsave->VGA_status = LOCALSAVE_IDLE;
	p_localsave->V1_status = LOCALSAVE_IDLE;
	p_localsave->V2_status = LOCALSAVE_IDLE;
	p_localsave->V3_status = LOCALSAVE_IDLE;
	p_localsave->V4_status = LOCALSAVE_IDLE;
	p_localsave->V5_status = LOCALSAVE_IDLE;
	p_localsave->V6_status = LOCALSAVE_IDLE;

	filename[0] = '\0';
	strcpy(save_dir, "./");
	strcat(save_dir, CfgDir);

	strcpy(MUX_dir, save_dir);
	strcat(MUX_dir, "MUX/");
	debug_printf("%s\n", MUX_dir);
	if(access(MUX_dir, F_OK)==-1){
		debug_printf("directory not exist, create it\n");
		if(mkdir(MUX_dir,0777) == -1){
			debug_printf("create directory failed\n");
		}
	}

	strcpy(VGA_dir, save_dir);
	strcat(VGA_dir, "VGA/");
	debug_printf("%s\n", VGA_dir);
	if(access(VGA_dir, F_OK)==-1){
		debug_printf("directory not exist, create it\n");
		if(mkdir(VGA_dir,0777) == -1){
			debug_printf("create directory failed\n");
		}
	}

	strcpy(V1_dir, save_dir);
	strcat(V1_dir, "V1/");
	debug_printf("%s\n", V1_dir);
	if(access(V1_dir, F_OK)==-1){
		debug_printf("directory not exist, create it\n");
		if(mkdir(V1_dir,0777) == -1){
			debug_printf("create directory failed\n");
		}
	}

	strcpy(V2_dir, save_dir);
	strcat(V2_dir, "V2/");
	debug_printf("%s\n", V2_dir);
	if(access(V2_dir, F_OK)==-1){
		debug_printf("directory not exist, create it\n");
		if(mkdir(V2_dir,0777) == -1){
			debug_printf("create directory failed\n");
		}
	}

	strcpy(V3_dir, save_dir);
	strcat(V3_dir, "V3/");
	debug_printf("%s\n", V3_dir);
	if(access(V3_dir, F_OK)==-1){
		debug_printf("directory not exist, create it\n");
		if(mkdir(V3_dir,0777) == -1){
			debug_printf("create directory failed\n");
		}
	}

	strcpy(V4_dir, save_dir);
	strcat(V4_dir, "V4/");
	debug_printf("%s\n", V4_dir);
	if(access(V4_dir, F_OK)==-1){
		debug_printf("directory not exist, create it\n");
		if(mkdir(V4_dir,0777) == -1){
			debug_printf("create directory failed\n");
		}
	}

	strcpy(V5_dir, save_dir);
	strcat(V5_dir, "V5/");
	debug_printf("%s\n", V5_dir);
	if(access(V5_dir, F_OK)==-1){
		debug_printf("directory not exist, create it\n");
		if(mkdir(V5_dir,0777) == -1){
			debug_printf("create directory failed\n");
		}
	}

	strcpy(V6_dir, save_dir);
	strcat(V6_dir, "V6/");
	debug_printf("%s\n", V6_dir);
	if(access(V6_dir, F_OK)==-1){
		debug_printf("directory not exist, create it\n");
		if(mkdir(V6_dir,0777) == -1){
			debug_printf("create directory failed\n");
		}
	}

	while(1){


		if(p_localsave->thread_canceled){
			if(//p_localsave->Audio_status == LOCALSAVE_IDLE
					p_localsave->Vstore_status == LOCALSAVE_IDLE
					&& p_localsave->Vlive_status == LOCALSAVE_IDLE
					&& p_localsave->VGA_status == LOCALSAVE_IDLE
					&& p_localsave->V1_status == LOCALSAVE_IDLE
					&& p_localsave->V2_status == LOCALSAVE_IDLE
					&& p_localsave->V3_status == LOCALSAVE_IDLE
					&& p_localsave->V4_status == LOCALSAVE_IDLE
					&& p_localsave->V5_status == LOCALSAVE_IDLE
					&& p_localsave->V6_status == LOCALSAVE_IDLE){
				break;
			}
			else{
				//p_localsave->Audio_status = LOCALSAVE_STOP;
				p_localsave->Vstore_status = LOCALSAVE_STOP;
				p_localsave->Vlive_status = LOCALSAVE_STOP;
				p_localsave->VGA_status = LOCALSAVE_STOP;
				p_localsave->V1_status = LOCALSAVE_STOP;
				p_localsave->V2_status = LOCALSAVE_STOP;
				p_localsave->V3_status = LOCALSAVE_STOP;
				p_localsave->V4_status = LOCALSAVE_STOP;
				p_localsave->V5_status = LOCALSAVE_STOP;
				p_localsave->V6_status = LOCALSAVE_STOP;
			}
		}


		//check command fifo and change state accordingly
		if(datafifo_head(p_localsave->Ctrl_consumer_fifo)){

 			debug_printf("thread_localsave receive message\n");

 			char *Msg = (char *)datafifo_head(p_localsave->Ctrl_consumer_fifo)->arg.data;
 			char cmd[CMD_SIZE+1];
 			unsigned int len;
 			char *ptr;
 			//int state_flag;

 			memcpy(cmd, &Msg[VERSION_SIZE], CMD_SIZE);
 			cmd[CMD_SIZE] = '\0';

            char2hex(&Msg[VERSION_SIZE+CMD_SIZE], &len, DATA_LEN_SIZE);
            //debug_printf("Total length of attributes: %d(%x)\n", len, len);

            ptr = &Msg[PACKET_HEADER_SIZE];

            if(!strcmp(cmd, COMMAND_GETDATA)){
            	while(len){
            		unsigned int name_len, value_len;
            		char attr_name[16];
            		char attr_value[1536];

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

            		if(!strcmp(attr_name, "Type")){
            			if(!strcmp(attr_value, "All")){
            				p_localsave->Vstore_status = LOCALSAVE_START;
            				p_localsave->Vlive_status = LOCALSAVE_START;
            				if(g_cfg.IsResouceMode){
            					p_localsave->VGA_status = LOCALSAVE_START;
            					p_localsave->V1_status = LOCALSAVE_START;
            					p_localsave->V2_status = LOCALSAVE_START;
            					p_localsave->V3_status = LOCALSAVE_START;
            					p_localsave->V4_status = LOCALSAVE_START;
            					p_localsave->V5_status = LOCALSAVE_START;
            					p_localsave->V6_status = LOCALSAVE_START;
            				}
            				live_vframes = 0;
            				live_aframes = 0;
            			}
            			else{
            				p_localsave->Vstore_status = LOCALSAVE_STOP;
            				p_localsave->Vlive_status = LOCALSAVE_STOP;
            				if(g_cfg.IsResouceMode){
            					p_localsave->VGA_status = LOCALSAVE_STOP;
            					p_localsave->V1_status = LOCALSAVE_STOP;
            					p_localsave->V2_status = LOCALSAVE_STOP;
            					p_localsave->V3_status = LOCALSAVE_STOP;
            					p_localsave->V4_status = LOCALSAVE_STOP;
            					p_localsave->V5_status = LOCALSAVE_STOP;
            					p_localsave->V6_status = LOCALSAVE_STOP;
            				}
            			}
            		}
            	}
            }

            if(!strcmp(cmd,COMMAND_INTERNAL)){
            	while(len){
            		unsigned int name_len, value_len;
            		char attr_name[16];
            		char attr_value[1536];

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

            		if(!strcmp(attr_name, "Pause")){
        				p_localsave->Vstore_status = LOCALSAVE_PAUSE;
        				p_localsave->Vlive_status = LOCALSAVE_PAUSE;
        				fprintf(pfd, "PAUSE: %d\n", frame_count);
        				if(g_cfg.IsResouceMode){
        					p_localsave->VGA_status = LOCALSAVE_PAUSE;
        					p_localsave->V1_status = LOCALSAVE_PAUSE;
        					p_localsave->V2_status = LOCALSAVE_PAUSE;
        					p_localsave->V3_status = LOCALSAVE_PAUSE;
        					p_localsave->V4_status = LOCALSAVE_PAUSE;
        					p_localsave->V5_status = LOCALSAVE_PAUSE;
        					p_localsave->V6_status = LOCALSAVE_PAUSE;
        				}
            		}

            		if(!strcmp(attr_name, "Resume")){
        				p_localsave->Vstore_status = LOCALSAVE_RESUME;
        				p_localsave->Vlive_status = LOCALSAVE_RESUME;
        				if(g_cfg.IsResouceMode){
        					p_localsave->VGA_status = LOCALSAVE_RESUME;
        					p_localsave->V1_status = LOCALSAVE_RESUME;
        					p_localsave->V2_status = LOCALSAVE_RESUME;
        					p_localsave->V3_status = LOCALSAVE_RESUME;
        					p_localsave->V4_status = LOCALSAVE_RESUME;
        					p_localsave->V5_status = LOCALSAVE_RESUME;
        					p_localsave->V6_status = LOCALSAVE_RESUME;
        				}
            		}
            	}
            }

            datafifo_consumer_remove_head(p_localsave->Ctrl_consumer_fifo);
		}

		//handle the state change
		//start request handle
		time_t cur_time;
		struct tm *p_tm;
		if(p_localsave->Vstore_status == LOCALSAVE_START){
			//generate base filename
			time(&cur_time);
			p_tm = gmtime(&cur_time);
			sprintf(filename, "%04d%02d%02d%02d%02d%02d", p_tm->tm_year+1900, p_tm->tm_mon+1, p_tm->tm_mday, p_tm->tm_hour+8, p_tm->tm_min, p_tm->tm_sec);

			//generate video file name and put it to tmpfilename[]
			strcpy(tmpfilename, MUX_dir);
			strcat(tmpfilename, filename);
			strcat(tmpfilename, ".264");
			vfd = fopen(tmpfilename, "w");
			//vfd = fopen("out.264", "w");
			if(!vfd){
				debug_printf("could not open the output file for video\n");
			}

			//generate audio file name and put it to tmpfilename[]
			strcpy(tmpfilename, MUX_dir);
			strcat(tmpfilename, filename);
			strcat(tmpfilename, ".aac");
			afd = fopen(tmpfilename, "w");
			//afd = fopen("out.aac", "w");
			if(!afd){
				debug_printf("could not open the output file for audio\n");
			}

			//generate property file name and open it if necessary
			strcpy(tmpfilename, MUX_dir);
			strcat(tmpfilename, filename);
			strcat(tmpfilename, ".pro");
			pfd = fopen(tmpfilename, "w");
			if(!pfd){
				debug_printf("could not open the output file for property\n");
			}

			frame_count = aframe_count = 0;
			v1_frames = v2_frames = v3_frames = v4_frames = v5_frames = 0;
			//change state to resume
			p_localsave->Vstore_status = LOCALSAVE_RESUME;

		}

		if(p_localsave->Vlive_status == LOCALSAVE_START){
			//change state to resume
			p_localsave->Vlive_status = LOCALSAVE_RESUME;

		}

		if(p_localsave->VGA_status == LOCALSAVE_START){
			//open file here
			strcpy(tmpfilename, VGA_dir);
			strcat(tmpfilename, filename);
			strcat(tmpfilename, ".264");
			VGA_fd = fopen(tmpfilename, "w");
			if(!VGA_fd){
				debug_printf("could not open the output file for VGA channel\n");
			}
			//change state to resume
			p_localsave->VGA_status = LOCALSAVE_RESUME;

		}

		if(p_localsave->V1_status == LOCALSAVE_START){
			//open file here
			strcpy(tmpfilename, V1_dir);
			strcat(tmpfilename, filename);
			strcat(tmpfilename, ".264");
			V1_fd = fopen(tmpfilename, "w");
			if(!V1_fd){
				debug_printf("could not open the output file for V1 channel\n");
			}
			//change state to resume
			p_localsave->V1_status = LOCALSAVE_RESUME;

		}

		if(p_localsave->V2_status == LOCALSAVE_START){
			//open file here
			strcpy(tmpfilename, V2_dir);
			strcat(tmpfilename, filename);
			strcat(tmpfilename, ".264");
			V2_fd = fopen(tmpfilename, "w");
			if(!V2_fd){
				debug_printf("could not open the output file for V2 channel\n");
			}
			//change state to resume
			p_localsave->V2_status = LOCALSAVE_RESUME;

		}

		if(p_localsave->V3_status == LOCALSAVE_START){
			//open file here
			strcpy(tmpfilename, V3_dir);
			strcat(tmpfilename, filename);
			strcat(tmpfilename, ".264");
			V3_fd = fopen(tmpfilename, "w");
			if(!V3_fd){
				debug_printf("could not open the output file for V3 channel\n");
			}
			//change state to resume
			p_localsave->V3_status = LOCALSAVE_RESUME;

		}

		if(p_localsave->V4_status == LOCALSAVE_START){
			//open file here
			strcpy(tmpfilename, V4_dir);
			strcat(tmpfilename, filename);
			strcat(tmpfilename, ".264");
			V4_fd = fopen(tmpfilename, "w");
			if(!V4_fd){
				debug_printf("could not open the output file for V4 channel\n");
			}
			//change state to resume
			p_localsave->V4_status = LOCALSAVE_RESUME;
		}

		if(p_localsave->V5_status == LOCALSAVE_START){
			//open file here
			strcpy(tmpfilename, V5_dir);
			strcat(tmpfilename, filename);
			strcat(tmpfilename, ".264");
			V5_fd = fopen(tmpfilename, "w");
			if(!V5_fd){
				debug_printf("could not open the output file for V5 channel\n");
			}
			//change state to resume
			p_localsave->V5_status = LOCALSAVE_RESUME;
		}

		if(p_localsave->V6_status == LOCALSAVE_START){
			//open file here
			strcpy(tmpfilename, V6_dir);
			strcat(tmpfilename, filename);
			strcat(tmpfilename, ".264");
			V6_fd = fopen(tmpfilename, "w");
			if(!V6_fd){
				debug_printf("could not open the output file for V6 channel\n");
			}
			//change state to resume
			p_localsave->V6_status = LOCALSAVE_RESUME;
		}

		//stop request handle
		if(p_localsave->Vstore_status == LOCALSAVE_STOP){
			//close file here
			fclose(afd);
			afd = NULL;
			fclose(vfd);
			vfd = NULL;
			fprintf(pfd, "STOP: %d\n", frame_count);
			fclose(pfd);
			pfd =NULL;

			//convert to mp4
			t_mp4format *p_fmt;
			p_fmt = malloc(sizeof(t_mp4format));//released in the thread created
			if(p_fmt){
				strcpy(p_fmt->filename, MUX_dir);
				strcat(p_fmt->filename, filename);
				sprintf(p_fmt->fps, "%.3f", g_cfg.frame_rate);
				p_fmt->has_audio = 1;

				pthread_t pt_mp4format_id;
				pthread_create(&pt_mp4format_id, NULL, thread_mp4format, (void *)p_fmt);
			}

			//change state to idle
			p_localsave->Vstore_status = LOCALSAVE_IDLE;
		}

		if(p_localsave->Vlive_status == LOCALSAVE_STOP){
			//change state to idle
			p_localsave->Vlive_status = LOCALSAVE_IDLE;

		}

		if(p_localsave->VGA_status == LOCALSAVE_STOP){
			//close file here
			fclose(VGA_fd);
			VGA_fd = NULL;

			//convert to mp4
			t_mp4format *p_fmt;
			p_fmt = malloc(sizeof(t_mp4format));//released in the thread created
			if(p_fmt){
				strcpy(p_fmt->filename, VGA_dir);
				strcat(p_fmt->filename, filename);
				sprintf(p_fmt->fps, "%.3f", g_cfg.frame_rate);
				p_fmt->has_audio = 0;

				pthread_t pt_mp4format_id;
				pthread_create(&pt_mp4format_id, NULL, thread_mp4format, (void *)p_fmt);
			}

			//change state to idle
			p_localsave->VGA_status = LOCALSAVE_IDLE;

		}

		if(p_localsave->V1_status == LOCALSAVE_STOP){
			//close file here
			fclose(V1_fd);
			V1_fd = NULL;

			//convert to mp4
			t_mp4format *p_fmt;
			p_fmt = malloc(sizeof(t_mp4format));//released in the thread created
			if(p_fmt){
				strcpy(p_fmt->filename, V1_dir);
				strcat(p_fmt->filename, filename);
				sprintf(p_fmt->fps, "%.3f", g_cfg.frame_rate);
				p_fmt->has_audio = 0;

				pthread_t pt_mp4format_id;
				pthread_create(&pt_mp4format_id, NULL, thread_mp4format, (void *)p_fmt);
			}

			//change state to idle
			p_localsave->V1_status = LOCALSAVE_IDLE;

		}

		if(p_localsave->V2_status == LOCALSAVE_STOP){
			//close file here
			fclose(V2_fd);
			V2_fd = NULL;

			//convert to mp4
			t_mp4format *p_fmt;
			p_fmt = malloc(sizeof(t_mp4format));//released in the thread created
			if(p_fmt){
				strcpy(p_fmt->filename, V2_dir);
				strcat(p_fmt->filename, filename);
				sprintf(p_fmt->fps, "%.3f", g_cfg.frame_rate);
				p_fmt->has_audio = 0;

				pthread_t pt_mp4format_id;
				pthread_create(&pt_mp4format_id, NULL, thread_mp4format, (void *)p_fmt);
			}

			//change state to idle
			p_localsave->V2_status = LOCALSAVE_IDLE;

		}

		if(p_localsave->V3_status == LOCALSAVE_STOP){
			//close file here
			fclose(V3_fd);
			V3_fd = NULL;

			//convert to mp4
			t_mp4format *p_fmt;
			p_fmt = malloc(sizeof(t_mp4format));//released in the thread created
			if(p_fmt){
				strcpy(p_fmt->filename, V3_dir);
				strcat(p_fmt->filename, filename);
				sprintf(p_fmt->fps, "%.3f", g_cfg.frame_rate);
				p_fmt->has_audio = 0;

				pthread_t pt_mp4format_id;
				pthread_create(&pt_mp4format_id, NULL, thread_mp4format, (void *)p_fmt);
			}

			//change state to idle
			p_localsave->V3_status = LOCALSAVE_IDLE;

		}

		if(p_localsave->V4_status == LOCALSAVE_STOP){
			//close file here
			fclose(V4_fd);
			V4_fd = NULL;

			//convert to mp4
			t_mp4format *p_fmt;
			p_fmt = malloc(sizeof(t_mp4format));//released in the thread created
			if(p_fmt){
				strcpy(p_fmt->filename, V4_dir);
				strcat(p_fmt->filename, filename);
				sprintf(p_fmt->fps, "%.3f", g_cfg.frame_rate);
				p_fmt->has_audio = 0;

				pthread_t pt_mp4format_id;
				pthread_create(&pt_mp4format_id, NULL, thread_mp4format, (void *)p_fmt);
			}

			//change state to idle
			p_localsave->V4_status = LOCALSAVE_IDLE;
		}

		if(p_localsave->V5_status == LOCALSAVE_STOP){
			//close file here
			fclose(V5_fd);
			V5_fd = NULL;

			//convert to mp4
			t_mp4format *p_fmt;
			p_fmt = malloc(sizeof(t_mp4format));//released in the thread created
			if(p_fmt){
				strcpy(p_fmt->filename, V5_dir);
				strcat(p_fmt->filename, filename);
				sprintf(p_fmt->fps, "%.3f", g_cfg.frame_rate);
				p_fmt->has_audio = 0;

				pthread_t pt_mp4format_id;
				pthread_create(&pt_mp4format_id, NULL, thread_mp4format, (void *)p_fmt);
			}

			//change state to idle
			p_localsave->V5_status = LOCALSAVE_IDLE;
		}

		if(p_localsave->V6_status == LOCALSAVE_STOP){
			//close file here
			fclose(V6_fd);
			V6_fd = NULL;

			//convert to mp4
			t_mp4format *p_fmt;
			p_fmt = malloc(sizeof(t_mp4format));//released in the thread created
			if(p_fmt){
				strcpy(p_fmt->filename, V6_dir);
				strcat(p_fmt->filename, filename);
				sprintf(p_fmt->fps, "%.3f", g_cfg.frame_rate);
				p_fmt->has_audio = 0;

				pthread_t pt_mp4format_id;
				pthread_create(&pt_mp4format_id, NULL, thread_mp4format, (void *)p_fmt);
			}

			//change state to idle
			p_localsave->V6_status = LOCALSAVE_IDLE;
		}

		if(datafifo_head(p_localsave->av_consumer_fifo)){

			if(datafifo_head(p_localsave->av_consumer_fifo)->ref->datafifo == p_localsave->Vmux_store){
				if(p_localsave->Vstore_status == LOCALSAVE_RESUME){
					if((datafifo_head(p_localsave->av_consumer_fifo)->arg.flags & FRAME_FLAG_FIRSTFRAME)
							||(datafifo_head(p_localsave->av_consumer_fifo)->arg.flags & FRAME_FLAG_KEYFRAME)){
						p_localsave->Vstore_status = LOCALSAVE_WORKING;
						fprintf(pfd, "RESUME: %d\n", frame_count);
					}
				}

				if(p_localsave->Vstore_status == LOCALSAVE_WORKING){
					fwrite(datafifo_head(p_localsave->av_consumer_fifo)->arg.data, 1, datafifo_head(p_localsave->av_consumer_fifo)->arg.len, vfd);
				}
				frame_count++;
			}

			if(datafifo_head(p_localsave->av_consumer_fifo)->ref->datafifo == p_localsave->Vmux_live){
				if(p_localsave->Vlive_status == LOCALSAVE_RESUME){
					if((datafifo_head(p_localsave->av_consumer_fifo)->arg.flags & FRAME_FLAG_FIRSTFRAME)
							||(datafifo_head(p_localsave->av_consumer_fifo)->arg.flags & FRAME_FLAG_KEYFRAME)){
						p_localsave->Vlive_status = LOCALSAVE_WORKING;
					}
				}

				if(p_localsave->Vlive_status == LOCALSAVE_WORKING){
					if(p_localsave->sock != -1){
						char pkt_buf[STREAMING_PACKET_HEADER_LENGTH];
						t_streaming_packet_header *ptr = (t_streaming_packet_header *)pkt_buf;

						ptr->flag = 0;
						ptr->size = datafifo_head(p_localsave->av_consumer_fifo)->arg.len;
						ptr->type = PACKET_TYPE_VIDEO;
						//ptr->timestamp = datafifo_head(p_localsave->av_consumer_fifo)->arg.timestamp;
						ptr->timestamp = live_vframes*1000/g_cfg.frame_rate;
#if 0
						if(datafifo_head(p_localsave->av_consumer_fifo)->arg.flags & FRAME_FLAG_KEYFRAME){
							ptr->flag = FRAME_FLAG_KEYFRAME;
						}
						else{
							ptr->flag = 0;
						}
#endif
						int ret = send(p_localsave->sock, pkt_buf, STREAMING_PACKET_HEADER_LENGTH, 0);
						if(ret<STREAMING_PACKET_HEADER_LENGTH){
							close(p_localsave->sock);
							p_localsave->sock = -1;
						}
						if(p_localsave->sock != -1){
							ret = send(p_localsave->sock, datafifo_head(p_localsave->av_consumer_fifo)->arg.data, datafifo_head(p_localsave->av_consumer_fifo)->arg.len, 0);
							if(ret<datafifo_head(p_localsave->av_consumer_fifo)->arg.len){
								close(p_localsave->sock);
								p_localsave->sock = -1;
							}
							live_vframes++;
						}
					}
				}
			}

			if(datafifo_head(p_localsave->av_consumer_fifo)->ref->datafifo == p_localsave->VGA){
				if(p_localsave->VGA_status == LOCALSAVE_RESUME){
					if((datafifo_head(p_localsave->av_consumer_fifo)->arg.flags & FRAME_FLAG_FIRSTFRAME)
							||(datafifo_head(p_localsave->av_consumer_fifo)->arg.flags & FRAME_FLAG_KEYFRAME)){
						p_localsave->VGA_status = LOCALSAVE_WORKING;
					}
				}

				if(p_localsave->VGA_status == LOCALSAVE_WORKING){
					fwrite(datafifo_head(p_localsave->av_consumer_fifo)->arg.data, 1, datafifo_head(p_localsave->av_consumer_fifo)->arg.len, VGA_fd);
				}
			}

			if(datafifo_head(p_localsave->av_consumer_fifo)->ref->datafifo == p_localsave->V1){
				if(p_localsave->V1_status == LOCALSAVE_RESUME){
					if((datafifo_head(p_localsave->av_consumer_fifo)->arg.flags & FRAME_FLAG_FIRSTFRAME)
							||(datafifo_head(p_localsave->av_consumer_fifo)->arg.flags & FRAME_FLAG_KEYFRAME)){
						p_localsave->V1_status = LOCALSAVE_WORKING;
					}
				}

				if(p_localsave->V1_status == LOCALSAVE_WORKING){
					fwrite(datafifo_head(p_localsave->av_consumer_fifo)->arg.data, 1, datafifo_head(p_localsave->av_consumer_fifo)->arg.len, V1_fd);
#if 1
					switch(g_vmap_table[0].function){
					case VIDEO_FUNCTION_TEACHER_CLOSEUP:
						if(g_forward_table.teacherspecial){
							streamframe(p_localsave, VIDEO_FUNCTION_TEACHER_CLOSEUP, &v1_frames);

						}
						break;
					case VIDEO_FUNCTION_STUDENT_CLOSEUP:
						if(g_forward_table.studentspecial){
							streamframe(p_localsave, VIDEO_FUNCTION_STUDENT_CLOSEUP, &v1_frames);
						}
						break;
					case VIDEO_FUNCTION_TEACHER_PANORAMA:
						if(g_forward_table.teacherfull){
							streamframe(p_localsave, VIDEO_FUNCTION_TEACHER_PANORAMA, &v1_frames);
						}
						break;
					case VIDEO_FUNCTION_STUDENT_PANORAMA:
						if(g_forward_table.studentfull){
							streamframe(p_localsave, VIDEO_FUNCTION_STUDENT_PANORAMA, &v1_frames);
						}
						break;
					case VIDEO_FUNCTION_BOARD_CLOSEUP:
						if(g_forward_table.blackboard){
							streamframe(p_localsave, VIDEO_FUNCTION_BOARD_CLOSEUP, &v1_frames);
						}
						break;
					default:
						break;
					}
#endif
				}
			}
			if(datafifo_head(p_localsave->av_consumer_fifo)->ref->datafifo == p_localsave->V2){
				if(p_localsave->V2_status == LOCALSAVE_RESUME){
					if((datafifo_head(p_localsave->av_consumer_fifo)->arg.flags & FRAME_FLAG_FIRSTFRAME)
							||(datafifo_head(p_localsave->av_consumer_fifo)->arg.flags & FRAME_FLAG_KEYFRAME)){
						p_localsave->V2_status = LOCALSAVE_WORKING;
					}
				}

				if(p_localsave->V2_status == LOCALSAVE_WORKING){
					fwrite(datafifo_head(p_localsave->av_consumer_fifo)->arg.data, 1, datafifo_head(p_localsave->av_consumer_fifo)->arg.len, V2_fd);
#if 1
					switch(g_vmap_table[1].function){
					case VIDEO_FUNCTION_TEACHER_CLOSEUP:
						if(g_forward_table.teacherspecial){
							streamframe(p_localsave, VIDEO_FUNCTION_TEACHER_CLOSEUP, &v2_frames);

						}
						break;
					case VIDEO_FUNCTION_STUDENT_CLOSEUP:
						if(g_forward_table.studentspecial){
							streamframe(p_localsave, VIDEO_FUNCTION_STUDENT_CLOSEUP, &v2_frames);
						}
						break;
					case VIDEO_FUNCTION_TEACHER_PANORAMA:
						if(g_forward_table.teacherfull){
							streamframe(p_localsave, VIDEO_FUNCTION_TEACHER_PANORAMA, &v2_frames);
						}
						break;
					case VIDEO_FUNCTION_STUDENT_PANORAMA:
						if(g_forward_table.studentfull){
							streamframe(p_localsave, VIDEO_FUNCTION_STUDENT_PANORAMA, &v2_frames);
						}
						break;
					case VIDEO_FUNCTION_BOARD_CLOSEUP:
						if(g_forward_table.blackboard){
							streamframe(p_localsave, VIDEO_FUNCTION_BOARD_CLOSEUP, &v2_frames);
						}
						break;
					default:
						break;
					}
#endif
				}
			}

			if(datafifo_head(p_localsave->av_consumer_fifo)->ref->datafifo == p_localsave->V3){
				if(p_localsave->V3_status == LOCALSAVE_RESUME){
					if((datafifo_head(p_localsave->av_consumer_fifo)->arg.flags & FRAME_FLAG_FIRSTFRAME)
							||(datafifo_head(p_localsave->av_consumer_fifo)->arg.flags & FRAME_FLAG_KEYFRAME)){
						p_localsave->V3_status = LOCALSAVE_WORKING;
					}
				}

				if(p_localsave->V3_status == LOCALSAVE_WORKING){
					fwrite(datafifo_head(p_localsave->av_consumer_fifo)->arg.data, 1, datafifo_head(p_localsave->av_consumer_fifo)->arg.len, V3_fd);
#if 1
					switch(g_vmap_table[2].function){
					case VIDEO_FUNCTION_TEACHER_CLOSEUP:
						if(g_forward_table.teacherspecial){
							streamframe(p_localsave, VIDEO_FUNCTION_TEACHER_CLOSEUP, &v3_frames);

						}
						break;
					case VIDEO_FUNCTION_STUDENT_CLOSEUP:
						if(g_forward_table.studentspecial){
							streamframe(p_localsave, VIDEO_FUNCTION_STUDENT_CLOSEUP, &v3_frames);
						}
						break;
					case VIDEO_FUNCTION_TEACHER_PANORAMA:
						if(g_forward_table.teacherfull){
							streamframe(p_localsave, VIDEO_FUNCTION_TEACHER_PANORAMA, &v3_frames);
						}
						break;
					case VIDEO_FUNCTION_STUDENT_PANORAMA:
						if(g_forward_table.studentfull){
							streamframe(p_localsave, VIDEO_FUNCTION_STUDENT_PANORAMA, &v3_frames);
						}
						break;
					case VIDEO_FUNCTION_BOARD_CLOSEUP:
						if(g_forward_table.blackboard){
							streamframe(p_localsave, VIDEO_FUNCTION_BOARD_CLOSEUP, &v3_frames);
						}
						break;
					default:
						break;
					}
#endif
				}
			}

			if(datafifo_head(p_localsave->av_consumer_fifo)->ref->datafifo == p_localsave->V4){
				if(p_localsave->V4_status == LOCALSAVE_RESUME){
					if((datafifo_head(p_localsave->av_consumer_fifo)->arg.flags & FRAME_FLAG_FIRSTFRAME)
							||(datafifo_head(p_localsave->av_consumer_fifo)->arg.flags & FRAME_FLAG_KEYFRAME)){
						p_localsave->V4_status = LOCALSAVE_WORKING;
					}
				}

				if(p_localsave->V4_status == LOCALSAVE_WORKING){
					fwrite(datafifo_head(p_localsave->av_consumer_fifo)->arg.data, 1, datafifo_head(p_localsave->av_consumer_fifo)->arg.len, V4_fd);
#if 1
					switch(g_vmap_table[3].function){
					case VIDEO_FUNCTION_TEACHER_CLOSEUP:
						if(g_forward_table.teacherspecial){
							streamframe(p_localsave, VIDEO_FUNCTION_TEACHER_CLOSEUP, &v4_frames);

						}
						break;
					case VIDEO_FUNCTION_STUDENT_CLOSEUP:
						if(g_forward_table.studentspecial){
							streamframe(p_localsave, VIDEO_FUNCTION_STUDENT_CLOSEUP, &v4_frames);
						}
						break;
					case VIDEO_FUNCTION_TEACHER_PANORAMA:
						if(g_forward_table.teacherfull){
							streamframe(p_localsave, VIDEO_FUNCTION_TEACHER_PANORAMA, &v4_frames);
						}
						break;
					case VIDEO_FUNCTION_STUDENT_PANORAMA:
						if(g_forward_table.studentfull){
							streamframe(p_localsave, VIDEO_FUNCTION_STUDENT_PANORAMA, &v4_frames);
						}
						break;
					case VIDEO_FUNCTION_BOARD_CLOSEUP:
						if(g_forward_table.blackboard){
							streamframe(p_localsave, VIDEO_FUNCTION_BOARD_CLOSEUP, &v4_frames);
						}
						break;
					default:
						break;
					}
#endif
				}
			}

			if(datafifo_head(p_localsave->av_consumer_fifo)->ref->datafifo == p_localsave->V5){
				if(p_localsave->V5_status == LOCALSAVE_RESUME){
					if((datafifo_head(p_localsave->av_consumer_fifo)->arg.flags & FRAME_FLAG_FIRSTFRAME)
							||(datafifo_head(p_localsave->av_consumer_fifo)->arg.flags & FRAME_FLAG_KEYFRAME)){
						p_localsave->V5_status = LOCALSAVE_WORKING;
					}
				}

				if(p_localsave->V5_status == LOCALSAVE_WORKING){
					fwrite(datafifo_head(p_localsave->av_consumer_fifo)->arg.data, 1, datafifo_head(p_localsave->av_consumer_fifo)->arg.len, V5_fd);
#if 1
					switch(g_vmap_table[4].function){
					case VIDEO_FUNCTION_TEACHER_CLOSEUP:
						if(g_forward_table.teacherspecial){
							streamframe(p_localsave, VIDEO_FUNCTION_TEACHER_CLOSEUP, &v5_frames);

						}
						break;
					case VIDEO_FUNCTION_STUDENT_CLOSEUP:
						if(g_forward_table.studentspecial){
							streamframe(p_localsave, VIDEO_FUNCTION_STUDENT_CLOSEUP, &v5_frames);
						}
						break;
					case VIDEO_FUNCTION_TEACHER_PANORAMA:
						if(g_forward_table.teacherfull){
							streamframe(p_localsave, VIDEO_FUNCTION_TEACHER_PANORAMA, &v5_frames);
						}
						break;
					case VIDEO_FUNCTION_STUDENT_PANORAMA:
						if(g_forward_table.studentfull){
							streamframe(p_localsave, VIDEO_FUNCTION_STUDENT_PANORAMA, &v5_frames);
						}
						break;
					case VIDEO_FUNCTION_BOARD_CLOSEUP:
						if(g_forward_table.blackboard){
							streamframe(p_localsave, VIDEO_FUNCTION_BOARD_CLOSEUP, &v5_frames);
						}
						break;
					default:
						break;
					}
#endif
				}
			}

			if(datafifo_head(p_localsave->av_consumer_fifo)->ref->datafifo == p_localsave->V6){
				if(p_localsave->V6_status == LOCALSAVE_RESUME){
					if((datafifo_head(p_localsave->av_consumer_fifo)->arg.flags & FRAME_FLAG_FIRSTFRAME)
							||(datafifo_head(p_localsave->av_consumer_fifo)->arg.flags & FRAME_FLAG_KEYFRAME)){
						p_localsave->V6_status = LOCALSAVE_WORKING;
					}
				}

				if(p_localsave->V6_status == LOCALSAVE_WORKING){
					fwrite(datafifo_head(p_localsave->av_consumer_fifo)->arg.data, 1, datafifo_head(p_localsave->av_consumer_fifo)->arg.len, V6_fd);
				}
			}

			if(datafifo_head(p_localsave->av_consumer_fifo)->ref->datafifo == p_localsave->audio){
				if(p_localsave->Vstore_status == LOCALSAVE_WORKING){
					fwrite(datafifo_head(p_localsave->av_consumer_fifo)->arg.data, 1, datafifo_head(p_localsave->av_consumer_fifo)->arg.len, afd);
				}

				if(p_localsave->Vlive_status == LOCALSAVE_WORKING){
					if(p_localsave->sock != -1){
						char pkt_buf[STREAMING_PACKET_HEADER_LENGTH];
						t_streaming_packet_header *ptr = (t_streaming_packet_header *)pkt_buf;

						ptr->flag = 0;
						ptr->size = datafifo_head(p_localsave->av_consumer_fifo)->arg.len;
						ptr->type = PACKET_TYPE_AUDIO;
						//ptr->timestamp = datafifo_head(p_localsave->av_consumer_fifo)->arg.timestamp;
						ptr->timestamp = live_aframes*1024/44.100;
#if 0
						if(datafifo_head(p_localsave->av_consumer_fifo)->arg.flags & FRAME_FLAG_KEYFRAME){
							ptr->flag = FRAME_FLAG_KEYFRAME;
						}
						else{
							ptr->flag = 0;
						}
#endif
						int ret = send(p_localsave->sock, pkt_buf, STREAMING_PACKET_HEADER_LENGTH, 0);
						if(ret<STREAMING_PACKET_HEADER_LENGTH){
							close(p_localsave->sock);
							p_localsave->sock = -1;
						}
						if(p_localsave->sock != -1){
							ret = send(p_localsave->sock, datafifo_head(p_localsave->av_consumer_fifo)->arg.data, datafifo_head(p_localsave->av_consumer_fifo)->arg.len, 0);
							if(ret<datafifo_head(p_localsave->av_consumer_fifo)->arg.len){
								close(p_localsave->sock);
								p_localsave->sock = -1;
							}
							live_aframes++;
						}

					}
				}
			}

			datafifo_consumer_remove_head(p_localsave->av_consumer_fifo);
		}
		usleep(1000);
		//debug_printf("This is from thread localsave\n");
	}

	if(p_localsave->sock>0){
		close(p_localsave->sock);
		p_localsave->sock = -1;
		usleep(10000);
	}
	debug_printf("thread localsave exit\n");
	p_localsave->thread_canceled = 2;

	pthread_exit(0);
}

void thread_img_askexit(t_img_thread_param *ptr)
{
	if(ptr->thread_canceled == 0){
		ptr->thread_canceled = 1;
	}
}

int thread_img_exited(t_img_thread_param *ptr)
{
	return ptr->thread_canceled==2?1:0;
}

//typedef unsigned char uint8_t;

int yuv422_to_jpeg(unsigned char *src, unsigned char* dst, int image_width, int image_height, FILE *tmpfile, int quality)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    //JSAMPROW row_pointer[1];  /* pointer to JSAMPLE row[s] */
    //int row_stride;    /* physical row width in image buffer */
    JSAMPIMAGE  buffer;
    int band,i,buf_width[3],buf_height[3], mem_size, max_line, counter;
    unsigned char *yuv[3];
    uint8_t *pSrc, *pDst;

    yuv[0] = src;
    yuv[1] = yuv[0] + (image_width * image_height);
    yuv[2] = yuv[1] + (image_width * image_height) /2;

    fseek(tmpfile, 0, 0);

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, tmpfile);

    cinfo.image_width = image_width;  /* image width and height, in pixels */
    cinfo.image_height = image_height;
    cinfo.input_components = 3;    /* # of color components per pixel */
    cinfo.in_color_space = JCS_RGB;  /* colorspace of input image */

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE );

    cinfo.raw_data_in = TRUE;
    cinfo.jpeg_color_space = JCS_YCbCr;
    cinfo.do_fancy_downsampling = FALSE;
    cinfo.comp_info[0].h_samp_factor = 2;
    cinfo.comp_info[0].v_samp_factor = 1;

    jpeg_start_compress(&cinfo, TRUE);

    buffer = (JSAMPIMAGE) (*cinfo.mem->alloc_small) ((j_common_ptr) &cinfo, JPOOL_IMAGE, 3 * sizeof(JSAMPARRAY));
    for(band=0; band <3; band++)
    {
        buf_width[band] = cinfo.comp_info[band].width_in_blocks * DCTSIZE;
        buf_height[band] = cinfo.comp_info[band].v_samp_factor * DCTSIZE;
        buffer[band] = (*cinfo.mem->alloc_sarray) ((j_common_ptr) &cinfo, JPOOL_IMAGE, buf_width[band], buf_height[band]);
    }
    max_line = cinfo.max_v_samp_factor*DCTSIZE;
    for(counter=0; cinfo.next_scanline < cinfo.image_height; counter++)
    {
        //buffer image copy.
        for(band=0; band <3; band++)
        {
            mem_size = buf_width[band];
            pDst = (uint8_t *) buffer[band][0];
            pSrc = (uint8_t *) yuv[band] + counter*buf_height[band] * buf_width[band];

            for(i=0; i <buf_height[band]; i++)
            {
                memcpy(pDst, pSrc, mem_size);
                pSrc += buf_width[band];
                pDst += buf_width[band];
            }
        }
        jpeg_write_raw_data(&cinfo, buffer, max_line);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    int len = ftell(tmpfile);
    fseek(tmpfile, 0, 0);
    fread(dst, len, 1,tmpfile);

    return len;
}

int yuv422_to_planner(unsigned char *src, unsigned char *dest, int image_width, int image_height)
{
	unsigned char *yuv[3];
    yuv[0] = dest;
    yuv[1] = yuv[0] + (image_width * image_height);
    yuv[2] = yuv[1] + (image_width * image_height)/2;
    int i;

    for(i = 0; i < image_height*image_width; i++){
    	yuv[0][i] = src[2*i];
    }
    for(i = 0; i < image_height*image_width/2; i++){
    	yuv[1][i] = src[4*i+1];
    }
    for(i = 0; i < image_height*image_width/2; i++){
    	yuv[2][i] = src[4*i+3];
    }
    return image_height*image_width*2;
}

int yuv422_to_yuv420(unsigned char *src, unsigned char *dest, int image_width, int image_height)
{
	unsigned char *ptr_s, *ptr_d;
	int i,j;

	ptr_s = src;
	ptr_d = dest;
	for(i = 0; i<image_height*image_width; i++){
		ptr_d[i] = ptr_s[i*2];
	}

	ptr_d = dest + image_height*image_width;
	for(j = 0; j<image_height/2; j++){
		for(i = 0; i<image_width; i++){
			//ptr_d[i] = ((unsigned int)ptr_s[i*2+1]+(unsigned int)ptr_s[i*2+1+image_width*2])>>1;
			ptr_d[i] = ptr_s[i*2+1];
		}
		ptr_s += image_width*4;
		ptr_d += image_width;
	}
	return image_height*image_width*3/2;
}

int yuv422_to_y(unsigned char *src, unsigned char *dest, int image_width, int image_height)
{
	int i;
	for(i=0; i<image_height*image_width; i++){
		dest[i] = src[i*2];
	}
	return image_height*image_width;
}

void JPEGPacknSend2df(unsigned char *buf, int len, int frame_num, int timestamp, int channel, t_datafifo *df)
{
	t_frame_info frm_info;
	int i;
	unsigned char *ptr;
	unsigned int residual;
	unsigned int pkt_len;
	int flags;

	frm_info.frame_length = len;
	frm_info.frame_num = frame_num;
	frm_info.frame_type = 0;
	frm_info.time_stamp = timestamp;
	frm_info.pkts_in_frame = len/MAX_AVPKT_SIZE;
	residual = len - MAX_AVPKT_SIZE * frm_info.pkts_in_frame;
	if(residual){
		frm_info.pkts_in_frame += 1;
	}

	residual = len;
	ptr = buf;
	for(i=0; i<frm_info.pkts_in_frame; i++){
		frm_info.packet_num = i;
		pkt_len = (MAX_AVPKT_SIZE<residual)?MAX_AVPKT_SIZE:residual;

		//build message
		t_cmd Msg;
		fill_cmd_header(&Msg, COMMAND_EVENT_JPEG);
		add_attr_int(&Msg, "StreamId", channel);
		add_attr_bin(&Msg, "Info", (char *)&frm_info, sizeof(frm_info));
		add_attr_bin(&Msg, "Data", (char *)ptr, pkt_len);

		//send the message to data fifo
		size_t size;
		char2hex(Msg.data_len, &size, DATA_LEN_SIZE);
		size += PACKET_HEADER_SIZE;
		//send(sock, Msg.fullbuf, size, 0);
		flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
		datafifo_producer_data_add(df, (unsigned char *)Msg.fullbuf, size, flags, 0);
		//debug_printf("send %d bytes\n", ret);
		//calculate new data start point and length left
		ptr += pkt_len;
		residual -= pkt_len;
		//sleep(1);
	}
}

void PIPPacknSend2df(unsigned char *buf, int len, int frame_num, int timestamp, int channel, t_datafifo *df)
{
	t_frame_info frm_info;
	int i;
	unsigned char *ptr;
	unsigned int residual;
	unsigned int pkt_len;
	int flags;

	frm_info.frame_length = len;
	frm_info.frame_num = frame_num;
	frm_info.frame_type = 0;
	frm_info.time_stamp = timestamp;
	frm_info.pkts_in_frame = len/MAX_AVPKT_SIZE;
	residual = len - MAX_AVPKT_SIZE * frm_info.pkts_in_frame;
	if(residual){
		frm_info.pkts_in_frame += 1;
	}

	residual = len;
	ptr = buf;
	for(i=0; i<frm_info.pkts_in_frame; i++){
		frm_info.packet_num = i;
		pkt_len = (MAX_AVPKT_SIZE<residual)?MAX_AVPKT_SIZE:residual;

		//build message
		t_cmd Msg;
		fill_cmd_header(&Msg, COMMAND_EVENT_PIP);
		add_attr_int(&Msg, "StreamId", channel);
		add_attr_bin(&Msg, "Info", (char *)&frm_info, sizeof(frm_info));
		add_attr_bin(&Msg, "Data", (char *)ptr, pkt_len);

		//send the message to data fifo
		size_t size;
		char2hex(Msg.data_len, &size, DATA_LEN_SIZE);
		size += PACKET_HEADER_SIZE;
		//send(sock, Msg.fullbuf, size, 0);
		flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
		datafifo_producer_data_add(df, (unsigned char *)Msg.fullbuf, size, flags, 0);
		//debug_printf("send %d bytes\n", ret);
		//calculate new data start point and length left
		ptr += pkt_len;
		residual -= pkt_len;
		//sleep(1);
	}
}

#define SKIP_STEP 5
void * thread_img(void * param)
{
	pthread_detach(pthread_self());
	t_img_thread_param *thread_param = (t_img_thread_param *)param;
	unsigned char *buffer1, *buffer2;
	FILE *fd;
	//int analyze_frame_num = 0;

	buffer1 = malloc(256*144*2);
	if(!buffer1){
		debug_printf("allocate buffer fail\n");
		exit(0);
	}

	buffer2 = malloc(256*144*2*2);
	if(!buffer2){
		debug_printf("allocate buffer fail\n");
		exit(0);
	}

	fd = fopen("./frame_result.jpg","w+");
	if(!fd){
		debug_printf("can not open target file for jpeg\n");
		exit(0);
	}

	while(1){
		if(thread_param->thread_canceled){
			break;
		}

		if(datafifo_head(thread_param->img_consumer_fifo)){

			if(datafifo_head(thread_param->img_consumer_fifo)->ref->datafifo == thread_param->imgV){
				//to JPEG
				int length;
				yuv422_to_planner(datafifo_head(thread_param->img_consumer_fifo)->arg.data, buffer1, 256, 144);
				length = yuv422_to_jpeg(buffer1, buffer2, 256, 144, fd, 100);
				//transmit to data fifo
				JPEGPacknSend2df(buffer2, length, datafifo_head(thread_param->img_consumer_fifo)->arg.timestamp,
						datafifo_head(thread_param->img_consumer_fifo)->arg.timestamp, 0, thread_param->JPEG_producer_fifo);
			}

			if(datafifo_head(thread_param->img_consumer_fifo)->ref->datafifo == thread_param->imgM){
				//do nothing
			}

			if(datafifo_head(thread_param->img_consumer_fifo)->ref->datafifo == thread_param->img1){

				//to JPEG
				int length;
				yuv422_to_planner(datafifo_head(thread_param->img_consumer_fifo)->arg.data, buffer1, 256, 144);
				length = yuv422_to_jpeg(buffer1, buffer2, 256, 144, fd, 100);
				//transmit to data fifo
				JPEGPacknSend2df(buffer2, length, datafifo_head(thread_param->img_consumer_fifo)->arg.timestamp,
						datafifo_head(thread_param->img_consumer_fifo)->arg.timestamp, g_vmap_table[0].function, thread_param->JPEG_producer_fifo);
				//to PIP if it is PIP channel

				if(g_vmap_table[0].pip){
					length = yuv422_to_yuv420(datafifo_head(thread_param->img_consumer_fifo)->arg.data, buffer1, 256, 144);
					PIPPacknSend2df(buffer1, length, datafifo_head(thread_param->img_consumer_fifo)->arg.timestamp,
							datafifo_head(thread_param->img_consumer_fifo)->arg.timestamp, 0, thread_param->PIP_producer_fifo);
					//debug_printf("pip data send\n");
				}
#if 1
				//to analyze if it need to be analyzed
				if(g_vmap_table[0].function == 1){
					int camholder_still;
					int analyze_frame_num;
					int num_in_group;

					pthread_mutex_lock(&g_capture_sync_mutex);
					camholder_still = g_camholder_still;
					analyze_frame_num = g_analyze_frame_num;
					num_in_group = g_num_in_group;

					if(g_camholder_still){
						g_analyze_frame_num++;
						if(g_analyze_frame_num%SKIP_STEP == 0){
							g_num_in_group++;
						}
					}
					pthread_mutex_unlock(&g_capture_sync_mutex);

					if(camholder_still){
						analyze_frame_num++;
						if(analyze_frame_num%SKIP_STEP == 0){
							length = yuv422_to_y(datafifo_head(thread_param->img_consumer_fifo)->arg.data, buffer1, 256, 144);
							int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
							datafifo_producer_data_add(thread_param->filtered_img_producer_fifo, buffer1, length, flags, num_in_group);
							//num_in_group++;
						}
					}
				}
#endif
			}

			if(datafifo_head(thread_param->img_consumer_fifo)->ref->datafifo == thread_param->img2){

				//to JPEG
				int length;
				yuv422_to_planner(datafifo_head(thread_param->img_consumer_fifo)->arg.data, buffer1, 256, 144);
				length = yuv422_to_jpeg(buffer1, buffer2, 256, 144, fd, 100);
				//transmit to data fifo
				JPEGPacknSend2df(buffer2, length, datafifo_head(thread_param->img_consumer_fifo)->arg.timestamp,
						datafifo_head(thread_param->img_consumer_fifo)->arg.timestamp, g_vmap_table[1].function, thread_param->JPEG_producer_fifo);
				//to PIP if it is PIP channel

				if(g_vmap_table[1].pip){
					length = yuv422_to_yuv420(datafifo_head(thread_param->img_consumer_fifo)->arg.data, buffer1, 256, 144);
					PIPPacknSend2df(buffer1, length, datafifo_head(thread_param->img_consumer_fifo)->arg.timestamp,
							datafifo_head(thread_param->img_consumer_fifo)->arg.timestamp, 0, thread_param->PIP_producer_fifo);
				}
#if 1
				//to analyze if it need to be analyzed
				if(g_vmap_table[1].function == 1){
					int camholder_still;
					int analyze_frame_num;
					int num_in_group;

					pthread_mutex_lock(&g_capture_sync_mutex);
					camholder_still = g_camholder_still;
					analyze_frame_num = g_analyze_frame_num;
					num_in_group = g_num_in_group;

					if(g_camholder_still){
						g_analyze_frame_num++;
						if(g_analyze_frame_num%SKIP_STEP == 0){
							g_num_in_group++;
						}
					}
					pthread_mutex_unlock(&g_capture_sync_mutex);

					if(camholder_still){
						analyze_frame_num++;
						if(analyze_frame_num%SKIP_STEP == 0){
							length = yuv422_to_y(datafifo_head(thread_param->img_consumer_fifo)->arg.data, buffer1, 256, 144);
							int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
							datafifo_producer_data_add(thread_param->filtered_img_producer_fifo, buffer1, length, flags, num_in_group);
							//num_in_group++;
						}
					}
				}
#endif
			}

			if(datafifo_head(thread_param->img_consumer_fifo)->ref->datafifo == thread_param->img3){
				//to JPEG
				int length;
				yuv422_to_planner(datafifo_head(thread_param->img_consumer_fifo)->arg.data, buffer1, 256, 144);
				length = yuv422_to_jpeg(buffer1, buffer2, 256, 144, fd, 100);
				//transmit to data fifo
				JPEGPacknSend2df(buffer2, length, datafifo_head(thread_param->img_consumer_fifo)->arg.timestamp,
						datafifo_head(thread_param->img_consumer_fifo)->arg.timestamp, g_vmap_table[2].function, thread_param->JPEG_producer_fifo);
				//to PIP if it is PIP channel
				if(g_vmap_table[2].pip){
					length = yuv422_to_yuv420(datafifo_head(thread_param->img_consumer_fifo)->arg.data, buffer1, 256, 144);
					PIPPacknSend2df(buffer1, length, datafifo_head(thread_param->img_consumer_fifo)->arg.timestamp,
							datafifo_head(thread_param->img_consumer_fifo)->arg.timestamp, 0, thread_param->PIP_producer_fifo);
				}
				//to analyze if it need to be analyzed
				if(g_vmap_table[2].function == 1){
					int camholder_still;
					int analyze_frame_num;
					int num_in_group;

					pthread_mutex_lock(&g_capture_sync_mutex);
					camholder_still = g_camholder_still;
					analyze_frame_num = g_analyze_frame_num;
					num_in_group = g_num_in_group;

					if(g_camholder_still){
						g_analyze_frame_num++;
						if(g_analyze_frame_num%SKIP_STEP == 0){
							g_num_in_group++;
						}
					}
					pthread_mutex_unlock(&g_capture_sync_mutex);

					if(camholder_still){
						analyze_frame_num++;
						if(analyze_frame_num%SKIP_STEP == 0){
							length = yuv422_to_y(datafifo_head(thread_param->img_consumer_fifo)->arg.data, buffer1, 256, 144);
							int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
							datafifo_producer_data_add(thread_param->filtered_img_producer_fifo, buffer1, length, flags, num_in_group);
							//num_in_group++;
						}
					}
				}
			}

			if(datafifo_head(thread_param->img_consumer_fifo)->ref->datafifo == thread_param->img4){
				//to JPEG
				int length;
				yuv422_to_planner(datafifo_head(thread_param->img_consumer_fifo)->arg.data, buffer1, 256, 144);
				length = yuv422_to_jpeg(buffer1, buffer2, 256, 144, fd, 100);
				//transmit to data fifo
				JPEGPacknSend2df(buffer2, length, datafifo_head(thread_param->img_consumer_fifo)->arg.timestamp,
						datafifo_head(thread_param->img_consumer_fifo)->arg.timestamp, g_vmap_table[3].function, thread_param->JPEG_producer_fifo);
				//to PIP if it is PIP channel
				if(g_vmap_table[3].pip){
					length = yuv422_to_yuv420(datafifo_head(thread_param->img_consumer_fifo)->arg.data, buffer1, 256, 144);
					PIPPacknSend2df(buffer1, length, datafifo_head(thread_param->img_consumer_fifo)->arg.timestamp,
							datafifo_head(thread_param->img_consumer_fifo)->arg.timestamp, 0, thread_param->PIP_producer_fifo);
				}
				//to analyze if it need to be analyzed
				if(g_vmap_table[3].function == 1){
					int camholder_still;
					int analyze_frame_num;
					int num_in_group;

					pthread_mutex_lock(&g_capture_sync_mutex);
					camholder_still = g_camholder_still;
					analyze_frame_num = g_analyze_frame_num;
					num_in_group = g_num_in_group;

					if(g_camholder_still){
						g_analyze_frame_num++;
						if(g_analyze_frame_num%SKIP_STEP == 0){
							g_num_in_group++;
						}
					}
					pthread_mutex_unlock(&g_capture_sync_mutex);

					if(camholder_still){
						analyze_frame_num++;
						if(analyze_frame_num%SKIP_STEP == 0){
							length = yuv422_to_y(datafifo_head(thread_param->img_consumer_fifo)->arg.data, buffer1, 256, 144);
							int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
							datafifo_producer_data_add(thread_param->filtered_img_producer_fifo, buffer1, length, flags, num_in_group);
							//num_in_group++;
						}
					}
				}
			}

			if(datafifo_head(thread_param->img_consumer_fifo)->ref->datafifo == thread_param->img5){
				//to JPEG
				int length;
				yuv422_to_planner(datafifo_head(thread_param->img_consumer_fifo)->arg.data, buffer1, 256, 144);
				length = yuv422_to_jpeg(buffer1, buffer2, 256, 144, fd, 100);
				//transmit to data fifo
				JPEGPacknSend2df(buffer2, length, datafifo_head(thread_param->img_consumer_fifo)->arg.timestamp,
						datafifo_head(thread_param->img_consumer_fifo)->arg.timestamp, g_vmap_table[4].function, thread_param->JPEG_producer_fifo);
				//to PIP if it is PIP channel
				if(g_vmap_table[4].pip){
					length = yuv422_to_yuv420(datafifo_head(thread_param->img_consumer_fifo)->arg.data, buffer1, 256, 144);
					PIPPacknSend2df(buffer1, length, datafifo_head(thread_param->img_consumer_fifo)->arg.timestamp,
							datafifo_head(thread_param->img_consumer_fifo)->arg.timestamp, 0, thread_param->PIP_producer_fifo);
				}
				//to analyze if it need to be analyzed
				if(g_vmap_table[4].function == 1){
					int camholder_still;
					int analyze_frame_num;
					int num_in_group;

					pthread_mutex_lock(&g_capture_sync_mutex);
					camholder_still = g_camholder_still;
					analyze_frame_num = g_analyze_frame_num;
					num_in_group = g_num_in_group;

					if(g_camholder_still){
						g_analyze_frame_num++;
						if(g_analyze_frame_num%SKIP_STEP == 0){
							g_num_in_group++;
						}
					}
					pthread_mutex_unlock(&g_capture_sync_mutex);

					if(camholder_still){
						analyze_frame_num++;
						if(analyze_frame_num%SKIP_STEP == 0){
							length = yuv422_to_y(datafifo_head(thread_param->img_consumer_fifo)->arg.data, buffer1, 256, 144);
							int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
							datafifo_producer_data_add(thread_param->filtered_img_producer_fifo, buffer1, length, flags, num_in_group);
							//num_in_group++;
						}
					}
				}
			}

			if(datafifo_head(thread_param->img_consumer_fifo)->ref->datafifo == thread_param->img6){
				//to JPEG
				int length;
				yuv422_to_planner(datafifo_head(thread_param->img_consumer_fifo)->arg.data, buffer1, 256, 144);
				length = yuv422_to_jpeg(buffer1, buffer2, 256, 144, fd, 100);
				//transmit to data fifo
				JPEGPacknSend2df(buffer2, length, datafifo_head(thread_param->img_consumer_fifo)->arg.timestamp,
						datafifo_head(thread_param->img_consumer_fifo)->arg.timestamp, g_vmap_table[5].function, thread_param->JPEG_producer_fifo);
				//to PIP if it is PIP channel
				if(g_vmap_table[5].pip){
					length = yuv422_to_yuv420(datafifo_head(thread_param->img_consumer_fifo)->arg.data, buffer1, 256, 144);
					PIPPacknSend2df(buffer1, length, datafifo_head(thread_param->img_consumer_fifo)->arg.timestamp,
							datafifo_head(thread_param->img_consumer_fifo)->arg.timestamp, 0, thread_param->PIP_producer_fifo);
				}
				//to analyze if it need to be analyzed
				if(g_vmap_table[5].function == 1){
					int camholder_still;
					int analyze_frame_num;
					int num_in_group;

					pthread_mutex_lock(&g_capture_sync_mutex);
					camholder_still = g_camholder_still;
					analyze_frame_num = g_analyze_frame_num;
					num_in_group = g_num_in_group;

					if(g_camholder_still){
						g_analyze_frame_num++;
						if(g_analyze_frame_num%SKIP_STEP == 0){
							g_num_in_group++;
						}
					}
					pthread_mutex_unlock(&g_capture_sync_mutex);

					if(camholder_still){
						analyze_frame_num++;
						if(analyze_frame_num%SKIP_STEP == 0){
							length = yuv422_to_y(datafifo_head(thread_param->img_consumer_fifo)->arg.data, buffer1, 256, 144);
							int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
							datafifo_producer_data_add(thread_param->filtered_img_producer_fifo, buffer1, length, flags, num_in_group);
							//num_in_group++;
						}
					}
				}
			}

			datafifo_consumer_remove_head(thread_param->img_consumer_fifo);
		}
		usleep(1000);
	}
	fclose(fd);
	free(buffer1);
	free(buffer2);
	thread_param->thread_canceled = 2;
	debug_printf("thread_img exit\n");
	pthread_exit(0);
}

void thread_SwitchTracking_askexit(t_SwitchTracking_thread_param *ptr)
{
	if(ptr->thread_canceled == 0){
		ptr->thread_canceled = 1;
	}
}

int thread_SwitchTracking_exited(t_SwitchTracking_thread_param *ptr)
{
	return ptr->thread_canceled==2?1:0;
}

int byte2int(char *buf)
{
	int result=0;
	int i;
	//result = buf[0];
	for(i=0; i<4; i++){
		result <<= 4;
		result |= buf[i];
	}
	if(result&0x8000){
		result |= 0xffff0000;
	}
	return result;
}

void GetDetectResult(t_DetectResult *result, char *Msg)
{
	//char cmd[CMD_SIZE+1];
	unsigned int len;
	char *ptr;

	//memcpy(cmd, &Msg[VERSION_SIZE], CMD_SIZE);
	//cmd[CMD_SIZE] = '\0';

	char2hex(&Msg[VERSION_SIZE+CMD_SIZE], &len, DATA_LEN_SIZE);
	debug_printf("Total length of attributes: %d(%x)\n", len, len);

	ptr = &Msg[PACKET_HEADER_SIZE];

	memset(result, 0, sizeof(t_DetectResult));
	while(len){
		unsigned int name_len, value_len;
		char attr_name[16];
		char attr_value[16];

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

		if(!strcmp(attr_name, "Detected")){
			char2hex(attr_value, &result->detected, value_len);
		}

		if(!strcmp(attr_name, "AvgTop")){
			char2hex(attr_value, &result->avg_top, value_len);
		}

		if(!strcmp(attr_name, "AvgLeft")){
			char2hex(attr_value, &result->avg_left, value_len);
		}

		if(!strcmp(attr_name, "AvgBottom")){
			char2hex(attr_value, &result->avg_bottom, value_len);
		}

		if(!strcmp(attr_name, "AvgRight")){
			char2hex(attr_value, &result->avg_right, value_len);
		}

		if(!strcmp(attr_name, "MaxTop")){
			char2hex(attr_value, &result->max_top, value_len);
		}

		if(!strcmp(attr_name, "MaxLeft")){
			char2hex(attr_value, &result->max_left, value_len);
		}

		if(!strcmp(attr_name, "MaxBottom")){
			char2hex(attr_value, &result->max_bottom, value_len);
		}

		if(!strcmp(attr_name, "MaxRight")){
			char2hex(attr_value, &result->max_right, value_len);
		}
	}
}

void GetVGAResult(t_VGA_result *result, char *Msg)
{
	//char cmd[CMD_SIZE+1];
	unsigned int len;
	char *ptr;

	//memcpy(cmd, &Msg[VERSION_SIZE], CMD_SIZE);
	//cmd[CMD_SIZE] = '\0';

	char2hex(&Msg[VERSION_SIZE+CMD_SIZE], &len, DATA_LEN_SIZE);
	//debug_printf("Total length of attributes: %d(%x)\n", len, len);
	ptr = &Msg[PACKET_HEADER_SIZE];
	while(len){
		unsigned int name_len, value_len;
		char attr_name[16];
		char attr_value[2];

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

		if(!strcmp(attr_value, "1")){
			result->changed = 1;
		}
		else if(!strcmp(attr_value, "2")){
			result->changed = 2;
		}
		else{
			result->changed = 0;
		}
	}
}


void GetAuxResult(t_aux_result *result, char *Msg)
{
	//char cmd[CMD_SIZE+1];
	unsigned int len;
	char *ptr;

	//memcpy(cmd, &Msg[VERSION_SIZE], CMD_SIZE);
	//cmd[CMD_SIZE] = '\0';

	char2hex(&Msg[VERSION_SIZE+CMD_SIZE], &len, DATA_LEN_SIZE);
	debug_printf("Total length of attributes: %d(%x)\n", len, len);

	ptr = &Msg[PACKET_HEADER_SIZE];
	memset(result, 0, sizeof(t_aux_result));
	int channel = 0;
	while(len){
		unsigned int name_len, value_len;
		char attr_name[16];
		char attr_value[16];

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

		if(!strcmp(attr_name, "Channel")){
			char2hex(attr_value, (unsigned int *)&channel, value_len);
		}

		if(!strcmp(attr_name, "Targets")){
			char2hex(attr_value, (unsigned int *)&result->aux[channel].target_num, value_len);
		}

		if(!strcmp(attr_name, "MaxTop")){
			char2hex(attr_value, (unsigned int *)&result->aux[channel].max_top, value_len);
		}


		if(!strcmp(attr_name, "MaxLeft")){
			char2hex(attr_value, (unsigned int *)&result->aux[channel].max_left, value_len);
		}


		if(!strcmp(attr_name, "MaxBottom")){
			char2hex(attr_value, (unsigned int *)&result->aux[channel].max_bottom, value_len);
		}


		if(!strcmp(attr_name, "MaxRight")){
			char2hex(attr_value, (unsigned int *)&result->aux[channel].max_right, value_len);
		}
	}

	if(result->aux[0].target_num*result->aux[1].target_num == 0){
		result->student_in_standing = 0;
	}
	else if(result->aux[0].target_num*result->aux[1].target_num == 1){
		if((result->aux[0].max_right-result->aux[0].max_left)>240 && (result->aux[1].max_right-result->aux[1].max_left)>240){
			result->student_in_standing = 2;
		}
		else{
			result->student_in_standing = 1;
		}
	}
	else{
		result->student_in_standing = 2;
	}
}

#define ANGLE_H0 45
#define X1 1000
#define Y1 0
#define ANGLE_H1 135
#define X2 500
#define Y2 -200
#define Z2 100
#define ANGLE_H2 90
#define VIEW_ANGLE_A 90

void student_tracking_geometry_init(t_student_tracking_geometry *geometry)
{
	geometry->camera_geometry[0].x = 0;
	geometry->camera_geometry[0].y = 0;
	geometry->camera_geometry[0].z = 0;
	geometry->camera_geometry[0].angle_h = ANGLE_H0;
	geometry->camera_geometry[0].angle_v = 0;
	geometry->camera_geometry[0].view_angle = VIEW_ANGLE_A;
	geometry->camera_geometry[1].x = X1;
	geometry->camera_geometry[1].y = Y1;
	geometry->camera_geometry[1].z = 0;
	geometry->camera_geometry[1].angle_h = ANGLE_H1;
	geometry->camera_geometry[1].angle_v = 0;
	geometry->camera_geometry[1].view_angle = VIEW_ANGLE_A;
	geometry->camera_geometry[2].x = X2;
	geometry->camera_geometry[2].y = Y2;
	geometry->camera_geometry[2].z = Z2;
	geometry->camera_geometry[2].angle_h = ANGLE_H2;
	geometry->camera_geometry[2].angle_v = 0;
	geometry->last_location.x = 0;
	geometry->last_location.y = 0;
}

void get_student_tracking_param(t_student_tracking_geometry *geometry, t_aux_result *result)
{
	float offset0 = (result->aux[0].max_left + result->aux[0].max_right -720)/2;
	float offset1 = (result->aux[1].max_left + result->aux[1].max_right -720)/2;
	debug_printf("offset0=%f, offset1=%f\n", offset0, offset1);

	float alpha = atan(offset0/360*tan(geometry->camera_geometry[0].view_angle/2*M_PI/180));
	float beta = atan(offset1/360*tan(geometry->camera_geometry[1].view_angle/2*M_PI/180));
	debug_printf("alpha=%f, beta=%f\n", alpha*180/M_PI, beta*180/M_PI);

	float x1 = geometry->camera_geometry[1].x;
	float y1 = geometry->camera_geometry[1].y;
	debug_printf("x1=%f, y1=%f\n", x1, y1);

	float angle_h0 = geometry->camera_geometry[0].angle_h*M_PI/180;
	float angle_h1 = geometry->camera_geometry[1].angle_h*M_PI/180;
	debug_printf("angle_h0=%f, angle_h1=%f\n", angle_h0*180/M_PI, angle_h1*180/M_PI);

	float A0 = angle_h0-alpha;
	if(A0>M_PI/2*89/90){
		A0 = M_PI/2*89/90;
	}
	float A1 = angle_h1-beta;
	if(A1< M_PI/2*91/90){
		A1 = M_PI/2*91/90;
	}
	debug_printf("A0=%f, A1=%f\n", A0*180/M_PI, A1*180/M_PI);

	if((tan(A1)-tan(A0))*1000000>=1 || (tan(A1)-tan(A0))*1000000<=-1){
		geometry->student_location.x = (x1*tan(A1)-y1)/(tan(A1)-tan(A0));
		geometry->student_location.y = tan(A0)*geometry->student_location.x;
	}
	else{
		geometry->student_location.x = x1/2;
		geometry->student_location.y = y1/2;
	}
	debug_printf("x=%d, y=%d\n", geometry->student_location.x, geometry->student_location.y);

	float x2 = geometry->camera_geometry[2].x;
	float y2 = geometry->camera_geometry[2].y;
	float z2 = geometry->camera_geometry[2].z;

	float r = sqrt((x2-geometry->student_location.x)*(x2-geometry->student_location.x)
				+ (y2-geometry->student_location.y)*(y2-geometry->student_location.y)
				+ z2*z2);

	geometry->target_view_angle = 2*atan(200/r);
	geometry->target_tilt_angle = atan(-z2/sqrt((x2-geometry->student_location.x)*(x2-geometry->student_location.x)+(y2-geometry->student_location.y)*(y2-geometry->student_location.y)));
	geometry->target_pan_angle = atan((geometry->student_location.x-x2)/(geometry->student_location.y-y2));
	debug_printf("target_pan_angle=%f, target_tilt_angle=%f, target_view_angle=%f\n", geometry->target_pan_angle*180/M_PI, geometry->target_tilt_angle*180/M_PI, geometry->target_view_angle*180/M_PI);

	float s0 = sqrt(geometry->student_location.x*geometry->student_location.x + geometry->student_location.y*geometry->student_location.y);
	float s1 = sqrt((geometry->student_location.x-x1)*(geometry->student_location.x-x1)+(geometry->student_location.y-y1)*(geometry->student_location.y-y1));

	if(s0*tan(((float)(result->aux[0].max_right-result->aux[0].max_left))*geometry->camera_geometry[0].view_angle*M_PI/720/180)<50){
		result->aux[0].target_num = 1;
	}
	else{
		result->aux[0].target_num = 2;
	}

	if(s1*tan(((float)(result->aux[1].max_right-result->aux[1].max_left))*geometry->camera_geometry[1].view_angle*M_PI/720/180)<50){
		result->aux[1].target_num = 1;
	}
	else{
		result->aux[1].target_num = 2;
	}

	result->student_in_standing = result->aux[0].target_num*result->aux[1].target_num;
	debug_printf("student_in_standing = %d\n", result->student_in_standing);

	geometry->dotracking = 0;
	if(result->student_in_standing==1){
		int deviation = sqrt((geometry->student_location.x-geometry->last_location.x)*(geometry->student_location.x-geometry->last_location.x)
								+(geometry->student_location.y-geometry->last_location.y)*(geometry->student_location.y-geometry->last_location.y));
		if(deviation>100){
			geometry->dotracking = 1;
			geometry->last_location.x = geometry->student_location.x;
			geometry->last_location.y = geometry->student_location.y;
			debug_printf("do student tracking\n");
		}
	}
}

void CamhldCmdSend(t_SwitchTracking_thread_param *p_camhldctrl, t_camera_holder *camera_holder, char* cwbuf, unsigned int cmdlen)
{
#if 1
	unsigned int len;
	t_cmd cmd;
	int flags;
	fill_cmd_header(&cmd, COMMAND_CAMHLD_BYPASS);
	add_attr_int(&cmd, "Channel", (camera_holder->uart_channel-1)%2);
	add_attr_bin(&cmd, "RawBytes", cwbuf, cmdlen);

	char2hex(cmd.data_len, &len, DATA_LEN_SIZE);
	len += PACKET_HEADER_SIZE;
	flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;

	if(camera_holder->uart_channel == 1 || camera_holder->uart_channel == 2){
		datafifo_producer_data_add(p_camhldctrl->TX_producer_fifo_A, (unsigned char *)cmd.fullbuf, (int)len, flags, 0);
	}

	if(camera_holder->uart_channel == 3 || camera_holder->uart_channel == 4){
		datafifo_producer_data_add(p_camhldctrl->TX_producer_fifo_B, (unsigned char *)cmd.fullbuf, (int)len, flags, 0);
	}

	if(camera_holder->uart_channel == 5 || camera_holder->uart_channel == 6){
		datafifo_producer_data_add(p_camhldctrl->TX_producer_fifo_C, (unsigned char *)cmd.fullbuf, (int)len, flags, 0);
	}

#endif
}

void GetZoom(t_SwitchTracking_thread_param *p_camhldctrl, t_camera_holder *camera_holder)
{
	char cwbuf[] = {0x81, 0x09, 0x04, 0x47, 0xff};

	CamhldCmdSend(p_camhldctrl, camera_holder, cwbuf, 5);
	camera_holder->command_pending |= CH_ZOOMINQ_PENDING;
	gettimeofday(&camera_holder->tv, NULL);
}

void GetPos(t_SwitchTracking_thread_param *p_camhldctrl, t_camera_holder *camera_holder)
{
	char cwbuf[] = {0x81, 0x09, 0x06, 0x12, 0xff};

	CamhldCmdSend(p_camhldctrl, camera_holder, cwbuf, 5);
	camera_holder->command_pending |= CH_PTINQ_PENDING;
	gettimeofday(&camera_holder->tv, NULL);
}

void AbsPos(t_SwitchTracking_thread_param *p_camhldctrl, t_camera_holder *camera_holder, int panpos, int tiltpos)
{
	char cwbuf[] = {0x81, 0x01, 0x06, 0x02, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff};
	cwbuf[4] = camera_holder->panspeed_use;
	cwbuf[5] = camera_holder->tiltspeed_use;
	cwbuf[6] = (panpos&0xf000)>>12;
	cwbuf[7] = (panpos&0xf00)>>8;
	cwbuf[8] = (panpos&0xf0)>>4;
	cwbuf[9] = (panpos&0xf);
	cwbuf[10] = (tiltpos&0xf000)>>12;
	cwbuf[11] = (tiltpos&0xf00)>>8;
	cwbuf[12] = (tiltpos&0xf0)>>4;
	cwbuf[13] = (tiltpos&0xf);
	CamhldCmdSend(p_camhldctrl, camera_holder, cwbuf, 15);

	camera_holder->target_PanPos = panpos;
	camera_holder->target_TiltPos = tiltpos;

	camera_holder->command_pending |= CH_COMPLETION_PENDING;
	gettimeofday(&camera_holder->tv, NULL);
}

void ZoomDirect(t_SwitchTracking_thread_param *p_camhldctrl, t_camera_holder *camera_holder, int zoompos)
{
	char cwbuf[] = {0x81, 0x01, 0x04, 0x47, 0x0, 0x0, 0x0, 0x0, 0xff};
	cwbuf[4] = (zoompos&0xf000)>>12;
	cwbuf[5] = (zoompos&0xf00)>>8;
	cwbuf[6] = (zoompos&0xf0)>>4;
	cwbuf[7] = (zoompos&0xf);
	CamhldCmdSend(p_camhldctrl, camera_holder, cwbuf, 9);

	camera_holder->target_ZoomPos = zoompos;

	camera_holder->command_pending |= CH_COMPLETION_PENDING;
	gettimeofday(&camera_holder->tv, NULL);
}

void Recall(t_SwitchTracking_thread_param *p_camhldctrl, t_camera_holder *camera_holder, unsigned char num)
{
	char cwbuf[] = {0x81, 0x01, 0x04, 0x3f, 0x02, 0x00, 0xff};
	cwbuf[5] = num;
	CamhldCmdSend(p_camhldctrl, camera_holder, cwbuf, 7);

	camera_holder->target_PanPos = camera_holder->preset[num].pan_pos;
	camera_holder->target_TiltPos = camera_holder->preset[num].tilt_pos;
	camera_holder->target_ZoomPos = camera_holder->preset[num].zoom_pos;

	camera_holder->command_pending |= CH_COMPLETION_PENDING;
	gettimeofday(&camera_holder->tv, NULL);
}

void MemSet(t_SwitchTracking_thread_param *p_camhldctrl, t_camera_holder *camera_holder, unsigned char num)
{
	char cwbuf[] = {0x81, 0x01, 0x04, 0x3f, 0x01, 0x00, 0xff};
	cwbuf[5] = num;
	CamhldCmdSend(p_camhldctrl, camera_holder, cwbuf, 7);

	camera_holder->preset[num].pan_pos = camera_holder->target_PanPos = camera_holder->PanPos;
	camera_holder->preset[num].tilt_pos = camera_holder->target_TiltPos = camera_holder->TiltPos;
	camera_holder->preset[num].zoom_pos = camera_holder->target_ZoomPos = camera_holder->ZoomPos;

	camera_holder->command_pending |= CH_COMPLETION_PENDING;
	gettimeofday(&camera_holder->tv, NULL);
}

void update_camholder_ctx(t_camera_holder *camera_holder)
{
	if(camera_holder->RspLen==11){//PT_pos response
		if(camera_holder->Rx_buf[0]==(char)0x90 && camera_holder->Rx_buf[1]==(char)0x50){
			camera_holder->PanPos = byte2int(&camera_holder->Rx_buf[2]);
			camera_holder->TiltPos = byte2int(&camera_holder->Rx_buf[6]);
			//clear corresponding pending flag
			camera_holder->command_pending = camera_holder->command_pending & (!CH_PTINQ_PENDING);
		}
	}

	if(camera_holder->RspLen == 7){//Zoom_pos response
		if(camera_holder->Rx_buf[0]==(char)0x90 && camera_holder->Rx_buf[1]==(char)0x50){
			camera_holder->ZoomPos = byte2int(&camera_holder->Rx_buf[2]);
			//clear corresponding pending flag
			camera_holder->command_pending = camera_holder->command_pending & (!CH_ZOOMINQ_PENDING);
		}
	}

	if(camera_holder->RspLen == 3){
		if(camera_holder->Rx_buf[0]==(char)0x90 && camera_holder->Rx_buf[1]==(char)0x51){
			//clear corresponding pending flag
			camera_holder->PanPos = camera_holder->target_PanPos;
			camera_holder->TiltPos = camera_holder->target_TiltPos;
			camera_holder->ZoomPos = camera_holder->target_ZoomPos;
			camera_holder->command_pending = camera_holder->command_pending & (!CH_COMPLETION_PENDING);
		}
	}

	if(camera_holder->RspLen == 4){
		if(camera_holder->Rx_buf[0]==(char)0x90 && (camera_holder->Rx_buf[1]==(char)0x60 || camera_holder->Rx_buf[1]==(char)0x61)){
			//error. clear all pending flags and set the error flag
			camera_holder->command_pending = 0;
			camera_holder->command_pending |= CH_ERROR;
		}
	}
}

void save_camholder_preset(t_camera_holder *camera_holder, int num)
{
	camera_holder->preset[num].pan_pos = camera_holder->PanPos;
	camera_holder->preset[num].tilt_pos = camera_holder->TiltPos;
	camera_holder->preset[num].zoom_pos = camera_holder->ZoomPos;
}

int get_camholder_pending_state(t_camera_holder *camera_holder)
{
	if(camera_holder->command_pending){
		return 1;
	}
	else{
		return 0;
	}
}

int get_camholder_err(t_camera_holder *camera_holder)
{
	if(camera_holder->command_pending & CH_ERROR){
		return 1;
	}
	else{
		return 0;
	}
}

void camholder_reset_pending_state(t_camera_holder *camera_holder)
{
	camera_holder->command_pending = 0;
}

int camholder_command_timeout(t_camera_holder *camera_holder, int timeout)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	int delt = (tv.tv_sec - camera_holder->tv.tv_sec) + (tv.tv_usec - camera_holder->tv.tv_usec)/1000000;
	if(delt>timeout){
		return 1;
	}
	else{
		return 0;
	}
}

void camera_holder_init(t_camera_holder *camera_holder)
{
	camera_holder->Rx_buf_p = camera_holder->Rx_buf;
	camera_holder->command_pending = 0;
	camera_holder->pan_angle_left = -120;
	camera_holder->pan_angle_right = 120;
	camera_holder->pan_min = -1600;
	camera_holder->pan_max = 1600;
	camera_holder->panspeed_use = 4;
	camera_holder->view_angle_max = 55.2;
	camera_holder->view_angle_min = 3.2;
	camera_holder->zoom_min = 0;
	camera_holder->zoom_max = 16384;
	camera_holder->tilt_angle_down = -30;
	camera_holder->tilt_angle_up = 90;
	camera_holder->tilt_min = -400;
	camera_holder->tilt_max = 1200;
	camera_holder->tiltspeed_use = 4;
}

int truth_table_init(int item_num, t_truth_table * truth_table)
{
	truth_table->item_num = item_num;
	memset(truth_table->cond, 0, item_num*sizeof(t_cond_item));
	return 0;
}

t_cond_item * truth_table_find_condition(int cond_idx, t_truth_table * truth_table)
{
	t_cond_item * ret = NULL;
	int i;
	for(i= 0; i<truth_table->item_num; i++){
		if(truth_table->cond[i].valid){
			if(truth_table->cond[i].cond_idx == cond_idx){
				ret = &truth_table->cond[i];
				break;
			}
		}
	}
	return ret;
}

t_cond_item * truth_table_alloc_conditon_item(t_truth_table *truth_table)
{
	t_cond_item * ret = NULL;
	int i;
	for(i= 0; i<truth_table->item_num; i++){
		if(!truth_table->cond[i].valid){
			ret = &truth_table->cond[i];
			break;
		}
	}
	return ret;
}

t_cond_item * truth_table_add_element_condition(int cond_idx, t_truth_table * truth_table)
{
	t_cond_item * ret = NULL;
	ret = truth_table_find_condition(cond_idx, truth_table);
	if(!ret){
		ret = truth_table_alloc_conditon_item(truth_table);
		if(ret){
			ret->cond_idx = cond_idx;
			ret->cond_type = 0;
			ret->value = 0;
			ret->valid = 1;
		}
	}
	return ret;
}

t_cond_item * truth_table_add_combination_condition(int cond_idx, int element_num, int *element, t_truth_table * truth_table)
{
	t_cond_item * ret = NULL;
	ret = truth_table_find_condition(cond_idx, truth_table);
	if(!ret){
		t_cond_item cond_item;
		memset(&cond_item, 0, sizeof(t_cond_item));
		cond_item.cond_idx = cond_idx;
		cond_item.cond_type = 1;
		cond_item.num_of_combined_element = element_num;
		cond_item.valid = 1;

		int i;
		for(i=0; i<element_num; i++){
			cond_item.element[i] = truth_table_find_condition(element[i], truth_table);
			if(!cond_item.element[i]){
				break;
			}
		}

		if(i == element_num){
			ret = truth_table_alloc_conditon_item(truth_table);
			if(ret){
				memcpy(ret, &cond_item, sizeof(t_cond_item));
				//printf("cond_combination_%d added\n", cond_idx+1-COND_COMBINATION_1);
			}
		}

	}
	return ret;
}

void truth_table_update_combination_condition(t_truth_table * truth_table)
{
	int i;
	for(i=0; i<truth_table->item_num; i++){
		if(truth_table->cond[i].valid && truth_table->cond[i].cond_type){
			int j;
			int value = 1;
			for(j=0; j<truth_table->cond[i].num_of_combined_element; j++){
				value = value && truth_table->cond[i].element[j]->value;
			}
			truth_table->cond[i].value = value;
		}
	}
}

int truth_table_element_condition_set(int cond_idx, t_truth_table * truth_table)
{
	int ret = 0;
	t_cond_item *cond_item = truth_table_find_condition(cond_idx, truth_table);
	if(cond_item){
		cond_item->value = 1;
	}
	else{
		ret = -1;
	}
	truth_table_update_combination_condition(truth_table);
	return ret;
}

int truth_table_element_condition_reset(int cond_idx, t_truth_table * truth_table)
{
	int ret = 0;
	t_cond_item *cond_item = truth_table_find_condition(cond_idx, truth_table);
	if(cond_item){
		cond_item->value = 0;
	}
	else{
		ret = -1;
	}
	truth_table_update_combination_condition(truth_table);
	return ret;
}

int truth_table_condition_test(int cond_idx, t_truth_table * truth_table)
{
	int ret = 0;
	t_cond_item *cond_item = truth_table_find_condition(cond_idx, truth_table);
	if(cond_item){
		ret = cond_item->value;
	}
	return ret;
}

void transition_table_init(t_transition_table *transition_table)
{
	memset(transition_table, -1, sizeof(t_transition_table));
#if 1
	//current state=FSM_VGA
	transition_table->item[FSM_VGA][COND_VGA_UNCHG] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA][COND_VGA_CHG] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA][COND_TEACHER_DETECTED_NONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA][COND_TEACHER_DETECTED_ONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA][COND_TEACHER_DETECTED_MULTIPLE] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA][COND_STUDENT_STAND_NONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA][COND_STUDENT_STAND_ONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA][COND_STUDENT_STAND_MULTIPLE] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA][COND_BOARD_WRITE] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA][COND_BOARD_NOTWRITE] = FSM_NOTRANSITION;

	transition_table->item[FSM_VGA][COND_COMBINATION_1] = FSM_STUDENT_PANORAMA;
	transition_table->item[FSM_VGA][COND_COMBINATION_2] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA][COND_COMBINATION_3] = FSM_TEACHER_CLOSEUP;
	transition_table->item[FSM_VGA][COND_COMBINATION_4] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA][COND_COMBINATION_5] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_VGA][COND_COMBINATION_6] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA][COND_COMBINATION_7] = FSM_STUDENT_CLOSEUP;
	transition_table->item[FSM_VGA][COND_COMBINATION_8] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA][COND_COMBINATION_9] = FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_VGA][COND_COMBINATION_10] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA][COND_COMBINATION_11] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_VGA][COND_COMBINATION_12] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA][COND_COMBINATION_13] = FSM_STUDENT_PANORAMA;
	transition_table->item[FSM_VGA][COND_COMBINATION_14] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA][COND_COMBINATION_15] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_VGA][COND_COMBINATION_16] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA][COND_COMBINATION_17] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_VGA][COND_COMBINATION_18] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA][COND_COMBINATION_19] = FSM_BOARD_CLOSEUP;
	transition_table->item[FSM_VGA][COND_COMBINATION_20] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA][COND_COMBINATION_21] = FSM_BOARD_CLOSEUP;
	transition_table->item[FSM_VGA][COND_COMBINATION_22] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA][COND_COMBINATION_23] = FSM_BOARD_CLOSEUP;
	transition_table->item[FSM_VGA][COND_COMBINATION_24] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA][COND_COMBINATION_25] = FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_VGA][COND_COMBINATION_26] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA][COND_COMBINATION_27] = FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_VGA][COND_COMBINATION_28] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA][COND_COMBINATION_29] = FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_VGA][COND_COMBINATION_30] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA][COND_COMBINATION_31] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_VGA][COND_COMBINATION_32] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA][COND_COMBINATION_33] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_VGA][COND_COMBINATION_34] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA][COND_COMBINATION_35] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_VGA][COND_COMBINATION_36] = FSM_NOTRANSITION;

	//current state=FSM_TEACHER_CLOSEUP
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_VGA_UNCHG] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_VGA_CHG] = FSM_VGA_PIP_TEACHER_CLOSEUP;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_TEACHER_DETECTED_NONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_TEACHER_DETECTED_ONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_TEACHER_DETECTED_MULTIPLE] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_STUDENT_STAND_NONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_STUDENT_STAND_ONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_STUDENT_STAND_MULTIPLE] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_BOARD_WRITE] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_BOARD_NOTWRITE] = FSM_NOTRANSITION;

	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_1] = FSM_STUDENT_PANORAMA;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_2] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_3] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_4] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_5] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_6] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_7] = FSM_STUDENT_CLOSEUP;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_8] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_9] = FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_10] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_11] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_12] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_13] = FSM_STUDENT_PANORAMA;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_14] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_15] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_16] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_17] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_18] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_19] = FSM_BOARD_CLOSEUP;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_20] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_21] = FSM_BOARD_CLOSEUP;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_22] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_23] = FSM_BOARD_CLOSEUP;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_24] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_25] = FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_26] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_27] = FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_28] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_29] = FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_30] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_31] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_32] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_33] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_34] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_35] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_TEACHER_CLOSEUP][COND_COMBINATION_36] = FSM_NOTRANSITION;

	//current state=FSM_STUDENT_CLOSEUP
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_VGA_UNCHG] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_VGA_CHG] = FSM_VGA;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_TEACHER_DETECTED_NONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_TEACHER_DETECTED_ONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_TEACHER_DETECTED_MULTIPLE] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_STUDENT_STAND_NONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_STUDENT_STAND_ONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_STUDENT_STAND_MULTIPLE] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_BOARD_WRITE] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_BOARD_NOTWRITE] = FSM_NOTRANSITION;

	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_1] = FSM_STUDENT_PANORAMA;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_2] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_3] = FSM_TEACHER_CLOSEUP;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_4] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_5] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_6] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_7] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_8] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_9] = FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_10] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_11] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_12] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_13] = FSM_STUDENT_PANORAMA;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_14] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_15] = FSM_STUDENT_PANORAMA;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_16] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_17] = FSM_STUDENT_PANORAMA;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_18] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_19] = FSM_BOARD_CLOSEUP;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_20] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_21] = FSM_BOARD_CLOSEUP;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_22] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_23] = FSM_BOARD_CLOSEUP;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_24] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_25] = FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_26] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_27] = FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_28] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_29] = FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_30] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_31] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_32] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_33] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_34] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_35] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_STUDENT_CLOSEUP][COND_COMBINATION_36] = FSM_NOTRANSITION;

	//current state=FSM_TEACHER_PANORAMA
	transition_table->item[FSM_TEACHER_PANORAMA][COND_VGA_UNCHG] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_VGA_CHG] = FSM_VGA;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_TEACHER_DETECTED_NONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_TEACHER_DETECTED_ONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_TEACHER_DETECTED_MULTIPLE] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_STUDENT_STAND_NONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_STUDENT_STAND_ONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_STUDENT_STAND_MULTIPLE] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_BOARD_WRITE] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_BOARD_NOTWRITE] = FSM_NOTRANSITION;

	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_1] = FSM_STUDENT_PANORAMA;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_2] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_3] = FSM_TEACHER_CLOSEUP;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_4] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_5] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_6] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_7] = FSM_STUDENT_CLOSEUP;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_8] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_9] = FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_10] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_11] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_12] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_13] = FSM_STUDENT_PANORAMA;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_14] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_15] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_16] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_17] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_18] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_19] = FSM_BOARD_CLOSEUP;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_20] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_21] = FSM_BOARD_CLOSEUP;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_22] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_23] = FSM_BOARD_CLOSEUP;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_24] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_25] = FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_26] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_27] = FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_28] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_29] = FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_30] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_31] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_32] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_33] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_34] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_35] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_PANORAMA][COND_COMBINATION_36] = FSM_NOTRANSITION;

	//current state=FSM_STUDENT_PANORAMA
	transition_table->item[FSM_STUDENT_PANORAMA][COND_VGA_UNCHG] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_VGA_CHG] = FSM_VGA;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_TEACHER_DETECTED_NONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_TEACHER_DETECTED_ONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_TEACHER_DETECTED_MULTIPLE] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_STUDENT_STAND_NONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_STUDENT_STAND_ONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_STUDENT_STAND_MULTIPLE] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_BOARD_WRITE] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_BOARD_NOTWRITE] = FSM_NOTRANSITION;

	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_1] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_2] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_3] = FSM_TEACHER_CLOSEUP;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_4] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_5] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_6] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_7] = FSM_STUDENT_CLOSEUP;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_8] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_9] = FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_10] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_11] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_12] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_13] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_14] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_15] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_16] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_17] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_18] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_19] = FSM_BOARD_CLOSEUP;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_20] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_21] = FSM_BOARD_CLOSEUP;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_22] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_23] = FSM_BOARD_CLOSEUP;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_24] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_25] = FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_26] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_27] = FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_28] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_29] = FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_30] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_31] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_32] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_33] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_34] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_35] = FSM_NOTRANSITION;
	transition_table->item[FSM_STUDENT_PANORAMA][COND_COMBINATION_36] = FSM_NOTRANSITION;

	//current state=FSM_BOARD_CLOSEUP
	transition_table->item[FSM_BOARD_CLOSEUP][COND_VGA_UNCHG] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_VGA_CHG] = FSM_VGA_PIP_TEACHER_CLOSEUP;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_TEACHER_DETECTED_NONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_TEACHER_DETECTED_ONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_TEACHER_DETECTED_MULTIPLE] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_STUDENT_STAND_NONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_STUDENT_STAND_ONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_STUDENT_STAND_MULTIPLE] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_BOARD_WRITE] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_BOARD_NOTWRITE] = FSM_NOTRANSITION;

	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_1] = FSM_STUDENT_PANORAMA;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_2] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_3] = FSM_TEACHER_CLOSEUP;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_4] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_5] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_6] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_7] = FSM_STUDENT_CLOSEUP;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_8] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_9] = FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_10] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_11] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_12] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_13] = FSM_STUDENT_PANORAMA;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_14] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_15] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_16] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_17] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_18] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_19] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_20] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_21] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_22] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_23] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_24] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_25] = FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_26] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_27] = FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_28] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_29] = FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_30] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_31] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_32] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_33] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_34] = FSM_NOTRANSITION;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_35] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_BOARD_CLOSEUP][COND_COMBINATION_36] = FSM_NOTRANSITION;

	//current state=FSM_VGA_PIP_TEACHER_CLOSEUP
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_VGA_UNCHG] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_VGA_CHG] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_TEACHER_DETECTED_NONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_TEACHER_DETECTED_ONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_TEACHER_DETECTED_MULTIPLE] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_STUDENT_STAND_NONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_STUDENT_STAND_ONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_STUDENT_STAND_MULTIPLE] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_BOARD_WRITE] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_BOARD_NOTWRITE] = FSM_NOTRANSITION;

	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_1] = FSM_STUDENT_PANORAMA;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_2] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_3] = FSM_TEACHER_CLOSEUP;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_4] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_5] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_6] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_7] = FSM_STUDENT_CLOSEUP;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_8] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_9] = FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_10] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_11] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_12] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_13] = FSM_STUDENT_PANORAMA;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_14] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_15] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_16] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_17] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_18] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_19] = FSM_BOARD_CLOSEUP;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_20] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_21] = FSM_BOARD_CLOSEUP;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_22] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_23] = FSM_BOARD_CLOSEUP;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_24] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_25] = FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_26] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_27] = FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_28] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_29] = FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_30] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_31] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_32] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_33] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_34] = FSM_NOTRANSITION;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_35] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_VGA_PIP_TEACHER_CLOSEUP][COND_COMBINATION_36] = FSM_NOTRANSITION;

	//current state=FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_VGA_UNCHG] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_VGA_CHG] = FSM_VGA_PIP_TEACHER_CLOSEUP;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_TEACHER_DETECTED_NONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_TEACHER_DETECTED_ONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_TEACHER_DETECTED_MULTIPLE] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_STUDENT_STAND_NONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_STUDENT_STAND_ONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_STUDENT_STAND_MULTIPLE] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_BOARD_WRITE] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_BOARD_NOTWRITE] = FSM_NOTRANSITION;

	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_1] = FSM_STUDENT_PANORAMA;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_2] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_3] = FSM_TEACHER_CLOSEUP;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_4] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_5] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_6] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_7] = FSM_STUDENT_CLOSEUP;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_8] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_9] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_10] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_11] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_12] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_13] = FSM_STUDENT_PANORAMA;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_14] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_15] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_16] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_17] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_18] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_19] = FSM_BOARD_CLOSEUP;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_20] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_21] = FSM_BOARD_CLOSEUP;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_22] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_23] = FSM_BOARD_CLOSEUP;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_24] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_25] = FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_26] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_27] = FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_28] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_29] = FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_30] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_31] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_32] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_33] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_34] = FSM_NOTRANSITION;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_35] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP][COND_COMBINATION_36] = FSM_NOTRANSITION;

	//current state=FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_VGA_UNCHG] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_VGA_CHG] = FSM_VGA_PIP_TEACHER_CLOSEUP;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_TEACHER_DETECTED_NONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_TEACHER_DETECTED_ONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_TEACHER_DETECTED_MULTIPLE] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_STUDENT_STAND_NONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_STUDENT_STAND_ONE] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_STUDENT_STAND_MULTIPLE] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_BOARD_WRITE] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_BOARD_NOTWRITE] = FSM_NOTRANSITION;

	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_1] = FSM_STUDENT_PANORAMA;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_2] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_3] = FSM_TEACHER_CLOSEUP;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_4] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_5] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_6] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_7] = FSM_STUDENT_CLOSEUP;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_8] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_9] = FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_10] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_11] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_12] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_13] = FSM_STUDENT_PANORAMA;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_14] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_15] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_16] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_17] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_18] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_19] = FSM_BOARD_CLOSEUP;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_20] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_21] = FSM_BOARD_CLOSEUP;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_22] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_23] = FSM_BOARD_CLOSEUP;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_24] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_25] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_26] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_27] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_28] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_29] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_30] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_31] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_32] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_33] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_34] = FSM_NOTRANSITION;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_35] = FSM_TEACHER_PANORAMA;
	transition_table->item[FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP][COND_COMBINATION_36] = FSM_NOTRANSITION;
#endif
}

void fsm_init(t_finite_state_machine *fsm, t_truth_table *truth_table, t_transition_table *transition_table)
{
	fsm->initial_state = FSM_TEACHER_PANORAMA;
	fsm->current_state = FSM_TEACHER_PANORAMA;
	fsm->truth_table = truth_table;
	fsm->transition_table = transition_table;
}

void fsm_set_state(t_finite_state_machine *fsm, char state)
{
	fsm->current_state = state;
}

char fsm_get_state(t_finite_state_machine *fsm)
{
	return fsm->current_state;
}

char fsm_state_update(t_finite_state_machine *fsm)
{
	char ret = FSM_NOTRANSITION;
	int i;
	for(i=0; i<COND_MAX; i++){
		if(fsm->transition_table->item[(unsigned char)fsm->current_state][i] != FSM_NOTRANSITION){
			if(truth_table_condition_test(i, fsm->truth_table)){
				ret = fsm->transition_table->item[(unsigned)fsm->current_state][i];
				fsm->current_state = ret;
				break;
			}
		}
	}
	return ret;
}

void * thread_SwitchTracking(void * param)
{
	pthread_detach(pthread_self());
	debug_printf("Thread thread_SwitchTracking running...\n");

	t_SwitchTracking_thread_param  *thread_param = (t_SwitchTracking_thread_param *)param;

	unsigned int acc_student_lost = 0;
	unsigned int acc_of_lost = 0;
	unsigned int motion_in_screen = 0;
	unsigned int still_in_screen = 0;
	unsigned int in_left = 0;
	unsigned int in_right = 0;
	int need_hiden = 0;
	//unsigned int motion_of_lost = 0;
	int direction = 1;
	unsigned int IsMultipleTeacher = 0;
	unsigned int motion_skip_num = 1;
	unsigned int acc_of_unchange = 0;
	int limit_left = -400;
	int limit_right = 400;
	int exclude_left = 0;
	int exclude_right = 0;

	struct timeval timeval1, timeval2;

	t_camera_holder teacher;
	camera_holder_init(&teacher);
	int vmaptable_idx = vmap_table_get_index_by_function(VIDEO_FUNCTION_TEACHER_CLOSEUP);
	if(vmaptable_idx!=-1){
		cam_param_load(PTZ_CONFIG_FILE_NAME, g_vmap_table[vmaptable_idx].cam_vendor, g_vmap_table[vmaptable_idx].cam_model, &teacher);
	}
	teacher_pt_speed_load(SYS_CONFIG_FILE_NAME, &teacher);

	int IsExclude;
	teacher_exclude_load(SYS_CONFIG_FILE_NAME, &IsExclude);

	t_camera_holder student;
	camera_holder_init(&student);
	vmaptable_idx = vmap_table_get_index_by_function(VIDEO_FUNCTION_STUDENT_CLOSEUP);
	if(vmaptable_idx!=-1){
		cam_param_load(PTZ_CONFIG_FILE_NAME, g_vmap_table[vmaptable_idx].cam_vendor, g_vmap_table[vmaptable_idx].cam_model, &student);
	}
	student_pt_speed_load(SYS_CONFIG_FILE_NAME, &student);

	t_student_tracking_geometry student_tracking_geometry;
	student_tracking_geometry_init(&student_tracking_geometry);
	student_tracking_geometry_load(SYS_CONFIG_FILE_NAME, &student_tracking_geometry);

	t_screen_geometry screen_geometry;
	screen_geometry.has_screen = 0;
	screen_geometry_load(SYS_CONFIG_FILE_NAME, &screen_geometry);
	float screen_angle_left = 0, screen_angle_right = 0;
	if(screen_geometry.has_screen){
		screen_angle_left = atan((float)screen_geometry.x_left/screen_geometry.y)*180/M_PI;
		screen_angle_right = atan((float)screen_geometry.x_right/screen_geometry.y)*180/M_PI;
		debug_printf("screen_angle_left = %f, screen_angle_right = %f\n", screen_angle_left, screen_angle_right);
	}

#if 0
	int i;
	for(i=0; i<6; i++){
		if(g_vmap_table[i].function == 1){
			teacher.uart_channel = g_vmap_table[i].uart_channel;
		}

		if(g_vmap_table[i].function == 2){
			student.uart_channel = g_vmap_table[i].uart_channel;
		}
	}
#else
	teacher.uart_channel = 1;
	student.uart_channel = 3;
#endif

	thread_param->teacher_status = 0;
	thread_param->student_status = 0;

	t_truth_table truth_table;

	truth_table_init(COND_MAX, &truth_table);
	//add element conditions
	truth_table_add_element_condition(COND_VGA_UNCHG, &truth_table);
	truth_table_add_element_condition(COND_VGA_CHG, &truth_table);

	truth_table_add_element_condition(COND_TEACHER_DETECTED_NONE, &truth_table);
	truth_table_add_element_condition(COND_TEACHER_DETECTED_ONE, &truth_table);
	truth_table_add_element_condition(COND_TEACHER_DETECTED_MULTIPLE, &truth_table);

	truth_table_add_element_condition(COND_STUDENT_STAND_NONE, &truth_table);
	truth_table_add_element_condition(COND_STUDENT_STAND_ONE, &truth_table);
	truth_table_add_element_condition(COND_STUDENT_STAND_MULTIPLE, &truth_table);

	truth_table_add_element_condition(COND_BOARD_WRITE, &truth_table);
	truth_table_add_element_condition(COND_BOARD_NOTWRITE, &truth_table);

	//add combined conditions
	int element_idx[MAX_ELEMENT_NUM];

	element_idx[0] = COND_VGA_UNCHG;
	element_idx[1] = COND_TEACHER_DETECTED_NONE;
	element_idx[2] = COND_STUDENT_STAND_NONE;
	element_idx[3] = COND_BOARD_NOTWRITE;
	truth_table_add_combination_condition(COND_COMBINATION_1, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_CHG;
	element_idx[1] = COND_TEACHER_DETECTED_NONE;
	element_idx[2] = COND_STUDENT_STAND_NONE;
	element_idx[3] = COND_BOARD_NOTWRITE;
	truth_table_add_combination_condition(COND_COMBINATION_2, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_UNCHG;
	element_idx[1] = COND_TEACHER_DETECTED_ONE;
	element_idx[2] = COND_STUDENT_STAND_NONE;
	element_idx[3] = COND_BOARD_NOTWRITE;
	truth_table_add_combination_condition(COND_COMBINATION_3, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_CHG;
	element_idx[1] = COND_TEACHER_DETECTED_ONE;
	element_idx[2] = COND_STUDENT_STAND_NONE;
	element_idx[3] = COND_BOARD_NOTWRITE;
	truth_table_add_combination_condition(COND_COMBINATION_4, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_UNCHG;
	element_idx[1] = COND_TEACHER_DETECTED_MULTIPLE;
	element_idx[2] = COND_STUDENT_STAND_NONE;
	element_idx[3] = COND_BOARD_NOTWRITE;
	truth_table_add_combination_condition(COND_COMBINATION_5, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_CHG;
	element_idx[1] = COND_TEACHER_DETECTED_MULTIPLE;
	element_idx[2] = COND_STUDENT_STAND_NONE;
	element_idx[3] = COND_BOARD_NOTWRITE;
	truth_table_add_combination_condition(COND_COMBINATION_6, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_UNCHG;
	element_idx[1] = COND_TEACHER_DETECTED_NONE;
	element_idx[2] = COND_STUDENT_STAND_ONE;
	element_idx[3] = COND_BOARD_NOTWRITE;
	truth_table_add_combination_condition(COND_COMBINATION_7, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_CHG;
	element_idx[1] = COND_TEACHER_DETECTED_NONE;
	element_idx[2] = COND_STUDENT_STAND_ONE;
	element_idx[3] = COND_BOARD_NOTWRITE;
	truth_table_add_combination_condition(COND_COMBINATION_8, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_UNCHG;
	element_idx[1] = COND_TEACHER_DETECTED_ONE;
	element_idx[2] = COND_STUDENT_STAND_ONE;
	element_idx[3] = COND_BOARD_NOTWRITE;
	truth_table_add_combination_condition(COND_COMBINATION_9, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_CHG;
	element_idx[1] = COND_TEACHER_DETECTED_ONE;
	element_idx[2] = COND_STUDENT_STAND_ONE;
	element_idx[3] = COND_BOARD_NOTWRITE;
	truth_table_add_combination_condition(COND_COMBINATION_10, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_UNCHG;
	element_idx[1] = COND_TEACHER_DETECTED_MULTIPLE;
	element_idx[2] = COND_STUDENT_STAND_ONE;
	element_idx[3] = COND_BOARD_NOTWRITE;
	truth_table_add_combination_condition(COND_COMBINATION_11, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_CHG;
	element_idx[1] = COND_TEACHER_DETECTED_MULTIPLE;
	element_idx[2] = COND_STUDENT_STAND_ONE;
	element_idx[3] = COND_BOARD_NOTWRITE;
	truth_table_add_combination_condition(COND_COMBINATION_12, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_UNCHG;
	element_idx[1] = COND_TEACHER_DETECTED_NONE;
	element_idx[2] = COND_STUDENT_STAND_MULTIPLE;
	element_idx[3] = COND_BOARD_NOTWRITE;
	truth_table_add_combination_condition(COND_COMBINATION_13, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_CHG;
	element_idx[1] = COND_TEACHER_DETECTED_NONE;
	element_idx[2] = COND_STUDENT_STAND_MULTIPLE;
	element_idx[3] = COND_BOARD_NOTWRITE;
	truth_table_add_combination_condition(COND_COMBINATION_14, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_UNCHG;
	element_idx[1] = COND_TEACHER_DETECTED_ONE;
	element_idx[2] = COND_STUDENT_STAND_MULTIPLE;
	element_idx[3] = COND_BOARD_NOTWRITE;
	truth_table_add_combination_condition(COND_COMBINATION_15, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_CHG;
	element_idx[1] = COND_TEACHER_DETECTED_ONE;
	element_idx[2] = COND_STUDENT_STAND_MULTIPLE;
	element_idx[3] = COND_BOARD_NOTWRITE;
	truth_table_add_combination_condition(COND_COMBINATION_16, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_UNCHG;
	element_idx[1] = COND_TEACHER_DETECTED_MULTIPLE;
	element_idx[2] = COND_STUDENT_STAND_MULTIPLE;
	element_idx[3] = COND_BOARD_NOTWRITE;
	truth_table_add_combination_condition(COND_COMBINATION_17, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_CHG;
	element_idx[1] = COND_TEACHER_DETECTED_MULTIPLE;
	element_idx[2] = COND_STUDENT_STAND_MULTIPLE;
	element_idx[3] = COND_BOARD_NOTWRITE;
	truth_table_add_combination_condition(COND_COMBINATION_18, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_UNCHG;
	element_idx[1] = COND_TEACHER_DETECTED_NONE;
	element_idx[2] = COND_STUDENT_STAND_NONE;
	element_idx[3] = COND_BOARD_WRITE;
	truth_table_add_combination_condition(COND_COMBINATION_19, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_CHG;
	element_idx[1] = COND_TEACHER_DETECTED_NONE;
	element_idx[2] = COND_STUDENT_STAND_NONE;
	element_idx[3] = COND_BOARD_WRITE;
	truth_table_add_combination_condition(COND_COMBINATION_20, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_UNCHG;
	element_idx[1] = COND_TEACHER_DETECTED_ONE;
	element_idx[2] = COND_STUDENT_STAND_NONE;
	element_idx[3] = COND_BOARD_WRITE;
	truth_table_add_combination_condition(COND_COMBINATION_21, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_CHG;
	element_idx[1] = COND_TEACHER_DETECTED_ONE;
	element_idx[2] = COND_STUDENT_STAND_NONE;
	element_idx[3] = COND_BOARD_WRITE;
	truth_table_add_combination_condition(COND_COMBINATION_22, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_UNCHG;
	element_idx[1] = COND_TEACHER_DETECTED_MULTIPLE;
	element_idx[2] = COND_STUDENT_STAND_NONE;
	element_idx[3] = COND_BOARD_WRITE;
	truth_table_add_combination_condition(COND_COMBINATION_23, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_CHG;
	element_idx[1] = COND_TEACHER_DETECTED_MULTIPLE;
	element_idx[2] = COND_STUDENT_STAND_NONE;
	element_idx[3] = COND_BOARD_WRITE;
	truth_table_add_combination_condition(COND_COMBINATION_24, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_UNCHG;
	element_idx[1] = COND_TEACHER_DETECTED_NONE;
	element_idx[2] = COND_STUDENT_STAND_ONE;
	element_idx[3] = COND_BOARD_WRITE;
	truth_table_add_combination_condition(COND_COMBINATION_25, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_CHG;
	element_idx[1] = COND_TEACHER_DETECTED_NONE;
	element_idx[2] = COND_STUDENT_STAND_ONE;
	element_idx[3] = COND_BOARD_WRITE;
	truth_table_add_combination_condition(COND_COMBINATION_26, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_UNCHG;
	element_idx[1] = COND_TEACHER_DETECTED_ONE;
	element_idx[2] = COND_STUDENT_STAND_ONE;
	element_idx[3] = COND_BOARD_WRITE;
	truth_table_add_combination_condition(COND_COMBINATION_27, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_CHG;
	element_idx[1] = COND_TEACHER_DETECTED_ONE;
	element_idx[2] = COND_STUDENT_STAND_ONE;
	element_idx[3] = COND_BOARD_WRITE;
	truth_table_add_combination_condition(COND_COMBINATION_28, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_UNCHG;
	element_idx[1] = COND_TEACHER_DETECTED_MULTIPLE;
	element_idx[2] = COND_STUDENT_STAND_ONE;
	element_idx[3] = COND_BOARD_WRITE;
	truth_table_add_combination_condition(COND_COMBINATION_29, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_CHG;
	element_idx[1] = COND_TEACHER_DETECTED_MULTIPLE;
	element_idx[2] = COND_STUDENT_STAND_ONE;
	element_idx[3] = COND_BOARD_WRITE;
	truth_table_add_combination_condition(COND_COMBINATION_30, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_UNCHG;
	element_idx[1] = COND_TEACHER_DETECTED_NONE;
	element_idx[2] = COND_STUDENT_STAND_MULTIPLE;
	element_idx[3] = COND_BOARD_WRITE;
	truth_table_add_combination_condition(COND_COMBINATION_31, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_CHG;
	element_idx[1] = COND_TEACHER_DETECTED_NONE;
	element_idx[2] = COND_STUDENT_STAND_MULTIPLE;
	element_idx[3] = COND_BOARD_WRITE;
	truth_table_add_combination_condition(COND_COMBINATION_32, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_UNCHG;
	element_idx[1] = COND_TEACHER_DETECTED_ONE;
	element_idx[2] = COND_STUDENT_STAND_MULTIPLE;
	element_idx[3] = COND_BOARD_WRITE;
	truth_table_add_combination_condition(COND_COMBINATION_33, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_CHG;
	element_idx[1] = COND_TEACHER_DETECTED_ONE;
	element_idx[2] = COND_STUDENT_STAND_MULTIPLE;
	element_idx[3] = COND_BOARD_WRITE;
	truth_table_add_combination_condition(COND_COMBINATION_34, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_UNCHG;
	element_idx[1] = COND_TEACHER_DETECTED_MULTIPLE;
	element_idx[2] = COND_STUDENT_STAND_MULTIPLE;
	element_idx[3] = COND_BOARD_WRITE;
	truth_table_add_combination_condition(COND_COMBINATION_35, 4, element_idx, &truth_table);

	element_idx[0] = COND_VGA_CHG;
	element_idx[1] = COND_TEACHER_DETECTED_MULTIPLE;
	element_idx[2] = COND_STUDENT_STAND_MULTIPLE;
	element_idx[3] = COND_BOARD_WRITE;
	truth_table_add_combination_condition(COND_COMBINATION_36, 4, element_idx, &truth_table);

	truth_table_element_condition_set(COND_VGA_UNCHG, &truth_table);
	truth_table_element_condition_reset(COND_VGA_CHG, &truth_table);
	truth_table_element_condition_set(COND_TEACHER_DETECTED_NONE, &truth_table);
	truth_table_element_condition_reset(COND_TEACHER_DETECTED_ONE, &truth_table);
	truth_table_element_condition_reset(COND_TEACHER_DETECTED_MULTIPLE, &truth_table);
	truth_table_element_condition_set(COND_STUDENT_STAND_NONE, &truth_table);
	truth_table_element_condition_reset(COND_STUDENT_STAND_ONE, &truth_table);
	truth_table_element_condition_reset(COND_STUDENT_STAND_MULTIPLE, &truth_table);
	truth_table_element_condition_set(COND_BOARD_NOTWRITE, &truth_table);
	truth_table_element_condition_reset(COND_BOARD_WRITE, &truth_table);

	t_transition_table transition_table;
	transition_table_init(&transition_table);

	t_finite_state_machine fsm;
	fsm_init(&fsm, &truth_table, &transition_table);
	switch_policy_load(POLICY_FILE_NAME, &fsm);
	gettimeofday(&timeval1, NULL);

	while(1){

		//check for exit
		if(thread_param->thread_canceled){
			break;
		}

		//teacher initialize and timeout checking
#if 1
		if(!get_camholder_pending_state(&teacher)){
			if(thread_param->teacher_status == 0){
				//recall preset 0
				Recall(thread_param, &teacher, 0);
				//debug_printf("teacher recall 0\n");
				thread_param->teacher_status = 1;
			}
		}

		if(!get_camholder_pending_state(&teacher)){
			if(thread_param->teacher_status == 1){
				//inquire zoom position
				GetZoom(thread_param, &teacher);
				//debug_printf("teacher get zoom\n");
				thread_param->teacher_status = 2;
			}
		}

		if(!get_camholder_pending_state(&teacher)){
			if(thread_param->teacher_status == 2){
				//inquire pan tilt position
				GetPos(thread_param, &teacher);
				//debug_printf("teacher get position\n");
				thread_param->teacher_status = 3;
			}
		}

		if(!get_camholder_pending_state(&teacher)){
			if(thread_param->teacher_status == 3){
				//save preset 0 zoom
				save_camholder_preset(&teacher, 0);
				//recall preset 1
				Recall(thread_param, &teacher, 1);

				thread_param->teacher_status = 4;
			}
		}

		if(!get_camholder_pending_state(&teacher)){
			if(thread_param->teacher_status == 4){
				//inquire pan tilt position
				GetPos(thread_param, &teacher);
				thread_param->teacher_status = 5;
			}
		}

		if(!get_camholder_pending_state(&teacher)){
			if(thread_param->teacher_status == 5){
				//save preset 1 PTZ (Z use preset 0)
				save_camholder_preset(&teacher, 1);
				//recall preset 2
				Recall(thread_param, &teacher, 2);
				thread_param->teacher_status = 6;
			}
		}

		if(!get_camholder_pending_state(&teacher)){
			if(thread_param->teacher_status == 6){
				//inquire pan tilt position
				GetPos(thread_param, &teacher);
				thread_param->teacher_status = 7;
			}
		}

		if(!get_camholder_pending_state(&teacher)){
			if(thread_param->teacher_status == 7){
				//save preset 2 PTZ (Z use preset 0)
				save_camholder_preset(&teacher, 2);
				//recall preset 3
				Recall(thread_param, &teacher, 3);
				thread_param->teacher_status = 8;
			}

		}

		if(!get_camholder_pending_state(&teacher)){
			if(thread_param->teacher_status == 8){
				//inquire pan tilt position
				GetPos(thread_param, &teacher);
				thread_param->teacher_status = 9;
			}
		}

		if(!get_camholder_pending_state(&teacher)){
			if(thread_param->teacher_status == 9){
				//save preset 3 PTZ (Z use preset 0)
				save_camholder_preset(&teacher, 3);
				//recall preset 4
				Recall(thread_param, &teacher, 4);
				thread_param->teacher_status = 10;
			}
		}

		if(!get_camholder_pending_state(&teacher)){
			if(thread_param->teacher_status == 10){
				//inquire pan tilt position
				GetPos(thread_param, &teacher);
				thread_param->teacher_status = 11;
			}
		}

		if(!get_camholder_pending_state(&teacher)){
			if(thread_param->teacher_status == 11){
				//save preset 4 PTZ (Z use preset 0)
				save_camholder_preset(&teacher, 4);
				//recall 5
				Recall(thread_param, &teacher, 5);
				thread_param->teacher_status = 12;
			}
		}

		if(!get_camholder_pending_state(&teacher)){
			if(thread_param->teacher_status == 12){
				//inquire pan tilt position
				GetPos(thread_param, &teacher);
				thread_param->teacher_status = 13;
			}
		}

		if(!get_camholder_pending_state(&teacher)){
			if(thread_param->teacher_status == 13){
				//save preset 5 PTZ (Z use preset 0)
				save_camholder_preset(&teacher, 5);
				//recall 6
				Recall(thread_param, &teacher, 6);
				thread_param->teacher_status = 14;
			}
		}

		if(!get_camholder_pending_state(&teacher)){
			if(thread_param->teacher_status == 14){
				//inquire pan tilt position
				GetPos(thread_param, &teacher);
				thread_param->teacher_status = 15;
			}
		}

		if(!get_camholder_pending_state(&teacher)){
			if(thread_param->teacher_status == 15){
				//save preset 6 PTZ (Z use preset 0)
				save_camholder_preset(&teacher, 6);
				//recall 0
				Recall(thread_param, &teacher, 0);
				thread_param->teacher_status = 16;
			}
		}

		if(!get_camholder_pending_state(&teacher)){
			if(thread_param->teacher_status == 16){
				if(teacher.preset[1].pan_pos < teacher.preset[2].pan_pos){
					limit_left = teacher.preset[1].pan_pos;
					limit_right = teacher.preset[2].pan_pos;
				}
				else{
					limit_right = teacher.preset[1].pan_pos;
					limit_left = teacher.preset[2].pan_pos;
				}
				if(teacher.preset[5].pan_pos < teacher.preset[6].pan_pos){
					exclude_left = teacher.preset[5].pan_pos;
					exclude_right = teacher.preset[6].pan_pos;
				}
				else{
					exclude_right = teacher.preset[5].pan_pos;
					exclude_left = teacher.preset[6].pan_pos;
				}
				thread_param->teacher_status = 17;
				debug_printf("Teacher camera holder initialized\n");
			}
		}

		if(get_camholder_pending_state(&teacher)){
			//error handling
			if(get_camholder_err(&teacher)){
				camholder_reset_pending_state(&teacher);
				if(thread_param->teacher_status > 0 && thread_param->teacher_status < 17){
					thread_param->teacher_status--;
				}
			}

			//timeout handling
			if(camholder_command_timeout(&teacher, 10)){
				camholder_reset_pending_state(&teacher);
				if(thread_param->teacher_status > 0 && thread_param->teacher_status < 17){
					thread_param->teacher_status--;
				}
			}
		}
#endif
		//student initialize and timeout checing
		if(!get_camholder_pending_state(&student)){
			if(thread_param->student_status == 0){
				//recall preset 0
				Recall(thread_param, &student, 0);
				//debug_printf("student recall 0\n");
				thread_param->student_status = 1;
			}
		}

		if(!get_camholder_pending_state(&student)){
			if(thread_param->student_status == 1){
				//inquire zoom position
				GetZoom(thread_param, &student);

				thread_param->student_status = 2;
			}
		}

		if(!get_camholder_pending_state(&student)){
			if(thread_param->student_status == 2){
				//inquire pan tilt position
				GetPos(thread_param, &student);

				thread_param->student_status = 3;
			}
		}

		if(!get_camholder_pending_state(&student)){
			if(thread_param->student_status == 3){
				save_camholder_preset(&student, 0);
				Recall(thread_param, &student, 0);
				thread_param->student_status = 4;
			}
		}

		if(!get_camholder_pending_state(&student)){
			if(thread_param->student_status == 4){
				thread_param->student_status = 5;
				debug_printf("Student camera holder initialized\n");
			}
		}

		if(get_camholder_pending_state(&student)){
			//error handling
			if(get_camholder_err(&student)){
				camholder_reset_pending_state(&student);
				if(thread_param->student_status > 0 && thread_param->student_status < 5){
					thread_param->student_status--;
				}
			}

			//timeout handling
			if(camholder_command_timeout(&student, 10)){
				camholder_reset_pending_state(&student);
				if(thread_param->student_status > 0 && thread_param->student_status < 5){
					thread_param->student_status--;
				}
			}
		}

		//check for uart response
		while(datafifo_head(thread_param->RX_consumer_fifo_A1)){
			if(teacher.uart_channel == 1){
				debug_printf("%02x ", datafifo_head(thread_param->RX_consumer_fifo_A1)->arg.data[0]);

				//save one byte to the RspBuf at a time and remove it from fifo
				*teacher.Rx_buf_p = datafifo_head(thread_param->RX_consumer_fifo_A1)->arg.data[0];
				datafifo_consumer_remove_head(thread_param->RX_consumer_fifo_A1);
				teacher.Rx_buf_p++;
				//handle the response when it ends with 0xff
				if(teacher.Rx_buf[teacher.Rx_buf_p - teacher.Rx_buf - 1]==(char)0xff){
					debug_printf("\n");
					debug_printf("teacher uart response received\n");
					//fflush(stdout);
					teacher.RspLen = teacher.Rx_buf_p - teacher.Rx_buf;
					teacher.Rx_buf_p = teacher.Rx_buf;

					update_camholder_ctx(&teacher);

					break;
				}
			}
			else if(student.uart_channel == 1){
				debug_printf("%02x", datafifo_head(thread_param->RX_consumer_fifo_A1)->arg.data[0]);

				//save one byte to the RspBuf at a time and remove it from fifo
				*student.Rx_buf_p = datafifo_head(thread_param->RX_consumer_fifo_A1)->arg.data[0];
				datafifo_consumer_remove_head(thread_param->RX_consumer_fifo_A1);
				student.Rx_buf_p++;
				//handle the response when it ends with 0xff
				if(student.Rx_buf[student.Rx_buf_p - student.Rx_buf - 1]==(char)0xff){
					debug_printf("\n");
					debug_printf("teacher uart response received\n");
					//fflush(stdout);
					student.RspLen = student.Rx_buf_p - student.Rx_buf;
					student.Rx_buf_p = student.Rx_buf;

					update_camholder_ctx(&student);

					break;
				}
			}
			else{
				datafifo_consumer_remove_head(thread_param->RX_consumer_fifo_A1);
			}
		}

		while(datafifo_head(thread_param->RX_consumer_fifo_A2)){
			datafifo_consumer_remove_head(thread_param->RX_consumer_fifo_A2);
		}

		while(datafifo_head(thread_param->RX_consumer_fifo_B1)){
			if(teacher.uart_channel == 3){
				debug_printf("%02x", datafifo_head(thread_param->RX_consumer_fifo_B1)->arg.data[0]);

				//save one byte to the RspBuf at a time and remove it from fifo
				*teacher.Rx_buf_p = datafifo_head(thread_param->RX_consumer_fifo_B1)->arg.data[0];
				datafifo_consumer_remove_head(thread_param->RX_consumer_fifo_B1);
				teacher.Rx_buf_p++;
				//handle the response when it ends with 0xff
				if(teacher.Rx_buf[teacher.Rx_buf_p - teacher.Rx_buf - 1]==(char)0xff){
					debug_printf("\n");
					debug_printf("teacher uart response received\n");
					//fflush(stdout);
					teacher.RspLen = teacher.Rx_buf_p - teacher.Rx_buf;
					teacher.Rx_buf_p = teacher.Rx_buf;

					update_camholder_ctx(&teacher);

					break;
				}
			}
			else if(student.uart_channel == 3){
				debug_printf("%02x", datafifo_head(thread_param->RX_consumer_fifo_B1)->arg.data[0]);

				//save one byte to the RspBuf at a time and remove it from fifo
				*student.Rx_buf_p = datafifo_head(thread_param->RX_consumer_fifo_B1)->arg.data[0];
				datafifo_consumer_remove_head(thread_param->RX_consumer_fifo_B1);
				student.Rx_buf_p++;
				//handle the response when it ends with 0xff
				if(student.Rx_buf[student.Rx_buf_p - student.Rx_buf - 1]==(char)0xff){
					debug_printf("\n");
					debug_printf("teacher uart response received\n");
					//fflush(stdout);
					student.RspLen = student.Rx_buf_p - student.Rx_buf;
					student.Rx_buf_p = student.Rx_buf;

					update_camholder_ctx(&student);

					break;
				}
			}
			else{
				datafifo_consumer_remove_head(thread_param->RX_consumer_fifo_B1);
			}
		}

		while(datafifo_head(thread_param->RX_consumer_fifo_B2)){
			datafifo_consumer_remove_head(thread_param->RX_consumer_fifo_B2);
		}

		while(datafifo_head(thread_param->RX_consumer_fifo_C1)){
			datafifo_consumer_remove_head(thread_param->RX_consumer_fifo_C1);
		}

		while(datafifo_head(thread_param->RX_consumer_fifo_C2)){
			datafifo_consumer_remove_head(thread_param->RX_consumer_fifo_C2);
		}

		//check whether camera holder is still
		if(!get_camholder_pending_state(&teacher)){
			if(!g_camholder_still){
				pthread_mutex_lock(&g_capture_sync_mutex);
				g_camholder_still = 1;
				pthread_mutex_unlock(&g_capture_sync_mutex);
			}
		}
		//check for control command
		while(datafifo_head(thread_param->Ctrl_consumer_fifo)){
 			debug_printf("thread_tracking receive message\n");

 			char *Msg = (char *)datafifo_head(thread_param->Ctrl_consumer_fifo)->arg.data;
 			char cmd[CMD_SIZE+1];
 			unsigned int len;
 			char *ptr;
 			//int state_flag;

 			memcpy(cmd, &Msg[VERSION_SIZE], CMD_SIZE);
 			cmd[CMD_SIZE] = '\0';

            char2hex(&Msg[VERSION_SIZE+CMD_SIZE], &len, DATA_LEN_SIZE);
            //debug_printf("Total length of attributes: %d(%x)\n", len, len);

            ptr = &Msg[PACKET_HEADER_SIZE];

            if(!strcmp(cmd,COMMAND_INTERNAL)){
            	while(len){
            		unsigned int name_len, value_len;
            		char attr_name[16];
            		char attr_value[1536];

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

            		if(!strcmp(attr_name, "Start")){
            			debug_printf("SwitchTracking receive start\n");
            			acc_of_lost = 3;
            			motion_in_screen = 0;
            			still_in_screen = 0;
            			in_left = 0;
            			in_right = 0;
            			//motion_of_lost = 0;
            			acc_student_lost = 0;
            			//recall preset 0
            			if(thread_param->teacher_status == 17){
            				Recall(thread_param, &teacher, 0);
            			}

            			//reset truth table
            			truth_table_element_condition_set(COND_VGA_UNCHG, &truth_table);
            			truth_table_element_condition_reset(COND_VGA_CHG, &truth_table);
            			truth_table_element_condition_set(COND_TEACHER_DETECTED_NONE, &truth_table);
            			truth_table_element_condition_reset(COND_TEACHER_DETECTED_ONE, &truth_table);
            			truth_table_element_condition_reset(COND_TEACHER_DETECTED_MULTIPLE, &truth_table);
            			truth_table_element_condition_set(COND_STUDENT_STAND_NONE, &truth_table);
            			truth_table_element_condition_reset(COND_STUDENT_STAND_ONE, &truth_table);
            			truth_table_element_condition_reset(COND_STUDENT_STAND_MULTIPLE, &truth_table);
            			truth_table_element_condition_set(COND_BOARD_NOTWRITE, &truth_table);
            			truth_table_element_condition_reset(COND_BOARD_WRITE, &truth_table);

            			fsm_set_state(&fsm, fsm.initial_state);
            			int ret = fsm_get_state(&fsm);
            			if(ret == FSM_VGA){
            				size_t size;
            				t_cmd outmsg;
            				fill_cmd_header(&outmsg, COMMAND_SWITCHCTRL);
            				add_attr_int(&outmsg, "CaptureId", VIDEO_FUNCTION_VGA); //VGA's function id and capture id are identical
            				add_attr_int(&outmsg, "PIP", 0); //0:normal

            				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
            				size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
            				int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
            				datafifo_producer_data_add(thread_param->Switch_producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
            				vmap_table_reset_pip();
            			}

            			if(ret == FSM_TEACHER_CLOSEUP){
            				size_t size;
            				t_cmd outmsg;
            				fill_cmd_header(&outmsg, COMMAND_SWITCHCTRL);
            				int cap_id = vmap_table_get_capid_by_function(VIDEO_FUNCTION_TEACHER_CLOSEUP);
            				add_attr_int(&outmsg, "CaptureId", cap_id);
            				add_attr_int(&outmsg, "PIP", 0); //0:normal

            				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
            				size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
            				int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
            				datafifo_producer_data_add(thread_param->Switch_producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
            				vmap_table_reset_pip();
            			}

            			if(ret == FSM_STUDENT_CLOSEUP){
            				size_t size;
            				t_cmd outmsg;
            				fill_cmd_header(&outmsg, COMMAND_SWITCHCTRL);
            				int cap_id = vmap_table_get_capid_by_function(VIDEO_FUNCTION_STUDENT_CLOSEUP);
            				add_attr_int(&outmsg, "CaptureId", cap_id);
            				add_attr_int(&outmsg, "PIP", 0); //0:normal

            				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
            				size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
            				int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
            				datafifo_producer_data_add(thread_param->Switch_producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
            				vmap_table_reset_pip();
            			}

            			if(ret == FSM_TEACHER_PANORAMA){
            				size_t size;
            				t_cmd outmsg;
            				fill_cmd_header(&outmsg, COMMAND_SWITCHCTRL);
            				int cap_id = vmap_table_get_capid_by_function(VIDEO_FUNCTION_TEACHER_PANORAMA);
            				add_attr_int(&outmsg, "CaptureId", cap_id);
            				add_attr_int(&outmsg, "PIP", 0); //0:normal

            				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
            				size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
            				int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
            				datafifo_producer_data_add(thread_param->Switch_producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
            				vmap_table_reset_pip();
            			}

            			if(ret == FSM_STUDENT_PANORAMA){
            				size_t size;
            				t_cmd outmsg;
            				fill_cmd_header(&outmsg, COMMAND_SWITCHCTRL);
            				int cap_id = vmap_table_get_capid_by_function(VIDEO_FUNCTION_STUDENT_PANORAMA);
            				add_attr_int(&outmsg, "CaptureId", cap_id);
            				add_attr_int(&outmsg, "PIP", 0); //0:normal

            				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
            				size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
            				int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
            				datafifo_producer_data_add(thread_param->Switch_producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
            				vmap_table_reset_pip();
            			}

            			if(ret == FSM_BOARD_CLOSEUP){
            				size_t size;
            				t_cmd outmsg;
            				fill_cmd_header(&outmsg, COMMAND_SWITCHCTRL);
            				int cap_id = vmap_table_get_capid_by_function(VIDEO_FUNCTION_BOARD_CLOSEUP);
            				add_attr_int(&outmsg, "CaptureId", cap_id);
            				add_attr_int(&outmsg, "PIP", 0); //0:normal

            				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
            				size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
            				int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
            				datafifo_producer_data_add(thread_param->Switch_producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
            				vmap_table_reset_pip();
            			}
            			gettimeofday(&timeval1, NULL);
            		}

            		if(!strcmp(attr_name, "Stop")){
            			debug_printf("SwitchTracking receive stop\n");
            			//recall preset 0
            			if(thread_param->teacher_status == 17){
            				Recall(thread_param, &teacher, 0);
            			}
            			vmap_table_reset_pip();
            		}

            		if(!strcmp(attr_name, "SwitchVideo")){
            			if(!strcmp(attr_value, "VGA")){
            				size_t size;
            				t_cmd outmsg;
            				fill_cmd_header(&outmsg, COMMAND_SWITCHCTRL);
            				add_attr_int(&outmsg, "CaptureId", VIDEO_FUNCTION_VGA); //VGA's function id and capture id are identical
            				add_attr_int(&outmsg, "PIP", 0); //0:normal

            				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
            				size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
            				int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
            				datafifo_producer_data_add(thread_param->Switch_producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
            				vmap_table_reset_pip();
            			}

            			if(!strcmp(attr_value, "TeacherSpecial")){
            				size_t size;
            				t_cmd outmsg;
            				fill_cmd_header(&outmsg, COMMAND_SWITCHCTRL);
            				int cap_id = vmap_table_get_capid_by_function(VIDEO_FUNCTION_TEACHER_CLOSEUP);
            				add_attr_int(&outmsg, "CaptureId", cap_id);
            				add_attr_int(&outmsg, "PIP", 0); //0:normal

            				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
            				size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
            				int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
            				datafifo_producer_data_add(thread_param->Switch_producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
            				vmap_table_reset_pip();
            			}

            			if(!strcmp(attr_value, "StudentSpecial")){
            				size_t size;
            				t_cmd outmsg;
            				fill_cmd_header(&outmsg, COMMAND_SWITCHCTRL);
            				int cap_id = vmap_table_get_capid_by_function(VIDEO_FUNCTION_STUDENT_CLOSEUP);
            				add_attr_int(&outmsg, "CaptureId", cap_id);
            				add_attr_int(&outmsg, "PIP", 0); //0:normal

            				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
            				size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
            				int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
            				datafifo_producer_data_add(thread_param->Switch_producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
            				vmap_table_reset_pip();
            			}

            			if(!strcmp(attr_value, "TeacherFull")){
            				size_t size;
            				t_cmd outmsg;
            				fill_cmd_header(&outmsg, COMMAND_SWITCHCTRL);
            				int cap_id = vmap_table_get_capid_by_function(VIDEO_FUNCTION_TEACHER_PANORAMA);
            				add_attr_int(&outmsg, "CaptureId", cap_id);
            				add_attr_int(&outmsg, "PIP", 0); //0:normal

            				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
            				size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
            				int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
            				datafifo_producer_data_add(thread_param->Switch_producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
            				vmap_table_reset_pip();
            			}

            			if(!strcmp(attr_value, "StudentFull")){
            				size_t size;
            				t_cmd outmsg;
            				fill_cmd_header(&outmsg, COMMAND_SWITCHCTRL);
            				int cap_id = vmap_table_get_capid_by_function(VIDEO_FUNCTION_STUDENT_PANORAMA);
            				add_attr_int(&outmsg, "CaptureId", cap_id);
            				add_attr_int(&outmsg, "PIP", 0); //0:normal

            				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
            				size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
            				int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
            				datafifo_producer_data_add(thread_param->Switch_producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
            				vmap_table_reset_pip();
            			}

            			if(!strcmp(attr_value, "Blackboard")){
            				size_t size;
            				t_cmd outmsg;
            				fill_cmd_header(&outmsg, COMMAND_SWITCHCTRL);
            				int cap_id = vmap_table_get_capid_by_function(VIDEO_FUNCTION_BOARD_CLOSEUP);
            				add_attr_int(&outmsg, "CaptureId", cap_id);
            				add_attr_int(&outmsg, "PIP", 0); //0:normal

            				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
            				size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
            				int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
            				datafifo_producer_data_add(thread_param->Switch_producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
            				vmap_table_reset_pip();
            			}

            			if(!strcmp(attr_value, "VGAWithTeacherSpecial")){
            				size_t size;
            				t_cmd outmsg;
            				fill_cmd_header(&outmsg, COMMAND_SWITCHCTRL);
            				add_attr_int(&outmsg, "CaptureId", VIDEO_FUNCTION_VGA);
            				add_attr_int(&outmsg, "PIP", 1); //0:normal

            				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
            				size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
            				int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
            				datafifo_producer_data_add(thread_param->Switch_producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
            				vmap_table_set_pip_by_function(VIDEO_FUNCTION_TEACHER_CLOSEUP);
            			}

            			if(!strcmp(attr_value, "TeacherSpecialWithStudentSpecial")){
            				size_t size;
            				t_cmd outmsg;
            				fill_cmd_header(&outmsg, COMMAND_SWITCHCTRL);
            				int cap_id = vmap_table_get_capid_by_function(VIDEO_FUNCTION_TEACHER_CLOSEUP);
            				add_attr_int(&outmsg, "CaptureId", cap_id);
            				add_attr_int(&outmsg, "PIP", 1); //0:normal

            				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
            				size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
            				int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
            				datafifo_producer_data_add(thread_param->Switch_producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
            				vmap_table_set_pip_by_function(VIDEO_FUNCTION_STUDENT_CLOSEUP);
            			}
            		}
            		if(!strcmp(attr_name, "SetRecordMode")){
            			if(!strcmp(attr_value, "Manual")){
            				g_cfg.IsAutoInstuct = 0;
            				debug_printf("set mode to manual\n");
            			}

            			if(!strcmp(attr_value, "HalfAuto")){
            				g_cfg.IsAutoInstuct = 1;
            				debug_printf("set mode to semi-auto\n");
            			}

            			if(!strcmp(attr_value, "Auto")){
            				g_cfg.IsAutoInstuct = 2;
            				debug_printf("set mode to auto\n");
            			}
            		}

            		if(!strcmp(attr_name, "CZWT")){
            			int zoomstep = (teacher.zoom_max-teacher.zoom_min)/100;
            			int dest_zoom = teacher.ZoomPos-zoomstep;
            			if(dest_zoom<teacher.zoom_min){
            				dest_zoom = teacher.zoom_min;
            			}
            			ZoomDirect(thread_param, &teacher, dest_zoom);
            			debug_printf("CZWT\n");
            		}

            		if(!strcmp(attr_name, "CZTT")){
            			int zoomstep = (teacher.zoom_max-teacher.zoom_min)/100;
            			int dest_zoom = teacher.ZoomPos+zoomstep;
            			if(dest_zoom>teacher.zoom_max){
            				dest_zoom = teacher.zoom_max;
            			}
            			ZoomDirect(thread_param, &teacher, dest_zoom);
            			debug_printf("CZTT\n");
            		}

            		if(!strcmp(attr_name, "CZWS")){
            			int zoomstep = (student.zoom_max-student.zoom_min)/100;
            			int dest_zoom = student.ZoomPos-zoomstep;
            			if(dest_zoom<student.zoom_min){
            				dest_zoom = student.zoom_min;
            			}
            			ZoomDirect(thread_param, &student, dest_zoom);
            			debug_printf("CZWS\n");
            		}

            		if(!strcmp(attr_name, "CZTS")){
            			int zoomstep = (student.zoom_max-student.zoom_min)/100;
            			int dest_zoom = student.ZoomPos+zoomstep;
            			if(dest_zoom>student.zoom_max){
            				dest_zoom = student.zoom_max;
            			}
            			ZoomDirect(thread_param, &student, dest_zoom);
            			debug_printf("CZTS\n");
            		}

            		if(!strcmp(attr_name, "CPTLT")){
            			int panstep = teacher.pan_max/teacher.pan_angle_right;
            			//int tiltstep = teacher.tilt_max/teacher.tilt_angle_up;
            			int destpan = teacher.PanPos-panstep;
            			if(destpan<teacher.pan_min){
            				destpan = teacher.pan_min;
            			}
            			int desttilt = teacher.TiltPos;
            			AbsPos(thread_param, &teacher, destpan, desttilt);
            			debug_printf("CPTLT\n");
            		}


            		if(!strcmp(attr_name, "CPTRT")){
            			int panstep = teacher.pan_max/teacher.pan_angle_right;
            			//int tiltstep = teacher.tilt_max/teacher.tilt_angle_up;
            			int destpan = teacher.PanPos+panstep;
            			if(destpan>teacher.pan_max){
            				destpan = teacher.pan_max;
            			}
            			int desttilt = teacher.TiltPos;
            			AbsPos(thread_param, &teacher, destpan, desttilt);
            			debug_printf("CPTRT\n");
            		}


            		if(!strcmp(attr_name, "CPTUT")){
            			//int panstep = teacher.pan_max/teacher.pan_angle_right;
            			int tiltstep = teacher.tilt_max/teacher.tilt_angle_up;
            			int destpan = teacher.PanPos;
            			int desttilt = teacher.TiltPos+tiltstep;
            			if(desttilt>teacher.tilt_max){
            				desttilt = teacher.tilt_max;
            			}
            			AbsPos(thread_param, &teacher, destpan, desttilt);
            			debug_printf("CPTUT\n");
            		}


            		if(!strcmp(attr_name, "CPTDT")){
            			//int panstep = teacher.pan_max/teacher.pan_angle_right;
            			int tiltstep = teacher.tilt_max/teacher.tilt_angle_up;
            			int destpan = teacher.PanPos;
            			int desttilt = teacher.TiltPos-tiltstep;
            			if(desttilt<teacher.tilt_min){
            				desttilt = teacher.tilt_min;
            			}
            			AbsPos(thread_param, &teacher, destpan, desttilt);
            			debug_printf("CPTDT\n");
            		}


            		if(!strcmp(attr_name, "CPTLS")){
            			int panstep = student.pan_max/student.pan_angle_right;
            			//int tiltstep = student.tilt_max/student.tilt_angle_up;
            			int destpan = student.PanPos-panstep;
            			if(destpan<student.pan_min){
            				destpan = student.pan_min;
            			}
            			int desttilt = student.TiltPos;
            			AbsPos(thread_param, &student, destpan, desttilt);
            			debug_printf("CPTLS\n");
            		}


            		if(!strcmp(attr_name, "CPTRS")){
            			int panstep = student.pan_max/student.pan_angle_right;
            			//int tiltstep = student.tilt_max/student.tilt_angle_up;
            			int destpan = student.PanPos+panstep;
            			if(destpan>student.pan_max){
            				destpan = student.pan_max;
            			}
            			int desttilt = student.TiltPos;
            			AbsPos(thread_param, &student, destpan, desttilt);
            			debug_printf("CPTRS\n");
            		}


            		if(!strcmp(attr_name, "CPTUS")){
            			//int panstep = student.pan_max/student.pan_angle_right;
            			int tiltstep = student.tilt_max/student.tilt_angle_up;
            			int destpan = student.PanPos;
            			int desttilt = student.TiltPos+tiltstep;
            			if(desttilt>student.tilt_max){
            				desttilt = student.tilt_max;
            			}
            			AbsPos(thread_param, &student, destpan, desttilt);
            			debug_printf("CPTUS\n");
            		}


            		if(!strcmp(attr_name, "CPTDS")){
            			//int panstep = student.pan_max/student.pan_angle_right;
            			int tiltstep = student.tilt_max/student.tilt_angle_up;
            			int destpan = student.PanPos;
            			int desttilt = student.TiltPos-tiltstep;
            			if(desttilt<student.tilt_min){
            				desttilt = student.tilt_min;
            			}
            			AbsPos(thread_param, &student, destpan, desttilt);
            			debug_printf("CPTDS\n");
            		}

            		if(!strcmp(attr_name, "CMIT")){
            			unsigned int number;
            			char2hex(attr_value, &number, value_len);
            			Recall(thread_param, &teacher, number);
            			debug_printf("CMIT\n");
            		}

            		if(!strcmp(attr_name, "CMST")){
            			unsigned int number;
            			char2hex(attr_value, &number, value_len);
            			MemSet(thread_param, &teacher, number);
            			debug_printf("CMST\n");
            		}

            		if(!strcmp(attr_name, "CMIS")){
            			unsigned int number;
            			char2hex(attr_value, &number, value_len);
            			Recall(thread_param, &student, number);
            			debug_printf("CMIS\n");
            		}

            		if(!strcmp(attr_name, "CMSS")){
            			unsigned int number;
            			char2hex(attr_value, &number, value_len);
            			MemSet(thread_param, &student, number);
            			debug_printf("CMSS\n");
            		}
            	}
            }

			datafifo_consumer_remove_head(thread_param->Ctrl_consumer_fifo);
		}



		//check for new event
		while(datafifo_head(thread_param->Event_consumer_fifo)){
#if 0	//only for test purpose
			unsigned char *Msg = datafifo_head(thread_param->Event_consumer_fifo)->arg.data;
			char cmd[CMD_SIZE+1];
			memcpy(cmd, &Msg[VERSION_SIZE], CMD_SIZE);
			cmd[CMD_SIZE] = '\0';
			if(!strcmp(cmd, COMMAND_EVENT_VID)){
				t_DetectResult DetectResult;
				GetDetectResult(&DetectResult, (char *)datafifo_head(thread_param->Event_consumer_fifo)->arg.data);
				if(DetectResult.detected){
					debug_printf("teacher detected\n");
				}
				else{
					debug_printf("teacher undetected\n");
				}
			}

			if(!strcmp(cmd, COMMAND_EVENT_VGA)){
				t_VGA_result result;
				GetVGAResult(&result, (char *)datafifo_head(thread_param->Event_consumer_fifo)->arg.data);
				if(result.changed){
					debug_printf("VGA changed\n");
				}
				else{
					debug_printf("VGA unchanged\n");
				}
			}
#endif


			//extract the command type
			unsigned char *Msg = datafifo_head(thread_param->Event_consumer_fifo)->arg.data;
			char cmd[CMD_SIZE+1];
			//unsigned int len;
			//char *ptr;
			//int flags;

			memcpy(cmd, &Msg[VERSION_SIZE], CMD_SIZE);
			cmd[CMD_SIZE] = '\0';

			if(g_cfg.IsAutoInstuct){
				//do teacher tracking
#define SCREEN_ON
#ifndef SCREEN_ON
				if(!strcmp(cmd, COMMAND_EVENT_VID)){
					if(thread_param->teacher_status == 17){
						if(!get_camholder_pending_state(&teacher)){
							if(motion_skip_num){
								t_DetectResult DetectResult;
								GetDetectResult(&DetectResult, (char *)datafifo_head(thread_param->Event_consumer_fifo)->arg.data);
								if(DetectResult.detected){
									acc_of_lost = 0;
									if(DetectResult.max_right - DetectResult.max_left >= 128){
										IsMultipleTeacher = 1;
									}
									else{
										IsMultipleTeacher = 0;
									}
									int offset = (int)(DetectResult.avg_left + DetectResult.avg_right -256)/2;
									debug_printf("left = %d, right = %d\n", DetectResult.avg_left, DetectResult.avg_right);
									if(offset>=45 || offset<=-45){
										//float view_angle = -(52*ZoomPos/16384)+55.2;
										float view_angle = -((teacher.view_angle_max - teacher.view_angle_min)*teacher.ZoomPos/teacher.zoom_max)+teacher.view_angle_max;
										float pan_angle = atan(tan(view_angle*M_PI/360)*offset/128);
										//int relative_pos = 1600*pan_angle*180/M_PI/120;
										int relative_pos = teacher.pan_max*pan_angle*180/M_PI/teacher.pan_angle_right;
										int destpan_pos = teacher.PanPos + relative_pos;
										debug_printf("offset = %d, view_angle = %f, pan_angle = %f\n", offset, view_angle, pan_angle*180/M_PI);
										debug_printf("relative_pos=%d, PanPos=%d\n", relative_pos, teacher.PanPos);

										if(!IsExclude){
											if(destpan_pos>limit_right){
												destpan_pos = limit_right;
											}

											if(destpan_pos<limit_left){
												destpan_pos = limit_left;
											}
										}
										else{
											if(teacher.PanPos>=exclude_right && teacher.PanPos<=limit_right){
												if(destpan_pos>limit_right){
													destpan_pos = limit_right;
												}

												if(destpan_pos<exclude_right){
													destpan_pos = exclude_right;
												}
											}
											if(teacher.PanPos>=limit_left && teacher.PanPos<=exclude_left){
												if(destpan_pos>exclude_left){
													destpan_pos = exclude_left;
												}

												if(destpan_pos<limit_left){
													destpan_pos = limit_left;
												}
											}
										}

										if(destpan_pos>teacher.PanPos){
											direction = 1;
										}
										else{
											direction = -1;
										}

										if(destpan_pos != teacher.PanPos){
											pthread_mutex_lock(&g_capture_sync_mutex);
											g_camholder_still = 0;
											g_analyze_frame_num = 0;
											g_num_in_group = 0;
											pthread_mutex_unlock(&g_capture_sync_mutex);
											AbsPos(thread_param, &teacher, destpan_pos, teacher.TiltPos);
											//motion_skip_num = 0;
										}
									}
								}
								else{
									acc_of_lost++;
									//scan when lost target
		#if 0
									if(acc_of_lost >= 18 && (acc_of_lost%6)==0){
										Recall(thread_param, &teacher, (acc_of_lost/6)%3);
									}
		#endif
									if(acc_of_lost >= 6 && (acc_of_lost%3)==0){
										int offset = 128*direction;
										float view_angle = -((teacher.view_angle_max - teacher.view_angle_min)*teacher.ZoomPos/teacher.zoom_max)+teacher.view_angle_max;
										float pan_angle = atan(tan(view_angle*M_PI/360)*offset/128);
										//int relative_pos = 1600*pan_angle*180/M_PI/120;
										int relative_pos = teacher.pan_max*pan_angle*180/M_PI/teacher.pan_angle_right;
										int destpan_pos = teacher.PanPos + relative_pos;

										if(!IsExclude){
											if(destpan_pos>limit_right){
												destpan_pos = limit_right;
												direction = direction*(-1);
											}

											if(destpan_pos<limit_left){
												destpan_pos = limit_left;
												direction = direction*(-1);
											}
										}
										else{
											if(teacher.PanPos>=exclude_right && teacher.PanPos<=limit_right){
												if(destpan_pos>limit_right){
													destpan_pos = limit_right;
													direction = direction*(-1);
												}

												if(destpan_pos<exclude_right){
													destpan_pos = exclude_right;
													if(destpan_pos == teacher.PanPos){
														destpan_pos = exclude_left;
													}
												}
											}
											if(teacher.PanPos>=limit_left && teacher.PanPos<=exclude_left){
												if(destpan_pos>exclude_left){
													destpan_pos = exclude_left;
													if(destpan_pos == teacher.PanPos){
														destpan_pos = exclude_right;
													}
												}

												if(destpan_pos<limit_left){
													destpan_pos = limit_left;
													direction = direction*(-1);
												}
											}
										}

										if(destpan_pos != teacher.PanPos){
											pthread_mutex_lock(&g_capture_sync_mutex);
											g_camholder_still = 0;
											g_analyze_frame_num = 0;
											g_num_in_group = 0;
											pthread_mutex_unlock(&g_capture_sync_mutex);
											AbsPos(thread_param, &teacher, destpan_pos, teacher.TiltPos);
											//motion_skip_num = 0;
										}
									}
								}
							}
							else{
								motion_skip_num++;
							}
						}
					}
				}
#else
				if(!strcmp(cmd, COMMAND_EVENT_VID)){
					if(thread_param->teacher_status == 17){
						if(!get_camholder_pending_state(&teacher)){

							t_DetectResult DetectResult;
							GetDetectResult(&DetectResult, (char *)datafifo_head(thread_param->Event_consumer_fifo)->arg.data);
							//make capture or lost decision
							if(DetectResult.detected){//detected
								if(screen_geometry.has_screen){//has screen
									//calculate obj position to judge whether it is in screen
									int left_in_screen, right_in_screen, in_screen;

									if(DetectResult.max_right-DetectResult.max_left<=52){
										int offset = (int)(DetectResult.avg_left + DetectResult.avg_right -256)/2;
										float view_angle = -((teacher.view_angle_max - teacher.view_angle_min)*teacher.ZoomPos/teacher.zoom_max)+teacher.view_angle_max;
										float offset_angle = atan(tan(view_angle*M_PI/360)*offset/128)*180/M_PI;
										int relative_pan_pos = teacher.PanPos - teacher.preset[0].pan_pos;
										float relative_angle = relative_pan_pos*teacher.pan_angle_right/teacher.pan_max;
										float obj_angle = (90-screen_geometry.camera_initial_angle)+relative_angle+offset_angle;
										debug_printf("avg_obj's angle = %f[%f,%f]\n",obj_angle, screen_angle_left, screen_angle_right);
										if(obj_angle>screen_angle_left && obj_angle<screen_angle_right){
											left_in_screen = 1;
											right_in_screen = 1;
										}
										else{
											left_in_screen = 0;
											right_in_screen = 0;
										}
									}
									else{
										int offset = (int)DetectResult.max_left+26;
										float view_angle = -((teacher.view_angle_max - teacher.view_angle_min)*teacher.ZoomPos/teacher.zoom_max)+teacher.view_angle_max;
										float offset_angle = atan(tan(view_angle*M_PI/360)*offset/128)*180/M_PI;
										int relative_pan_pos = teacher.PanPos - teacher.preset[0].pan_pos;
										float relative_angle = relative_pan_pos*teacher.pan_angle_right/teacher.pan_max;
										float obj_angle = (90-screen_geometry.camera_initial_angle)+relative_angle+offset_angle;
										debug_printf("left_obj's angle = %f[%f,%f]\n", obj_angle, screen_angle_left, screen_angle_right);
										if(obj_angle>screen_angle_left && obj_angle<screen_angle_right){
											left_in_screen = 1;
										}
										else{
											left_in_screen = 0;
										}

										offset = (int)DetectResult.max_right-26;
										view_angle = -((teacher.view_angle_max - teacher.view_angle_min)*teacher.ZoomPos/teacher.zoom_max)+teacher.view_angle_max;
										offset_angle = atan(tan(view_angle*M_PI/360)*offset/128)*180/M_PI;
										relative_pan_pos = teacher.PanPos - teacher.preset[0].pan_pos;
										relative_angle = relative_pan_pos*teacher.pan_angle_right/teacher.pan_max;
										obj_angle = (90-screen_geometry.camera_initial_angle)+relative_angle+offset_angle;
										debug_printf("right_obj's angle = %f[%f,%f]\n", obj_angle, screen_angle_left, screen_angle_right);
										if(obj_angle>screen_angle_left && obj_angle<screen_angle_right){
											right_in_screen = 1;
										}
										else{
											right_in_screen = 0;
										}
									}

									if((!left_in_screen) && (!right_in_screen)){
										in_screen = 0;
										int offset = (int)(DetectResult.avg_left + DetectResult.avg_right -256)/2;
										float view_angle = -((teacher.view_angle_max - teacher.view_angle_min)*teacher.ZoomPos/teacher.zoom_max)+teacher.view_angle_max;
										float offset_angle = atan(tan(view_angle*M_PI/360)*offset/128)*180/M_PI;
										int relative_pan_pos = teacher.PanPos - teacher.preset[0].pan_pos;
										float relative_angle = relative_pan_pos*teacher.pan_angle_right/teacher.pan_max;
										float obj_angle = (90-screen_geometry.camera_initial_angle)+relative_angle+offset_angle;
										if(obj_angle>screen_angle_left && obj_angle<screen_angle_right){
											if((abs(DetectResult.max_left-128))<abs(DetectResult.max_right-128)){
												DetectResult.avg_left = DetectResult.max_left;
												DetectResult.avg_right = DetectResult.avg_left+52;
											}
											else{
												DetectResult.avg_right = DetectResult.max_right;
												DetectResult.avg_left = DetectResult.avg_right-52;
											}
										}
									}
									else if((!left_in_screen) && right_in_screen){
										in_screen = 0;
										DetectResult.avg_left = DetectResult.max_left;
										DetectResult.avg_right = DetectResult.avg_left+52;
										DetectResult.max_right = DetectResult.max_left;
									}
									else if(left_in_screen && (!right_in_screen)){
										in_screen = 0;
										DetectResult.avg_right = DetectResult.max_right;
										DetectResult.avg_left = DetectResult.avg_right-52;
										DetectResult.max_left = DetectResult.max_right;
									}
									else{
										in_screen = 1;
									}

									if(in_screen){//in screen
										if(DetectResult.detected>=2){//motion
											motion_in_screen++;
											still_in_screen = 0;
											if(acc_of_lost<3){
												acc_of_lost = 0;
											}
											else{
												if(motion_in_screen>=2){
													acc_of_lost = 0;
												}
												else{
													acc_of_lost++;
												}
											}
										}
										else{//not motion
											still_in_screen++;
											motion_in_screen=0;
											if(acc_of_lost<3){
												if(still_in_screen < 90){
													acc_of_lost = 0;
												}
												else{
													acc_of_lost = 3;
												}
											}
											else{
												acc_of_lost++;
											}
										}
									}
									else{//not in screen
										//target captured
										acc_of_lost = 0;
										still_in_screen = 0;
										motion_in_screen = 0;
									}
								}
								else{//no screen, target captured, same as not in screen
									acc_of_lost = 0;
								}
							}
							else{//not detected
								acc_of_lost++;
								if(screen_geometry.has_screen){
									still_in_screen++;
									if(still_in_screen == 90){
										if(acc_of_lost<3){
											acc_of_lost = 3;
										}
									}
								}
							}

							//do tracking
							if(DetectResult.detected && acc_of_lost==0){//tracking when detected
								if(DetectResult.max_right - DetectResult.max_left >= 128){
									IsMultipleTeacher = 1;
								}
								else{
									IsMultipleTeacher = 0;
								}
#define HIDEN_PAUSE
#ifndef HIDEN_PAUSE
								int offset = (int)(DetectResult.avg_left + DetectResult.avg_right -256)/2;
								debug_printf("left = %d, right = %d\n", DetectResult.avg_left, DetectResult.avg_right);
								if(offset>=45 || offset<=-45){
									//float view_angle = -(52*ZoomPos/16384)+55.2;
									float view_angle = -((teacher.view_angle_max - teacher.view_angle_min)*teacher.ZoomPos/teacher.zoom_max)+teacher.view_angle_max;
									float pan_angle = atan(tan(view_angle*M_PI/360)*offset/128);
									//int relative_pos = 1600*pan_angle*180/M_PI/120;
									int relative_pos = teacher.pan_max*pan_angle*180/M_PI/teacher.pan_angle_right;
									int destpan_pos = teacher.PanPos + relative_pos;
									debug_printf("offset = %d, view_angle = %f, pan_angle = %f\n", offset, view_angle, pan_angle*180/M_PI);
									debug_printf("relative_pos=%d, PanPos=%d\n", relative_pos, teacher.PanPos);

									if(!IsExclude){
										if(destpan_pos>limit_right){
											destpan_pos = limit_right;
										}

										if(destpan_pos<limit_left){
											destpan_pos = limit_left;
										}
									}
									else{
										if(teacher.PanPos>=exclude_right && teacher.PanPos<=limit_right){
											if(destpan_pos>limit_right){
												destpan_pos = limit_right;
											}

											if(destpan_pos<exclude_right){
												destpan_pos = exclude_right;
											}
										}
										if(teacher.PanPos>=limit_left && teacher.PanPos<=exclude_left){
											if(destpan_pos>exclude_left){
												destpan_pos = exclude_left;
											}

											if(destpan_pos<limit_left){
												destpan_pos = limit_left;
											}
										}
									}

									if(destpan_pos>teacher.PanPos){
										direction = 1;
									}
									else{
										direction = -1;
									}

									if(destpan_pos != teacher.PanPos){
										pthread_mutex_lock(&g_capture_sync_mutex);
										g_camholder_still = 0;
										g_analyze_frame_num = 0;
										g_num_in_group = 0;
										pthread_mutex_unlock(&g_capture_sync_mutex);
										AbsPos(thread_param, &teacher, destpan_pos, teacher.TiltPos);
										//motion_skip_num = 0;
									}
								}
#else
#define OFFSET_THRES_LOW 26
#define OFFSET_THRES_HIGH 78
								int do_teacher_tracking = 0;
								int offset = (int)(DetectResult.avg_left + DetectResult.avg_right -256)/2;
								debug_printf("left = %d, right = %d\n", DetectResult.avg_left, DetectResult.avg_right);
								if(abs(offset)<=OFFSET_THRES_LOW){
									in_left = 0;
									in_right = 0;
								}
								else if(offset>OFFSET_THRES_LOW && offset<OFFSET_THRES_HIGH){
									in_left = 0;
									in_right++;
									if(in_right==10){
										do_teacher_tracking = 1;
									}
								}
								else if(offset>-OFFSET_THRES_HIGH && offset<-OFFSET_THRES_LOW){
									in_left++;
									in_right = 0;
									if(in_left==10){
										do_teacher_tracking = 1;
									}
								}
								else{
									do_teacher_tracking = 1;
								}

								if(do_teacher_tracking){
									in_left = 0;
									in_right = 0;
									float view_angle = -((teacher.view_angle_max - teacher.view_angle_min)*teacher.ZoomPos/teacher.zoom_max)+teacher.view_angle_max;
									float pan_angle = atan(tan(view_angle*M_PI/360)*offset/128);
									int relative_pos = teacher.pan_max*pan_angle*180/M_PI/teacher.pan_angle_right;
									int destpan_pos = teacher.PanPos + relative_pos;
									debug_printf("offset = %d, view_angle = %f, pan_angle = %f\n", offset, view_angle, pan_angle*180/M_PI);
									debug_printf("relative_pos=%d, PanPos=%d\n", relative_pos, teacher.PanPos);

									if(!IsExclude){
										if(destpan_pos>limit_right){
											destpan_pos = limit_right;
										}

										if(destpan_pos<limit_left){
											destpan_pos = limit_left;
										}
									}
									else{
										if(teacher.PanPos>=exclude_right && teacher.PanPos<=limit_right){
											if(destpan_pos>limit_right){
												destpan_pos = limit_right;
											}

											if(destpan_pos<exclude_right){
												destpan_pos = exclude_right;
											}
										}
										if(teacher.PanPos>=limit_left && teacher.PanPos<=exclude_left){
											if(destpan_pos>exclude_left){
												destpan_pos = exclude_left;
											}

											if(destpan_pos<limit_left){
												destpan_pos = limit_left;
											}
										}
									}

									if(destpan_pos>teacher.PanPos){
										direction = 1;
									}
									else{
										direction = -1;
									}

									float pan_angle_threshold = atan(tan(view_angle*M_PI/360)*OFFSET_THRES_HIGH/128);
									int relative_pos_threshold = teacher.pan_max*pan_angle*180/M_PI/teacher.pan_angle_right;
									if(abs(destpan_pos-teacher.PanPos)<abs(relative_pos) || abs(destpan_pos-teacher.PanPos)<relative_pos_threshold)
									{
										need_hiden = 0;
									}
									else{
										need_hiden =1;
									}

									if(destpan_pos != teacher.PanPos){
										pthread_mutex_lock(&g_capture_sync_mutex);
										g_camholder_still = 0;
										g_analyze_frame_num = 0;
										g_num_in_group = 0;
										pthread_mutex_unlock(&g_capture_sync_mutex);
										AbsPos(thread_param, &teacher, destpan_pos, teacher.TiltPos);
									}
								}
								else{
									need_hiden = 0;
								}
#endif
							}

							//scan when lost target
							if(acc_of_lost >= 6 && (acc_of_lost%3)==0){
								int offset = 128*direction;
								float view_angle = -((teacher.view_angle_max - teacher.view_angle_min)*teacher.ZoomPos/teacher.zoom_max)+teacher.view_angle_max;
								float pan_angle = atan(tan(view_angle*M_PI/360)*offset/128);
								//int relative_pos = 1600*pan_angle*180/M_PI/120;
								int relative_pos = teacher.pan_max*pan_angle*180/M_PI/teacher.pan_angle_right;
								int destpan_pos = teacher.PanPos + relative_pos;

								if(!IsExclude){
									if(destpan_pos>limit_right){
										destpan_pos = limit_right;
										direction = direction*(-1);
									}

									if(destpan_pos<limit_left){
										destpan_pos = limit_left;
										direction = direction*(-1);
									}
								}
								else{
									if(teacher.PanPos>=exclude_right && teacher.PanPos<=limit_right){
										if(destpan_pos>limit_right){
											destpan_pos = limit_right;
											direction = direction*(-1);
										}

										if(destpan_pos<exclude_right){
											destpan_pos = exclude_right;
											if(destpan_pos == teacher.PanPos){
												destpan_pos = exclude_left;
											}
										}
									}
									if(teacher.PanPos>=limit_left && teacher.PanPos<=exclude_left){
										if(destpan_pos>exclude_left){
											destpan_pos = exclude_left;
											if(destpan_pos == teacher.PanPos){
												destpan_pos = exclude_right;
											}
										}

										if(destpan_pos<limit_left){
											destpan_pos = limit_left;
											direction = direction*(-1);
										}
									}
								}

								if(destpan_pos != teacher.PanPos){
									pthread_mutex_lock(&g_capture_sync_mutex);
									g_camholder_still = 0;
									g_analyze_frame_num = 0;
									g_num_in_group = 0;
									pthread_mutex_unlock(&g_capture_sync_mutex);
									AbsPos(thread_param, &teacher, destpan_pos, teacher.TiltPos);
									//motion_skip_num = 0;
								}
							}
						}
					}
				}
#endif

#if 1
				//do student tracking
				if(!strcmp(cmd, COMMAND_EVENT_AUX)){
					if(thread_param->student_status == 5){
						t_aux_result result;
						GetAuxResult(&result, (char *)datafifo_head(thread_param->Event_consumer_fifo)->arg.data);
						if(result.student_in_standing){
							get_student_tracking_param(&student_tracking_geometry, &result);
						}
						if(student_tracking_geometry.dotracking){
							int relative_pan_pos = student.pan_max*student_tracking_geometry.target_pan_angle*180/M_PI/student.pan_angle_right;
							int destpan_pos = student.preset[0].pan_pos + relative_pan_pos;
							if(destpan_pos>student.pan_max){
								destpan_pos = student.pan_max;
							}
							if(destpan_pos<student.pan_min){
								destpan_pos = student.pan_min;
							}
							int relative_tilt_pos = student.tilt_max*student_tracking_geometry.target_tilt_angle*180/M_PI/student.tilt_angle_up;
							int desttilt_pos = student.preset[0].tilt_pos + relative_tilt_pos;
							if(desttilt_pos>student.tilt_max){
								desttilt_pos = student.tilt_max;
							}
							if(desttilt_pos<student.tilt_min){
								desttilt_pos = student.tilt_min;
							}
							AbsPos(thread_param, &student, destpan_pos, desttilt_pos);
							//float view_angle = -((teacher.view_angle_max - teacher.view_angle_min)*teacher.ZoomPos/teacher.zoom_max)+teacher.view_angle_max;
							//(teacher.view_angle_max - teacher.view_angle_min)*teacher.ZoomPos/teacher.zoom_max = teacher.view_angle_max - view_angle;
							//(teacher.view_angle_max - teacher.view_angle_min)*teacher.ZoomPos = (teacher.view_angle_max - view_angle)*teacher.zoom_max;
							//teacher.ZoomPos = (teacher.view_angle_max - view_angle)*teacher.zoom_max/(teacher.view_angle_max - teacher.view_angle_min);
							int destzoom_pos = (student.view_angle_max-student_tracking_geometry.target_view_angle*180/M_PI)*student.zoom_max/(student.view_angle_max-student.view_angle_min);
							if(destzoom_pos>student.zoom_max){
								destzoom_pos = student.zoom_max;
							}
							if(destzoom_pos<student.zoom_min){
								destzoom_pos = student.zoom_min;
							}
							debug_printf("destzoom = %d\n", destzoom_pos);
							ZoomDirect(thread_param, &student, destzoom_pos);
						}
					}
					else{//test purpose only
						t_aux_result result;
						GetAuxResult(&result, (char *)datafifo_head(thread_param->Event_consumer_fifo)->arg.data);
						if(result.student_in_standing){
							get_student_tracking_param(&student_tracking_geometry, &result);
							//debug_printf("x=%d, y=%d\n", student_tracking_geometry.student_location.x, student_tracking_geometry.student_location.y);
							int destzoom_pos = (student.view_angle_max-student_tracking_geometry.target_view_angle*180/M_PI)*student.zoom_max/(student.view_angle_max-student.view_angle_min);
							if(destzoom_pos>student.zoom_max){
								destzoom_pos = student.zoom_max;
							}
							if(destzoom_pos<student.zoom_min){
								destzoom_pos = student.zoom_min;
							}
							debug_printf("destzoom = %d\n", destzoom_pos);
						}
					}
				}
#endif

#if 1
			//update condition table
#ifndef HIDEN_PAUSE
			if(!strcmp(cmd, COMMAND_EVENT_VID)){
				if(acc_of_lost==0){
					if(IsMultipleTeacher){
						truth_table_element_condition_set(COND_TEACHER_DETECTED_MULTIPLE, &truth_table);
						truth_table_element_condition_reset(COND_TEACHER_DETECTED_ONE, &truth_table);

					}
					else{
						truth_table_element_condition_reset(COND_TEACHER_DETECTED_MULTIPLE, &truth_table);
						truth_table_element_condition_set(COND_TEACHER_DETECTED_ONE, &truth_table);
					}
					truth_table_element_condition_reset(COND_TEACHER_DETECTED_NONE, &truth_table);
				}
				else{
					if(acc_of_lost>=3){
						truth_table_element_condition_set(COND_TEACHER_DETECTED_NONE, &truth_table);
						truth_table_element_condition_reset(COND_TEACHER_DETECTED_ONE, &truth_table);
						truth_table_element_condition_reset(COND_TEACHER_DETECTED_MULTIPLE, &truth_table);
					}
				}
			}
#else
			if(!strcmp(cmd, COMMAND_EVENT_VID)){
				if(acc_of_lost==0){
					if(!need_hiden){
						if(IsMultipleTeacher){
							truth_table_element_condition_set(COND_TEACHER_DETECTED_MULTIPLE, &truth_table);
							truth_table_element_condition_reset(COND_TEACHER_DETECTED_ONE, &truth_table);

						}
						else{
							truth_table_element_condition_reset(COND_TEACHER_DETECTED_MULTIPLE, &truth_table);
							truth_table_element_condition_set(COND_TEACHER_DETECTED_ONE, &truth_table);
						}
						truth_table_element_condition_reset(COND_TEACHER_DETECTED_NONE, &truth_table);
					}
					else{
						truth_table_element_condition_set(COND_TEACHER_DETECTED_NONE, &truth_table);
						truth_table_element_condition_reset(COND_TEACHER_DETECTED_ONE, &truth_table);
						truth_table_element_condition_reset(COND_TEACHER_DETECTED_MULTIPLE, &truth_table);
					}
				}
				else{
					if(acc_of_lost>=3){
						truth_table_element_condition_set(COND_TEACHER_DETECTED_NONE, &truth_table);
						truth_table_element_condition_reset(COND_TEACHER_DETECTED_ONE, &truth_table);
						truth_table_element_condition_reset(COND_TEACHER_DETECTED_MULTIPLE, &truth_table);
					}
				}
			}
#endif

			if(!strcmp(cmd, COMMAND_EVENT_AUX)){
				t_aux_result result;
				GetAuxResult(&result, (char *)datafifo_head(thread_param->Event_consumer_fifo)->arg.data);
#if 1
				if(result.student_in_standing){// the purpose is to exclude the target not in specified area. need get_student_tracking_param to support
					get_student_tracking_param(&student_tracking_geometry, &result);//for student_in_standing post correction
				}
#endif
				if(!result.student_in_standing){
					acc_student_lost++;
					if(acc_student_lost == 6){
						truth_table_element_condition_set(COND_STUDENT_STAND_NONE, &truth_table);
						truth_table_element_condition_reset(COND_STUDENT_STAND_ONE, &truth_table);
						truth_table_element_condition_reset(COND_STUDENT_STAND_MULTIPLE, &truth_table);
					}
				}
				else if(result.student_in_standing == 1){
					acc_student_lost = 0;
					truth_table_element_condition_reset(COND_STUDENT_STAND_NONE, &truth_table);
					truth_table_element_condition_set(COND_STUDENT_STAND_ONE, &truth_table);
					truth_table_element_condition_reset(COND_STUDENT_STAND_MULTIPLE, &truth_table);
				}
				else{
					acc_student_lost = 0;
					truth_table_element_condition_reset(COND_STUDENT_STAND_NONE, &truth_table);
					truth_table_element_condition_reset(COND_STUDENT_STAND_ONE, &truth_table);
					truth_table_element_condition_set(COND_STUDENT_STAND_MULTIPLE, &truth_table);
				}
			}

			if(!strcmp(cmd, COMMAND_EVENT_VGA)){
				t_VGA_result result;
				GetVGAResult(&result, (char *)datafifo_head(thread_param->Event_consumer_fifo)->arg.data);
				if(result.changed == 1){
					truth_table_element_condition_set(COND_VGA_CHG, &truth_table);
					truth_table_element_condition_reset(COND_VGA_UNCHG, &truth_table);
					acc_of_unchange = 0;
				}
				else if(result.changed == 2){
					acc_of_unchange = g_cfg.VGADuration;
					truth_table_element_condition_set(COND_VGA_UNCHG, &truth_table);
					truth_table_element_condition_reset(COND_VGA_CHG, &truth_table);
				}
				else{
					acc_of_unchange++;
					if(acc_of_unchange == g_cfg.VGADuration){
						truth_table_element_condition_set(COND_VGA_UNCHG, &truth_table);
						truth_table_element_condition_reset(COND_VGA_CHG, &truth_table);
					}
				}
			}

			//add blackboard event here
#endif
			}
			datafifo_consumer_remove_head(thread_param->Event_consumer_fifo);
		}

		//do switch FSM transition
#if 1
		gettimeofday(&timeval2, NULL);
		int interval = (timeval2.tv_sec - timeval1.tv_sec)*1000 + (timeval2.tv_usec - timeval1.tv_usec)/1000;

		if(g_cfg.IsAutoInstuct==2 && interval>=3000){
			char ret = fsm_state_update(&fsm);
			//debug_printf("state machine updated and ret = %d\n", ret);

			//enter new state handling
			if(ret != FSM_NOTRANSITION){
				//record the transition time
				timeval1.tv_sec = timeval2.tv_sec;
				timeval1.tv_usec = timeval2.tv_usec;
				debug_printf("state not change\n");
			}

			if(ret == FSM_VGA){
				size_t size;
				t_cmd outmsg;
				fill_cmd_header(&outmsg, COMMAND_SWITCHCTRL);
				add_attr_int(&outmsg, "CaptureId", VIDEO_FUNCTION_VGA); //VGA's function id and capture id are identical
				add_attr_int(&outmsg, "PIP", 0); //0:normal

				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
				size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
				int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
				datafifo_producer_data_add(thread_param->Switch_producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
				vmap_table_reset_pip();
				debug_printf("change to FSM_VGA state\n");
			}

			if(ret == FSM_TEACHER_CLOSEUP){
				size_t size;
				t_cmd outmsg;
				fill_cmd_header(&outmsg, COMMAND_SWITCHCTRL);
				int cap_id = vmap_table_get_capid_by_function(VIDEO_FUNCTION_TEACHER_CLOSEUP);
				add_attr_int(&outmsg, "CaptureId", cap_id);
				add_attr_int(&outmsg, "PIP", 0); //0:normal

				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
				size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
				int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
				datafifo_producer_data_add(thread_param->Switch_producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
				vmap_table_reset_pip();
				debug_printf("change to FSM_TEACHER_CLOSEUP state\n");
			}

			if(ret == FSM_STUDENT_CLOSEUP){
				size_t size;
				t_cmd outmsg;
				fill_cmd_header(&outmsg, COMMAND_SWITCHCTRL);
				int cap_id = vmap_table_get_capid_by_function(VIDEO_FUNCTION_STUDENT_CLOSEUP);
				add_attr_int(&outmsg, "CaptureId", cap_id);
				add_attr_int(&outmsg, "PIP", 0); //0:normal

				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
				size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
				int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
				datafifo_producer_data_add(thread_param->Switch_producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
				vmap_table_reset_pip();
				debug_printf("change to FSM_STUDENT_CLOSEUP state\n");
			}

			if(ret == FSM_TEACHER_PANORAMA){
				size_t size;
				t_cmd outmsg;
				fill_cmd_header(&outmsg, COMMAND_SWITCHCTRL);
				int cap_id = vmap_table_get_capid_by_function(VIDEO_FUNCTION_TEACHER_PANORAMA);
				add_attr_int(&outmsg, "CaptureId", cap_id);
				add_attr_int(&outmsg, "PIP", 0); //0:normal

				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
				size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
				int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
				datafifo_producer_data_add(thread_param->Switch_producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
				vmap_table_reset_pip();
				debug_printf("change to FSM_TEACHER_PANORAMA state\n");
			}

			if(ret == FSM_STUDENT_PANORAMA){
				size_t size;
				t_cmd outmsg;
				fill_cmd_header(&outmsg, COMMAND_SWITCHCTRL);
				int cap_id = vmap_table_get_capid_by_function(VIDEO_FUNCTION_STUDENT_PANORAMA);
				add_attr_int(&outmsg, "CaptureId", cap_id);
				add_attr_int(&outmsg, "PIP", 0); //0:normal

				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
				size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
				int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
				datafifo_producer_data_add(thread_param->Switch_producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
				vmap_table_reset_pip();
				debug_printf("change to FSM_STUDENT_PANORAMA state\n");
			}

			if(ret == FSM_BOARD_CLOSEUP){
				size_t size;
				t_cmd outmsg;
				fill_cmd_header(&outmsg, COMMAND_SWITCHCTRL);
				int cap_id = vmap_table_get_capid_by_function(VIDEO_FUNCTION_BOARD_CLOSEUP);
				add_attr_int(&outmsg, "CaptureId", cap_id);
				add_attr_int(&outmsg, "PIP", 0); //0:normal

				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
				size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
				int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
				datafifo_producer_data_add(thread_param->Switch_producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
				vmap_table_reset_pip();
				debug_printf("change to FSM_BOARD_CLOSEUP state\n");
			}

			if(ret == FSM_VGA_PIP_TEACHER_CLOSEUP){
				size_t size;
				t_cmd outmsg;
				fill_cmd_header(&outmsg, COMMAND_SWITCHCTRL);
				//int cap_id = vmap_table_get_capid_by_function(VIDEO_FUNCTION_TEACHER_CLOSEUP);
				add_attr_int(&outmsg, "CaptureId", VIDEO_FUNCTION_VGA);
				add_attr_int(&outmsg, "PIP", 1); //0:normal

				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
				size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
				int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
				datafifo_producer_data_add(thread_param->Switch_producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
				vmap_table_set_pip_by_function(VIDEO_FUNCTION_TEACHER_CLOSEUP);
				debug_printf("change to FSM_VGA_PIP_TEACHER_CLOSEUP state\n");
			}

			if(ret == FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP){
				size_t size;
				t_cmd outmsg;
				fill_cmd_header(&outmsg, COMMAND_SWITCHCTRL);
				int cap_id = vmap_table_get_capid_by_function(VIDEO_FUNCTION_TEACHER_CLOSEUP);
				add_attr_int(&outmsg, "CaptureId", cap_id);
				add_attr_int(&outmsg, "PIP", 1); //0:normal

				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
				size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
				int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
				datafifo_producer_data_add(thread_param->Switch_producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
				vmap_table_set_pip_by_function(VIDEO_FUNCTION_STUDENT_CLOSEUP);
				debug_printf("change to FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP state\n");
			}

			if(ret == FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP){
				size_t size;
				t_cmd outmsg;
				fill_cmd_header(&outmsg, COMMAND_SWITCHCTRL);
				int cap_id = vmap_table_get_capid_by_function(VIDEO_FUNCTION_BOARD_CLOSEUP);
				add_attr_int(&outmsg, "CaptureId", cap_id);
				add_attr_int(&outmsg, "PIP", 1); //0:normal

				char2hex(outmsg.data_len, &size, DATA_LEN_SIZE);
				size += (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE);
				int flags = FRAME_FLAG_NEWFRAME | FRAME_FLAG_FRAMEEND;
				datafifo_producer_data_add(thread_param->Switch_producer_fifo, (unsigned char *)outmsg.fullbuf, (int)size, flags, 0);
				vmap_table_set_pip_by_function(VIDEO_FUNCTION_STUDENT_CLOSEUP);
				debug_printf("change to FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP state\n");
			}
		}
#endif
		usleep(1000);
	}

	debug_printf("thread camholder control exit\n");
	thread_param->thread_canceled = 2;

	pthread_exit(0);
}

int make_reg09_int(t_video_map *vmap_table)
{
	int value = 0;
	int i;
	for(i = 0; i<5; i++){
		if(!(vmap_table[i].cap_id&0x10)){
			value |= (1<<i);
		}
	}
	return value;
}

int main(int argc, char **argv)
{
	signal(SIGPIPE, SIG_IGN);

	vmap_table_init(g_vmap_file);
	vmap_table_load(SYS_CONFIG_FILE_NAME, g_vmap_table);
	g_reg_09 = make_reg09_int(g_vmap_table);
	debug_printf("Reg_09=%x\n", g_reg_09);

	config_init(g_config_file);
	config_load(SYS_CONFIG_FILE_NAME, &g_cfg);

	memset(&g_forward_table, 0, sizeof(t_forward_table));
	forward_table_load(SYS_CONFIG_FILE_NAME, &g_forward_table);

	int ret;
	//declare fifos
	t_datafifo Ctrl_producer_fifo;
	t_datafifo Loopback_producer_fifo;

	t_datafifo Vmux_store_producer_fifo;
	t_datafifo Vmux_live_producer_fifo;
	t_datafifo VGA_producer_fifo;
	t_datafifo Audio_producer_fifo;
	t_datafifo V1_producer_fifo;
	t_datafifo V2_producer_fifo;
	t_datafifo V3_producer_fifo;
	t_datafifo V4_producer_fifo;
	t_datafifo V5_producer_fifo;
	t_datafifo V6_producer_fifo;

	t_datafifo ImgM_producer_fifo;
	t_datafifo ImgV_producer_fifo;
	t_datafifo Img1_producer_fifo;
	t_datafifo Img2_producer_fifo;
	t_datafifo Img3_producer_fifo;
	t_datafifo Img4_producer_fifo;
	t_datafifo Img5_producer_fifo;
	t_datafifo Img6_producer_fifo;

	t_datafifo Event_producer_fifo_A;
	t_datafifo Event_producer_fifo_B;
	t_datafifo Event_producer_fifo_C;
	t_datafifo Event_producer_fifo_D;
	t_datafifo Event_producer_fifo_Analyze;

	t_datafifo UART_RX_producer_fifo_A1;
	t_datafifo UART_RX_producer_fifo_A2;
	t_datafifo UART_RX_producer_fifo_B1;
	t_datafifo UART_RX_producer_fifo_B2;
	t_datafifo UART_RX_producer_fifo_C1;
	t_datafifo UART_RX_producer_fifo_C2;

	t_datafifo UART_TX_producer_fifo_A;
	t_datafifo UART_TX_producer_fifo_B;
	t_datafifo UART_TX_producer_fifo_C;

	t_datafifo Switch_producer_fifo;

	t_datafifo JPEG_producer_fifo;
	t_datafifo PIP_producer_fifo;
	t_datafifo Filtered_img_producer_fifo;

	//consumer fifos
	t_datafifo Ctrl_consumer_fifo_A;
	t_datafifo Ctrl_consumer_fifo_B;
	t_datafifo Ctrl_consumer_fifo_C;
	t_datafifo Ctrl_consumer_fifo_D;
	t_datafifo Ctrl_consumer_fifo_localsave;
	t_datafifo Ctrl_consumer_fifo_SwitchTracking;

	t_datafifo V_consumer_fifo;
	t_datafifo Img_consumer_fifo;

	t_datafifo Instructor_consumer_fifo;
	//t_datafifo PIP_consumer_fifo;
	t_datafifo Filtered_img_consumer_fifo;

	t_datafifo UART_RX_consumer_fifo_A1;
	t_datafifo UART_RX_consumer_fifo_A2;
	t_datafifo UART_RX_consumer_fifo_B1;
	t_datafifo UART_RX_consumer_fifo_B2;
	t_datafifo UART_RX_consumer_fifo_C1;
	t_datafifo UART_RX_consumer_fifo_C2;
	t_datafifo UART_TX_consumer_fifo_A;
	t_datafifo UART_TX_consumer_fifo_B;
	t_datafifo UART_TX_consumer_fifo_C;

	t_datafifo Event_consumer_fifo;

	//init and connect the fifos
	datafifo_init(&Ctrl_producer_fifo, EINBUF_SIZE, EINBUF_CNT);
	datafifo_init(&Loopback_producer_fifo, EINBUF_SIZE, EINBUF_CNT);

	datafifo_init(&Vmux_store_producer_fifo, VINBUF_SIZE, VINBUF_CNT);
	datafifo_init(&Vmux_live_producer_fifo, VINBUF_SIZE, VINBUF_CNT);
	datafifo_init(&VGA_producer_fifo, VINBUF_SIZE, VINBUF_CNT);
	datafifo_init(&Audio_producer_fifo, AINBUF_SIZE, AINBUF_CNT);
	datafifo_init(&V1_producer_fifo, VINBUF_SIZE, VINBUF_CNT);
	datafifo_init(&V2_producer_fifo, VINBUF_SIZE, VINBUF_CNT);
	datafifo_init(&V3_producer_fifo, VINBUF_SIZE, VINBUF_CNT);
	datafifo_init(&V4_producer_fifo, VINBUF_SIZE, VINBUF_CNT);
	datafifo_init(&V5_producer_fifo, VINBUF_SIZE, VINBUF_CNT);
	datafifo_init(&V6_producer_fifo, VINBUF_SIZE, VINBUF_CNT);

	datafifo_init(&ImgM_producer_fifo, IMGBUF_SIZE, IMGBUF_CNT);
	datafifo_init(&ImgV_producer_fifo, IMGBUF_SIZE, IMGBUF_CNT);
	datafifo_init(&Img1_producer_fifo, IMGBUF_SIZE, IMGBUF_CNT);
	datafifo_init(&Img2_producer_fifo, IMGBUF_SIZE, IMGBUF_CNT);
	datafifo_init(&Img3_producer_fifo, IMGBUF_SIZE, IMGBUF_CNT);
	datafifo_init(&Img4_producer_fifo, IMGBUF_SIZE, IMGBUF_CNT);
	datafifo_init(&Img5_producer_fifo, IMGBUF_SIZE, IMGBUF_CNT);
	datafifo_init(&Img6_producer_fifo, IMGBUF_SIZE, IMGBUF_CNT);

	datafifo_init(&Event_producer_fifo_A, EINBUF_SIZE, EINBUF_CNT);
	datafifo_init(&Event_producer_fifo_B, EINBUF_SIZE, EINBUF_CNT);
	datafifo_init(&Event_producer_fifo_C, EINBUF_SIZE, EINBUF_CNT);
	datafifo_init(&Event_producer_fifo_D, EINBUF_SIZE, EINBUF_CNT);
	datafifo_init(&Event_producer_fifo_Analyze, EINBUF_SIZE, EINBUF_CNT);

	datafifo_init(&UART_TX_producer_fifo_A, EINBUF_SIZE, EINBUF_CNT);
	datafifo_init(&UART_TX_producer_fifo_B, EINBUF_SIZE, EINBUF_CNT);
	datafifo_init(&UART_TX_producer_fifo_C, EINBUF_SIZE, EINBUF_CNT);


	datafifo_init(&UART_RX_producer_fifo_A1, BYPBUF_SIZE, BYPBUF_CNT);
	datafifo_init(&UART_RX_producer_fifo_A2, BYPBUF_SIZE, BYPBUF_CNT);
	datafifo_init(&UART_RX_producer_fifo_B1, BYPBUF_SIZE, BYPBUF_CNT);
	datafifo_init(&UART_RX_producer_fifo_B2, BYPBUF_SIZE, BYPBUF_CNT);
	datafifo_init(&UART_RX_producer_fifo_C1, BYPBUF_SIZE, BYPBUF_CNT);
	datafifo_init(&UART_RX_producer_fifo_C2, BYPBUF_SIZE, BYPBUF_CNT);

	datafifo_init(&Switch_producer_fifo, EINBUF_SIZE, EINBUF_CNT);

	datafifo_init(&JPEG_producer_fifo,EINBUF_SIZE, 64*EINBUF_CNT);
	datafifo_init(&PIP_producer_fifo, EINBUF_SIZE, 64*EINBUF_CNT);
	datafifo_init(&Filtered_img_producer_fifo, IMGBUF_SIZE, IMGBUF_CNT);

	datafifo_init(&Ctrl_consumer_fifo_A, 0, 64*EINBUF_CNT);
	datafifo_connect(&Ctrl_producer_fifo, &Ctrl_consumer_fifo_A);
	datafifo_connect(&Switch_producer_fifo, &Ctrl_consumer_fifo_A);
	datafifo_connect(&PIP_producer_fifo, &Ctrl_consumer_fifo_A);

	datafifo_init(&Ctrl_consumer_fifo_B, 0, EINBUF_CNT);
	datafifo_connect(&Ctrl_producer_fifo, &Ctrl_consumer_fifo_B);

	datafifo_init(&Ctrl_consumer_fifo_C, 0, EINBUF_CNT);
	datafifo_connect(&Ctrl_producer_fifo, &Ctrl_consumer_fifo_C);

	datafifo_init(&Ctrl_consumer_fifo_D, 0, EINBUF_CNT);
	datafifo_connect(&Ctrl_producer_fifo, &Ctrl_consumer_fifo_D);

	datafifo_init(&Ctrl_consumer_fifo_localsave, 0, EINBUF_CNT);
	datafifo_connect(&Ctrl_producer_fifo, &Ctrl_consumer_fifo_localsave);

	datafifo_init(&Ctrl_consumer_fifo_SwitchTracking, 0, EINBUF_CNT);
	datafifo_connect(&Ctrl_producer_fifo, &Ctrl_consumer_fifo_SwitchTracking);

	datafifo_init(&V_consumer_fifo, 0, 8*VINBUF_CNT);
	datafifo_connect(&Vmux_store_producer_fifo, &V_consumer_fifo);
	datafifo_connect(&Vmux_live_producer_fifo, &V_consumer_fifo);
	datafifo_connect(&VGA_producer_fifo, &V_consumer_fifo);
	datafifo_connect(&Audio_producer_fifo, &V_consumer_fifo);
	datafifo_connect(&V1_producer_fifo, &V_consumer_fifo);
	datafifo_connect(&V2_producer_fifo, &V_consumer_fifo);
	datafifo_connect(&V3_producer_fifo, &V_consumer_fifo);
	datafifo_connect(&V4_producer_fifo, &V_consumer_fifo);
	datafifo_connect(&V5_producer_fifo, &V_consumer_fifo);
	datafifo_connect(&V6_producer_fifo, &V_consumer_fifo);

	datafifo_init(&Img_consumer_fifo, 0, 8*IMGBUF_CNT);
	datafifo_connect(&ImgM_producer_fifo, &Img_consumer_fifo);
	datafifo_connect(&ImgV_producer_fifo, &Img_consumer_fifo);
	datafifo_connect(&Img1_producer_fifo, &Img_consumer_fifo);
	datafifo_connect(&Img2_producer_fifo, &Img_consumer_fifo);
	datafifo_connect(&Img3_producer_fifo, &Img_consumer_fifo);
	datafifo_connect(&Img4_producer_fifo, &Img_consumer_fifo);
	datafifo_connect(&Img5_producer_fifo, &Img_consumer_fifo);
	datafifo_connect(&Img6_producer_fifo, &Img_consumer_fifo);

	datafifo_init(&Instructor_consumer_fifo, 0, 64*EINBUF_CNT);
	datafifo_connect(&JPEG_producer_fifo, &Instructor_consumer_fifo);
	datafifo_connect(&Loopback_producer_fifo, &Instructor_consumer_fifo);

	datafifo_init(&Filtered_img_consumer_fifo, 0, IMGBUF_CNT);
	datafifo_connect(&Filtered_img_producer_fifo, &Filtered_img_consumer_fifo);

	datafifo_init(&UART_RX_consumer_fifo_A1, 0, BYPBUF_CNT);
	datafifo_connect(&UART_RX_producer_fifo_A1, &UART_RX_consumer_fifo_A1);
	datafifo_init(&UART_RX_consumer_fifo_A2, 0, BYPBUF_CNT);
	datafifo_connect(&UART_RX_producer_fifo_A2, &UART_RX_consumer_fifo_A2);
	datafifo_init(&UART_RX_consumer_fifo_B1, 0, BYPBUF_CNT);
	datafifo_connect(&UART_RX_producer_fifo_B1, &UART_RX_consumer_fifo_B1);
	datafifo_init(&UART_RX_consumer_fifo_B2, 0, BYPBUF_CNT);
	datafifo_connect(&UART_RX_producer_fifo_B2, &UART_RX_consumer_fifo_B2);
	datafifo_init(&UART_RX_consumer_fifo_C1, 0, BYPBUF_CNT);
	datafifo_connect(&UART_RX_producer_fifo_C1, &UART_RX_consumer_fifo_C1);
	datafifo_init(&UART_RX_consumer_fifo_C2, 0, BYPBUF_CNT);
	datafifo_connect(&UART_RX_producer_fifo_C2, &UART_RX_consumer_fifo_C2);

	datafifo_init(&UART_TX_consumer_fifo_A, 0, EINBUF_CNT);
	datafifo_connect(&UART_TX_producer_fifo_A, &UART_TX_consumer_fifo_A);
	datafifo_init(&UART_TX_consumer_fifo_B, 0, EINBUF_CNT);
	datafifo_connect(&UART_TX_producer_fifo_B, &UART_TX_consumer_fifo_B);
	datafifo_init(&UART_TX_consumer_fifo_C, 0, EINBUF_CNT);
	datafifo_connect(&UART_TX_producer_fifo_C, &UART_TX_consumer_fifo_C);

	datafifo_init(&Event_consumer_fifo, 0, EINBUF_CNT);
	datafifo_connect(&Event_producer_fifo_A, &Event_consumer_fifo);
	datafifo_connect(&Event_producer_fifo_B, &Event_consumer_fifo);
	datafifo_connect(&Event_producer_fifo_C, &Event_consumer_fifo);
	datafifo_connect(&Event_producer_fifo_D, &Event_consumer_fifo);
	datafifo_connect(&Event_producer_fifo_Analyze, &Event_consumer_fifo);

	t_instructor_thread_param	instructor_thread_param;
	t_coreA_thread_param	coreA_thread_param;
	t_coreBC_thread_param	coreB_thread_param;
	t_coreBC_thread_param	coreC_thread_param;
	t_coreD_thread_param	coreD_thread_param;
	t_localsave_thread_param	localsave_thread_param;
	t_SwitchTracking_thread_param	SwitchTracking_thread_param;
	t_videoanalyze_thread_param videoanalyze_thread_param;
	t_img_thread_param	img_thread_param;

	pthread_t instructorlisten_thread_id;
	pthread_t localsave_thread_id;
	pthread_t SwitchTracking_thread_id;
	pthread_t videoanalyze_thread_id;
	pthread_t img_thread_id;
	pthread_t coreA_thread_id, coreB_thread_id, coreC_thread_id, coreD_thread_id;

	coreA_thread_param.Audio_producer_fifo = &Audio_producer_fifo;
	coreA_thread_param.VGA_producer_fifo = &VGA_producer_fifo;
	coreA_thread_param.Vlive_producer_fifo = &Vmux_live_producer_fifo;
	coreA_thread_param.Vstore_producer_fifo = &Vmux_store_producer_fifo;
	coreA_thread_param.ImgM_producer_fifo = &ImgM_producer_fifo;
	coreA_thread_param.ImgV_producer_fifo = &ImgV_producer_fifo;
	coreA_thread_param.Event_producer_fifo = &Event_producer_fifo_A;
	coreA_thread_param.UART_RX_producer_fifo_1 = &UART_RX_producer_fifo_A1;
	coreA_thread_param.UART_RX_producer_fifo_2 = &UART_RX_producer_fifo_A2;
	coreA_thread_param.UART_TX_consumer_fifo = &UART_TX_consumer_fifo_A;
	coreA_thread_param.Ctrl_consumer_fifo = &Ctrl_consumer_fifo_A;
	coreA_thread_param.thread_canceled = 0;

	ret = pthread_create(&coreA_thread_id, NULL, thread_coreA, (void *)&coreA_thread_param);
	if(ret<0){
		debug_printf("create thread_coreA failed\n");
		exit(0);
	}

	coreB_thread_param.V1_producer_fifo = &V1_producer_fifo;
	coreB_thread_param.V2_producer_fifo = &V2_producer_fifo;
	coreB_thread_param.Img1_producer_fifo = &Img1_producer_fifo;
	coreB_thread_param.Img2_producer_fifo = &Img2_producer_fifo;
	coreB_thread_param.Event_producer_fifo = &Event_producer_fifo_B;
	coreB_thread_param.UART_RX_producer_fifo_1 = &UART_RX_producer_fifo_B1;
	coreB_thread_param.UART_RX_producer_fifo_2 = &UART_RX_producer_fifo_B2;
	coreB_thread_param.UART_TX_consumer_fifo = &UART_TX_consumer_fifo_B;
	coreB_thread_param.Ctrl_consumer_fifo = &Ctrl_consumer_fifo_B;
	coreB_thread_param.thread_canceled = 0;

	ret = pthread_create(&coreB_thread_id, NULL, thread_coreB, (void *)&coreB_thread_param);
	if(ret<0){
		debug_printf("create thread_coreB failed\n");
		exit(0);
	}

	coreC_thread_param.V1_producer_fifo = &V3_producer_fifo;
	coreC_thread_param.V2_producer_fifo = &V4_producer_fifo;
	coreC_thread_param.Img1_producer_fifo = &Img3_producer_fifo;
	coreC_thread_param.Img2_producer_fifo = &Img4_producer_fifo;
	coreC_thread_param.Event_producer_fifo = &Event_producer_fifo_C;
	coreC_thread_param.UART_RX_producer_fifo_1 = &UART_RX_producer_fifo_C1;
	coreC_thread_param.UART_RX_producer_fifo_2 = &UART_RX_producer_fifo_C2;
	coreC_thread_param.UART_TX_consumer_fifo = &UART_TX_consumer_fifo_C;
	coreC_thread_param.Ctrl_consumer_fifo = &Ctrl_consumer_fifo_C;
	coreC_thread_param.thread_canceled = 0;

	ret = pthread_create(&coreC_thread_id, NULL, thread_coreC, (void *)&coreC_thread_param);
	if(ret<0){
		debug_printf("create thread_coreC failed\n");
		exit(0);
	}

	coreD_thread_param.V1_producer_fifo = &V5_producer_fifo;
	coreD_thread_param.V2_producer_fifo = &V6_producer_fifo;
	coreD_thread_param.Img1_producer_fifo = &Img5_producer_fifo;
	coreD_thread_param.Img2_producer_fifo = &Img6_producer_fifo;
	coreD_thread_param.Event_producer_fifo = &Event_producer_fifo_D;
	coreD_thread_param.Ctrl_consumer_fifo = &Ctrl_consumer_fifo_D;
	coreD_thread_param.thread_canceled = 0;

	ret = pthread_create(&coreD_thread_id, NULL, thread_coreD, (void *)&coreD_thread_param);
	if(ret<0){
		debug_printf("create thread_coreD failed\n");
		exit(0);
	}

	localsave_thread_param.Ctrl_consumer_fifo = &Ctrl_consumer_fifo_localsave;
	localsave_thread_param.av_consumer_fifo = &V_consumer_fifo;
	localsave_thread_param.Vmux_store = &Vmux_store_producer_fifo;
	localsave_thread_param.Vmux_live = &Vmux_live_producer_fifo;
	localsave_thread_param.VGA = &VGA_producer_fifo;
	localsave_thread_param.audio = &Audio_producer_fifo;
	localsave_thread_param.V1 = &V1_producer_fifo;
	localsave_thread_param.V2 = &V2_producer_fifo;
	localsave_thread_param.V3 = &V3_producer_fifo;
	localsave_thread_param.V4 = &V4_producer_fifo;
	localsave_thread_param.V5 = &V5_producer_fifo;
	localsave_thread_param.V6 = &V6_producer_fifo;
	localsave_thread_param.thread_canceled = 0;
	localsave_thread_param.sock = -1;
	//localsave_thread_param.IsResouceMode = 0;

	ret = pthread_create(&localsave_thread_id, NULL, thread_localsave, (void *)&localsave_thread_param);
	if(ret<0){
		debug_printf("create thread_localsave failed\n");
		exit(0);
	}

	SwitchTracking_thread_param.Switch_producer_fifo = &Switch_producer_fifo;
	SwitchTracking_thread_param.TX_producer_fifo_A = &UART_TX_producer_fifo_A;
	SwitchTracking_thread_param.TX_producer_fifo_B = &UART_TX_producer_fifo_B;
	SwitchTracking_thread_param.TX_producer_fifo_C = &UART_TX_producer_fifo_C;
	SwitchTracking_thread_param.RX_consumer_fifo_A1 = &UART_RX_consumer_fifo_A1;
	SwitchTracking_thread_param.RX_consumer_fifo_A2 = &UART_RX_consumer_fifo_A2;
	SwitchTracking_thread_param.RX_consumer_fifo_B1 = &UART_RX_consumer_fifo_B1;
	SwitchTracking_thread_param.RX_consumer_fifo_B2 = &UART_RX_consumer_fifo_B2;
	SwitchTracking_thread_param.RX_consumer_fifo_C1 = &UART_RX_consumer_fifo_C1;
	SwitchTracking_thread_param.RX_consumer_fifo_C2 = &UART_RX_consumer_fifo_C2;
	SwitchTracking_thread_param.Event_consumer_fifo = &Event_consumer_fifo;
	SwitchTracking_thread_param.Ctrl_consumer_fifo = &Ctrl_consumer_fifo_SwitchTracking;
	SwitchTracking_thread_param.thread_canceled = 0;

	ret = pthread_create(&SwitchTracking_thread_id, NULL, thread_SwitchTracking, (void *)&SwitchTracking_thread_param);
	if(ret<0){
		debug_printf("create thread_SwitchTracking failed\n");
		exit(0);
	}

	videoanalyze_thread_param.producer_fifo = &Event_producer_fifo_Analyze;
	videoanalyze_thread_param.consumer_fifo = &Filtered_img_consumer_fifo;
	videoanalyze_thread_param.thread_canceled = 0;

	ret = pthread_create(&videoanalyze_thread_id, NULL, thread_analyze, (void *)&videoanalyze_thread_param);
	if(ret<0){
		debug_printf("create thread_analyze failed\n");
		exit(0);
	}

	img_thread_param.JPEG_producer_fifo = &JPEG_producer_fifo;
	img_thread_param.PIP_producer_fifo = &PIP_producer_fifo;
	img_thread_param.filtered_img_producer_fifo = &Filtered_img_producer_fifo;
	img_thread_param.img_consumer_fifo = &Img_consumer_fifo;
	img_thread_param.imgM = &ImgM_producer_fifo;
	img_thread_param.imgV = &ImgV_producer_fifo;
	img_thread_param.img1 = &Img1_producer_fifo;
	img_thread_param.img2 = &Img2_producer_fifo;
	img_thread_param.img3 = &Img3_producer_fifo;
	img_thread_param.img4 = &Img4_producer_fifo;
	img_thread_param.img5 = &Img5_producer_fifo;
	img_thread_param.img6 = &Img6_producer_fifo;
	img_thread_param.thread_canceled = 0;

	ret = pthread_create(&img_thread_id, NULL, thread_img, (void *)&img_thread_param);
	if(ret<0){
		debug_printf("create thread_img failed\n");
		exit(0);
	}

	sleep(1);


	instructor_thread_param.producer_fifo = &Ctrl_producer_fifo;
	instructor_thread_param.consumer_fifo = &Instructor_consumer_fifo;
	instructor_thread_param.coreA_thread_param = &coreA_thread_param;
	instructor_thread_param.coreB_thread_param = &coreB_thread_param;
	instructor_thread_param.coreC_thread_param = &coreC_thread_param;
	instructor_thread_param.coreD_thread_param = &coreD_thread_param;
	instructor_thread_param.thread_listen_param.socket = -1;
	instructor_thread_param.thread_listen_param.port = 3000;
	instructor_thread_param.thread_listen_param.thread_canceled = 0;
	instructor_thread_param.status = RECBC_IDLE;

	//start thread_listen
	ret = pthread_create(&instructorlisten_thread_id, NULL, thread_listen, (void *)&instructor_thread_param.thread_listen_param);
	if(ret<0){
		debug_printf("create thread_listen for instructor failed\n");
		exit(0);
	}

	//start the instructor connection handling in main thread
	//int ret;
	fd_set fdsr;
	struct timeval tv;
	int busy;
	while(1){
		busy = 0;
		//send the outbound message if available
#if 1
		if(datafifo_head(instructor_thread_param.consumer_fifo)){
			if(instructor_thread_param.thread_listen_param.socket>=0){
				ret=send(instructor_thread_param.thread_listen_param.socket, datafifo_head(instructor_thread_param.consumer_fifo)->arg.data, datafifo_head(instructor_thread_param.consumer_fifo)->arg.len, 0);
				if(ret<0){
					close(instructor_thread_param.thread_listen_param.socket);
					instructor_thread_param.thread_listen_param.socket = -1;
				}
			}
			datafifo_consumer_remove_head(instructor_thread_param.consumer_fifo);
			busy++;
		}
#else
		while(1){
			if(datafifo_head(instructor_thread_param.consumer_fifo) == NULL){
				break;
			}
			else{
				//debug_printf("send message from consumer fifo to socket\n");
				if(instructor_thread_param.thread_listen_param.socket>=0){
					ret=send(instructor_thread_param.thread_listen_param.socket, datafifo_head(instructor_thread_param.consumer_fifo)->arg.data, datafifo_head(instructor_thread_param.consumer_fifo)->arg.len, 0);
					if(ret<0){
						close(instructor_thread_param.thread_listen_param.socket);
						instructor_thread_param.thread_listen_param.socket = -1;
					}
				}
				datafifo_consumer_remove_head(instructor_thread_param.consumer_fifo);
			}
		}
#endif
		//check the incoming message

#if 1
		if(instructor_thread_param.thread_listen_param.socket>=0){
			FD_ZERO(&fdsr);
			FD_SET(instructor_thread_param.thread_listen_param.socket, &fdsr);

			int maxfd = instructor_thread_param.thread_listen_param.socket;
			tv.tv_sec = 0;
			tv.tv_usec = 0;
			ret = select(maxfd+1, &fdsr, NULL, NULL, &tv);
			//debug_printf("select return\n");

			if(ret<0){
				debug_printf("instructor select error\n");
				close(instructor_thread_param.thread_listen_param.socket);
				instructor_thread_param.thread_listen_param.socket = -1;
				//break;
			}

			if(ret==0){
				//debug_printf("select return 0\n");
				//break;
			}

			if(FD_ISSET(instructor_thread_param.thread_listen_param.socket, &fdsr)){
				int MsgGot;
				//debug_printf("sock_client readable\n");
				char buf[CMD_BUFSIZE];

				MsgGot = getMsg(instructor_thread_param.thread_listen_param.socket, buf);

				//exception handle
				if(MsgGot<0){
					debug_printf("instructor disconnected to server\n");
					close(instructor_thread_param.thread_listen_param.socket);
					instructor_thread_param.thread_listen_param.socket = -1;
					//break;
				}

				//normal handle
				if(MsgGot == 0){
					//debug_printf("Message from server uncompleted\n");
					usleep(1000);
				}

				if(MsgGot>0){
					debug_printf("Message got from server\n");
					MsgDump(buf);
					//To do: add further processing here
					InstructorMsgHandle(&instructor_thread_param, buf);
					busy++;
				}
			}
		}

#else
		while(1){
			if(instructor_thread_param.thread_listen_param.socket>=0){
				FD_ZERO(&fdsr);
				FD_SET(instructor_thread_param.thread_listen_param.socket, &fdsr);

				int maxfd = instructor_thread_param.thread_listen_param.socket;
				tv.tv_sec = 0;
				tv.tv_usec = 1000;
				ret = select(maxfd+1, &fdsr, NULL, NULL, &tv);
				//debug_printf("select return\n");

				if(ret<0){
					debug_printf("select error\n");
					close(instructor_thread_param.thread_listen_param.socket);
					instructor_thread_param.thread_listen_param.socket = -1;
					break;
				}

				if(ret==0){
					//debug_printf("select return 0\n");
					break;
				}

				if(FD_ISSET(instructor_thread_param.thread_listen_param.socket, &fdsr)){
					int MsgGot;
					//debug_printf("sock_client readable\n");
					char buf[CMD_BUFSIZE];

					MsgGot = getMsg(instructor_thread_param.thread_listen_param.socket, buf);

					//exception handle
					if(MsgGot<0){
						debug_printf("disconnected to server\n");
						close(instructor_thread_param.thread_listen_param.socket);
						instructor_thread_param.thread_listen_param.socket = -1;
						break;
					}

					//normal handle
					if(MsgGot == 0){
						//debug_printf("Message from server uncompleted\n");
						usleep(1000);
					}

					if(MsgGot>0){
						debug_printf("Message got from server\n");
						MsgDump(buf);
						//To do: add further processing here
						InstructorMsgHandle(&instructor_thread_param, buf);
					}
				}
			}
			else{
				usleep(1000);
				break;
			}
		}
#endif

#if 1
		if(!busy){
			usleep(1000);
		}

#else
		usleep(1000);
#endif
	}

	while(!thread_listen_exited(&instructor_thread_param.thread_listen_param)){
		thread_listen_askexit(&instructor_thread_param.thread_listen_param);
	}

	if(instructor_thread_param.thread_listen_param.socket>=0){
		close(instructor_thread_param.thread_listen_param.socket);
		instructor_thread_param.thread_listen_param.socket = -1;
	}
	return 0;
}
