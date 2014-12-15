/*
 * streaming_srv.h
 *
 *  Created on: Apr 25, 2013
 *      Author: spark
 */

#include "datafifo.h"

#ifndef STREAMING_SRV_H_
#define STREAMING_SRV_H_

#define MAX_DATA_SRC_NUM 1
#define MAX_CLIENT_NUM 5
#define MAX_FRAME_BUFFERED 60
#define STREAMING_PACKET_HEADER_LENGTH 16

typedef struct s_datasink
{
	//int thread_canceled;
	t_datafifo consumer_fifo;
	unsigned int timestamp;
	void * p_datasrc_ctx;
	void * p_mainstreaming_ctx;
}t_datasink;

typedef struct s_clientstreaming
{
	//int thread_canceled;
	t_datafifo *p_consumer_fifo;
	t_datasink *p_datasink;
	int sock;
	void * p_mainstreaming_ctx;
}t_clientstreaming;

typedef struct s_dfpool_item
{
	int allocated;
	t_datafifo df;
}t_fifopool_item;

#if 0
typedef struct s_streamingclient
{
	int active;
	t_clientstreaming *p_clientstreaming;
}t_streamingclient;
#endif

typedef struct s_datasrc_ctx
{
	int valid;
	int refcnt;

	int sock_datasrc;	//this should be valid before starting any other handling related to this context.
						//if it is invalid, nothing should be done with this context except releasing it
	char src_id[16];
	t_datafifo producer_fifo;
	t_datasink datasink;
	//t_streamingclient clients[MAX_CLIENT_NUM];
	char *databuf;
	char *data_p;
	//int bufstatus;
}t_datasrc_ctx;

typedef struct s_fifopool
{
	pthread_mutex_t mutex;
	t_fifopool_item client_fifo_item[MAX_CLIENT_NUM];
}t_fifopool;

typedef struct s_datasrc_ctx_pool
{
	pthread_mutex_t mutex;
	t_datasrc_ctx datasrc_ctx[MAX_DATA_SRC_NUM];
}t_datasrc_ctx_pool;

typedef struct s_mainstreaming_ctx
{
	int sock_src_listen;
	int sock_client_listen;
	t_datasrc_ctx_pool datasrc_ctx_pool;
	t_fifopool client_fifo_pool;
	int max_fd;
}t_mainstreaming_ctx;

#define datasrc_ctx_pool_lock(datasrc_ctx_pool) pthread_mutex_lock(&(datasrc_ctx_pool)->mutex)
#define datasrc_ctx_pool_unlock(datasrc_ctx_pool) pthread_mutex_unlock(&(datasrc_ctx_pool)->mutex)

typedef struct s_streaming_packet_header
{
	unsigned int size;
	unsigned int type;
	unsigned int flag;
	unsigned int timestamp;
}t_streaming_packet_header;

#if 0
typedef enum
{
	FRMTYPE_VIDEO_IDR,
	FRMTYPE_VIDEO_I,
	FRMTYPE_VIDEO_P,
	FRMTYPE_AUDIO
}e_frame_type;
#endif

typedef enum
{
	PACKET_TYPE_ID,
	PACKET_TYPE_VIDEO,
	PACKET_TYPE_AUDIO,
}e_packet_type;

#endif /* STREAMING_SRV_H_ */
