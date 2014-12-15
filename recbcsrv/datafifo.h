/*
 * netsock.h
 *
 *  Created on: Jan 15, 2013
 *      Author: Spark
 */
#ifndef DATAFIFO_H_
#define DATAFIFO_H_

#define DATAFIFO_ITEM_STATUS_INIT 0
#define DATAFIFO_ITEM_STATUS_FILLING 1
#define DATAFIFO_ITEM_STATUS_READY 2
#define DATAFIFO_ITEM_STATUS_DOING 3

#define FRAME_FLAG_NEWFRAME 0x1
#define FRAME_FLAG_FRAMEEND 0x2
#define FRAME_FLAG_FIRSTFRAME 0x4
#define FRAME_FLAG_KEYFRAME 0x8
#define FRAME_FLAG_AUDIOFRM 0x10
#define FRAME_FLAG_USERDATA 0x20
#define BIT31 0x80000000

//item for linklist, shared by both producer and consumer's linklist
typedef struct s_datafifo_item
{
	struct s_datafifo_item * next;
	struct s_datafifo_item * prev;
	struct s_datafifo * datafifo; //pointer to which datafifo this item belongs to.

	struct s_datafifo_item * ref; //ref to producer's item, used by consumer only
	int refcnt; //be referenced count, used in producer only.

	//to find unused items in datafifo->items. status==0 means unused.
	//also in producer, indicate the data/len status.
	int status;

	struct{
		unsigned char * data;
		int len;
		int flags;
		int timestamp;
	}arg;
}t_datafifo_item;

typedef struct s_datafifo_con
{
	struct s_datafifo * datafifo;
	struct s_datafifo_con * next;
	struct s_datafifo_con * prev;
}t_datafifo_con;

//linklist for both producer and consumer
typedef struct s_datafifo
{
	unsigned int data_maxlen;

	t_datafifo_item * items; //all items pool, array.
	unsigned int item_cnt; //pool size

	t_datafifo_item * listhead; //linklist, fifo.
	t_datafifo_item * listtail;

	pthread_mutex_t mutex;
	pthread_cond_t cond;
	pthread_cond_t full_cond;

	struct s_datafifo_con * consumers; //consumers list
}t_datafifo;

int datafifo_init(t_datafifo *df, int size, int cnt);
int datafifo_connect(t_datafifo *producer, t_datafifo * consumer);
int datafifo_disconnect(t_datafifo * producer, t_datafifo * consumer);

int datafifo_producer_data_add(t_datafifo * df, unsigned char *data, int len, int flags, int timestamp);

int datafifo_consumer_remove_head(t_datafifo *df);
void datafifo_consumer_clear(t_datafifo *df);

#define datafifo_lock(datafifo) pthread_mutex_lock(& (datafifo)->mutex)
#define datafifo_unlock(datafifo) pthread_mutex_unlock(& (datafifo)->mutex)
#define datafifo_signal(datafifo) pthread_cond_broadcast(& (datafifo)->cond);

#define datafifo_wait(datafifo) \
do{ \
	pthread_mutex_lock(& (datafifo)->mutex); \
	if(datafifo_head(datafifo) != NULL && datafifo_head(datafifo)->status == DATAFIFO_ITEM_STATUS_READY){ \
		pthread_mutex_unlock(& (datafifo)->mutex); \
		break; \
	} \
	pthread_cond_wait(& (datafifo)->cond, & (datafifo)->mutex); \
	pthread_mutex_unlock(& (datafifo)->mutex); \
}while(0)

#define datafifo_head(datafifo) (datafifo)->listhead


#endif
