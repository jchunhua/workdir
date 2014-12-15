/*
 * recbc_srv.h
 *
 *  Created on: Mar 18, 2013
 *      Author: spark
 */

#ifndef RECBC_SRV_H_
#define RECBC_SRV_H_

#include "datafifo.h"
#define MAXDEVNUM 100

enum e_VideoSrc
{
	VIDEO_SRC_NONE = -1,
	VIDEO_SRC_MUX,
	VIDEO_SRC_VGA,
	VIDEO_SRC_V1,
	VIDEO_SRC_V2,
	VIDEO_SRC_V3,
	VIDEO_SRC_V4,
	VIDEO_SRC_V5,
	VIDEO_SRC_VAUX1,
	VIDEO_SRC_VAUX2,
};

enum e_CaptureIf
{
	CAPTURE_IF_NONE = -1,
	CAPTURE_IF_VGA,
	CAPTURE_IF_HDSDI,
	CAPTURE_IF_HDMI,
	CAPTURE_IF_CVBS,
};

enum e_AnalyzingMode
{
	ANALYZING_MODE_NONE = -1,
	ANALYZING_MODE_VIDEO,
	ANALYZING_MODE_VGA,
	ANALYZING_MODE_AUX,
};
typedef struct s_videoconfig
{
	enum e_VideoSrc vsrc;
	int Present;
	int CaptureId;
	enum e_CaptureIf capif;
	int CoreId;
	int PortId;
	enum e_AnalyzingMode analymode;
	int UartId;
	int sockfd;
} t_videoconfig;

typedef struct s_idx_id_map
{
	int BrdIdx;
	char BrdId[13];
}t_idx_id_map;

typedef struct s_sock_id_map
{
	int socket;
	char BrdId[13];
}t_sock_id_map;

typedef struct s_dev_cfg
{
	int valid;
	t_sock_id_map BrdTbl[4];
}t_dev_cfg;

typedef struct s_hello_attr
{
	char AttrName[8];
	char AttrValue[64];
}t_hello_attr;


typedef struct s_hello_attrs
{
	int NumofAttr;
	t_hello_attr attrs[3];
}t_hello_attrs;

#define CMD_BUFSIZE 1536
#define VERSION_SIZE 2
#define CMD_SIZE 4
#define DATA_LEN_SIZE 4
#define ARG_NAME_LEN_SIZE 1
#define ARG_LEN_SIZE 4
#define PACKET_HEADER_SIZE (VERSION_SIZE + CMD_SIZE + DATA_LEN_SIZE)
#define COMMAND_VERSION "01"
typedef struct w_cmd
{
	char fullbuf[CMD_BUFSIZE];
	char *version;
	char *cmd;
	char *data_len;
	char *data;

	char *cmd_p;
}t_cmd;

typedef struct s_listen_thread_param
{
	int socket;
	unsigned short port;
	int thread_canceled;
	int senderr;
	int recverr;
}t_listen_thread_param;

typedef struct s_localsave_thread_param
{
	int thread_canceled;
	t_datafifo *av_consumer_fifo;
	t_datafifo *Ctrl_consumer_fifo;
	t_datafifo *audio;
	t_datafifo *Vmux_store;
	t_datafifo *Vmux_live;
	t_datafifo *VGA;
	t_datafifo *V1;
	t_datafifo *V2;
	t_datafifo *V3;
	t_datafifo *V4;
	t_datafifo *V5;
	t_datafifo *V6;
	//int Audio_status;
	int Vstore_status;
	int Vlive_status;
	int VGA_status;
	int V1_status;
	int V2_status;
	int V3_status;
	int V4_status;
	int V5_status;
	int V6_status;
	int sock;
	//int IsResouceMode;
}t_localsave_thread_param;

typedef struct s_SwitchTracking_thread_param
{
	int thread_canceled;
	t_datafifo *TX_producer_fifo_A;
	t_datafifo *TX_producer_fifo_B;
	t_datafifo *TX_producer_fifo_C;
	t_datafifo *Switch_producer_fifo;
	t_datafifo *RX_consumer_fifo_A1;
	t_datafifo *RX_consumer_fifo_A2;
	t_datafifo *RX_consumer_fifo_B1;
	t_datafifo *RX_consumer_fifo_B2;
	t_datafifo *RX_consumer_fifo_C1;
	t_datafifo *RX_consumer_fifo_C2;
	t_datafifo *Event_consumer_fifo;
	t_datafifo *Ctrl_consumer_fifo;
	int teacher_status;
	int student_status;
}t_SwitchTracking_thread_param;

typedef struct s_videoanalyze_thread_param
{
	int thread_canceled;
	t_datafifo *producer_fifo;
	t_datafifo *consumer_fifo;

}t_videoanalyze_thread_param;

typedef struct s_img_thread_param
{
	int thread_canceled;
	t_datafifo *PIP_producer_fifo;
	t_datafifo *JPEG_producer_fifo;
	t_datafifo *filtered_img_producer_fifo;
	t_datafifo *img_consumer_fifo;
	t_datafifo *imgM;
	t_datafifo *imgV;
	t_datafifo *img1;
	t_datafifo *img2;
	t_datafifo *img3;
	t_datafifo *img4;
	t_datafifo *img5;
	t_datafifo *img6;
}t_img_thread_param;

typedef struct s_coreA_thread_param
{
	int thread_canceled;
	t_datafifo *Vstore_producer_fifo;
	t_datafifo *Vlive_producer_fifo;
	t_datafifo *VGA_producer_fifo;
	t_datafifo *Audio_producer_fifo;
	t_datafifo *ImgM_producer_fifo;
	t_datafifo *ImgV_producer_fifo;
	t_datafifo *Event_producer_fifo;
	t_datafifo *UART_RX_producer_fifo_1;
	t_datafifo *UART_RX_producer_fifo_2;
	t_datafifo *Ctrl_consumer_fifo;
	t_datafifo *UART_TX_consumer_fifo;
	t_listen_thread_param thread_listen_param;
}t_coreA_thread_param;

typedef struct s_coreBC_thread_param
{
	int thread_canceled;
	t_datafifo *V1_producer_fifo;
	t_datafifo *V2_producer_fifo;
	t_datafifo *Img1_producer_fifo;
	t_datafifo *Img2_producer_fifo;
	t_datafifo *Event_producer_fifo;
	t_datafifo *UART_RX_producer_fifo_1;
	t_datafifo *UART_RX_producer_fifo_2;
	t_datafifo *Ctrl_consumer_fifo;
	t_datafifo *UART_TX_consumer_fifo;
	t_listen_thread_param thread_listen_param;
}t_coreBC_thread_param;

typedef struct s_coreD_thread_param
{
	int thread_canceled;
	t_datafifo *V1_producer_fifo;
	t_datafifo *V2_producer_fifo;
	t_datafifo *Img1_producer_fifo;
	t_datafifo *Img2_producer_fifo;
	t_datafifo *Event_producer_fifo;
	t_datafifo *Ctrl_consumer_fifo;
	t_listen_thread_param thread_listen_param;
}t_coreD_thread_param;

typedef struct s_instructor_tread_param
{
	t_datafifo *producer_fifo;
	t_datafifo *consumer_fifo;
	t_listen_thread_param thread_listen_param;
	t_coreA_thread_param *coreA_thread_param;
	t_coreBC_thread_param *coreB_thread_param;
	t_coreBC_thread_param *coreC_thread_param;
	t_coreD_thread_param *coreD_thread_param;
	int status;
}t_instructor_thread_param;

typedef struct s_DetectResult
{
	unsigned int detected;
	unsigned int avg_top;
	unsigned int avg_left;
	unsigned int avg_bottom;
	unsigned int avg_right;
	unsigned int max_top;
	unsigned int max_left;
	unsigned int max_bottom;
	unsigned int max_right;
}t_DetectResult;

#define VINBUF_SIZE 1024*1024
#define VINBUF_CNT 3

#define AINBUF_SIZE 1024
#define AINBUF_CNT 10

#define EINBUF_SIZE 1536
#define EINBUF_CNT 10

#define BYPBUF_SIZE 8
#define	BYPBUF_CNT 200

#define IMGBUF_SIZE 72*1024
#define IMGBUF_CNT 3

typedef enum
{
	FRMTYPE_VIDEO_IDR,
	FRMTYPE_VIDEO_I,
	FRMTYPE_VIDEO_P,
	FRMTYPE_AUDIO
}e_frame_type;

typedef struct s_frame_info
{
	unsigned int frame_type;
	unsigned int frame_length;
	unsigned int frame_num;
	unsigned int packet_num;
	unsigned int frame_rate;
	unsigned int time_stamp;
	unsigned int pkts_in_frame;
}t_frame_info;

#define MAX_AVPKT_SIZE 1024

typedef enum
{
	LOCALSAVE_IDLE = 0,
	LOCALSAVE_START,
	LOCALSAVE_WORKING,
	LOCALSAVE_PAUSE,
	LOCALSAVE_RESUME,
	LOCALSAVE_STOP,
}e_localsave_status;

typedef enum
{
	RECBC_IDLE = 0,
	RECBC_WORKING,
}e_recbc_status;

typedef struct s_mp4format
{
	char filename[256];
	char fps[16];
	int has_audio;
}t_mp4format;

typedef enum
{
	VIDEO_FUNCTION_VGA = 0,
	VIDEO_FUNCTION_TEACHER_CLOSEUP,
	VIDEO_FUNCTION_STUDENT_CLOSEUP,
	VIDEO_FUNCTION_TEACHER_PANORAMA,
	VIDEO_FUNCTION_STUDENT_PANORAMA,
	VIDEO_FUNCTION_BOARD_CLOSEUP,
}e_video_function;

typedef struct s_video_map
{
	int index; //array index
	int function; //1:teacher_closeup; 2:student_closeup; 3:teacher_panorama; 4:student_panorama; 5:board_closeup; 6:cvbs
	//int analyze_type;
	int uart_channel; //1:A1; 2:A2; 3:B1; 4:B2; 5:C1; 6:C2
	int pip; //0:not used by PIP; 1:used by PIP
	int cap_id; //1~5:SDI1~5; 6~10:HDMI1~5; 11~15:CVBS1~5
	char cam_vendor[32];
	char cam_model[32];
}t_video_map;

typedef struct s_recbc_cfg
{
	int frame_width;
	int frame_height;
	float frame_rate;
	int bitrate;
	int live_bitrate;
	int IsResouceMode;
	int num_of_CoreBoard;
	int IsAutoInstuct;
	int hasA;
	int hasB;
	int hasC;
	int hasD;
	char PolicyFileName[64];
	int VGADuration;
}t_recbc_cfg;

typedef struct s_preset_pos
{
	int pan_pos;
	int tilt_pos;
	int zoom_pos;
}t_preset_pos;

typedef struct s_camera_holder
{
	int uart_channel;
	char Rx_buf[16];
	char *Rx_buf_p;
	unsigned int RspLen;
	int command_pending;
	int PanPos;
	int TiltPos;
	int ZoomPos;
	int target_PanPos;
	int target_TiltPos;
	int target_ZoomPos;
	int pan_min;
	int pan_max;
	int tilt_min;
	int tilt_max;
	int zoom_min;
	int zoom_max;
	int panspeed_use;
	int tiltspeed_use;
	float view_angle_min;
	float view_angle_max;
	float pan_angle_left;
	float pan_angle_right;
	float tilt_angle_up;
	float tilt_angle_down;
	struct timeval tv;

	//float zoom_range; //example: 18x
	t_preset_pos preset[16];
}t_camera_holder;

#define CH_COMPLETION_PENDING 1
#define CH_PTINQ_PENDING (1<<1)
#define CH_ZOOMINQ_PENDING (1<<2)
#define CH_ERROR (1<<31)

#define LINK_READY 1
#define LINK_NOTREADY 0

typedef enum
{
	COND_VGA_UNCHG = 0,
	COND_VGA_CHG,
	COND_TEACHER_DETECTED_NONE,
	COND_TEACHER_DETECTED_ONE,
	COND_TEACHER_DETECTED_MULTIPLE,
	COND_STUDENT_STAND_NONE,
	COND_STUDENT_STAND_ONE,
	COND_STUDENT_STAND_MULTIPLE,
	COND_BOARD_NOTWRITE,
	COND_BOARD_WRITE,

	COND_COMBINATION_1,
	COND_COMBINATION_2,
	COND_COMBINATION_3,
	COND_COMBINATION_4,
	COND_COMBINATION_5,
	COND_COMBINATION_6,
	COND_COMBINATION_7,
	COND_COMBINATION_8,
	COND_COMBINATION_9,
	COND_COMBINATION_10,
	COND_COMBINATION_11,
	COND_COMBINATION_12,
	COND_COMBINATION_13,
	COND_COMBINATION_14,
	COND_COMBINATION_15,
	COND_COMBINATION_16,
	COND_COMBINATION_17,
	COND_COMBINATION_18,
	COND_COMBINATION_19,
	COND_COMBINATION_20,
	COND_COMBINATION_21,
	COND_COMBINATION_22,
	COND_COMBINATION_23,
	COND_COMBINATION_24,
	COND_COMBINATION_25,
	COND_COMBINATION_26,
	COND_COMBINATION_27,
	COND_COMBINATION_28,
	COND_COMBINATION_29,
	COND_COMBINATION_30,
	COND_COMBINATION_31,
	COND_COMBINATION_32,
	COND_COMBINATION_33,
	COND_COMBINATION_34,
	COND_COMBINATION_35,
	COND_COMBINATION_36,
	COND_MAX,
}e_cond_idx;

#define MAX_COND_NAME_LENGTH 16
#define MAX_ELEMENT_NUM 4
typedef struct s_cond_item
{
	int valid;
	int cond_idx;
	int value;
	//char name[MAX_COND_NAME_LENGTH];
	int cond_type; //0:element; 1:combined
	int num_of_combined_element; //only for combined type conditions
	struct s_cond_item *element[MAX_ELEMENT_NUM]; //only for combined type conditions
}t_cond_item;

//#define MAX_COND_NUM 16
typedef struct s_truth_table
{
	int item_num;
	t_cond_item cond[COND_MAX];
}t_truth_table;

typedef enum
{
	FSM_NOTRANSITION = -1,
	FSM_VGA,
	FSM_TEACHER_CLOSEUP,
	FSM_STUDENT_CLOSEUP,
	FSM_TEACHER_PANORAMA,
	FSM_STUDENT_PANORAMA,
	FSM_BOARD_CLOSEUP,
	FSM_VGA_PIP_TEACHER_CLOSEUP,
	FSM_TEACHER_CLOSEUP_PIP_STUDENT_CLOSEUP,
	FSM_BLACKBOARD_PIP_STUDENT_CLOSEUP,
	FSM_STATE_MAX,
}e_fsm_state;

typedef struct s_transition_table
{
	char item[FSM_STATE_MAX][COND_MAX];
}t_transition_table;

typedef struct s_finite_state_machine
{
	char initial_state;
	char current_state;
	t_truth_table *truth_table;
	t_transition_table *transition_table;
}t_finite_state_machine;

typedef struct s_VGA_result
{
	int changed;
}t_VGA_result;

typedef struct s_camera_geometry
{
	int x;
	int y;
	int z;
	float angle_h;
	float angle_v;
	float view_angle;
}t_camera_geometry;

typedef struct s_area
{
	int left;
	int front;
	int right;
	int rear;
}t_area;

typedef struct s_location
{
	int x;
	int y;
}t_location;

typedef struct s_aux_camera_param
{
	unsigned int target_num;
	int max_top;
	int max_left;
	int max_bottom;
	int max_right;
}t_aux_camera_param;

typedef struct s_aux_result
{
	t_aux_camera_param aux[2];
	int student_in_standing;
}t_aux_result;

typedef struct s_student_tracking_geometry
{
	t_camera_geometry camera_geometry[3];
	t_area student_area;
	t_location student_location;
	t_location last_location;
	float target_pan_angle;
	float target_tilt_angle;
	float target_view_angle;
	int dotracking;
}t_student_tracking_geometry;

typedef struct s_screen_geometry
{
	int has_screen;
	int x_left;
	int x_right;
	int y;
	float camera_initial_angle;
}t_screen_geometry;

typedef struct s_internal_abbr_cmd
{
	char cmd[8];
	char para1[2];
	char para2[2];
	char para3[2];
	char para4[2];
	char intvalue[4];
}t_internal_abbr_cmd;

typedef struct s_forward_table
{
	int teacherspecial;
	int studentspecial;
	int teacherfull;
	int studentfull;
	int blackboard;
}t_forward_table;

#endif /* RECBC_SRV_H_ */
