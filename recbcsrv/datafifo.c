/*
 * netsock.c
 *
 *  Created on: Jan 15, 2013
 *      Author: Spark Yang
 */

#include "rt_generic.h"
#include "datafifo.h"

//#define DATAFIFO_DEBUG

/* frame operations */
static t_datafifo_item * datafifo_alloc(t_datafifo *df)
{
	int i;
	//t_datafifo_item * cur = NULL;

	pthread_mutex_lock(&df->mutex);
	while(1){
		for(i=0; i < df->item_cnt; i++){
			if(df->items[i].status == DATAFIFO_ITEM_STATUS_INIT){
				df->items[i].arg.flags = 0;
				df->items[i].arg.len = 0;
				df->items[i].next = NULL;
				df->items[i].prev = NULL;
				df->items[i].ref = NULL;
				df->items[i].refcnt = 0;

				df->items[i].status = DATAFIFO_ITEM_STATUS_FILLING;
				pthread_mutex_unlock(&df->mutex);
				return &df->items[i];
			}
		}
		//full, wait for signal
		pthread_cond_wait(&df->full_cond, &df->mutex);
	}
	//Should never happen. Just eliminate warnings
	return NULL;
}

/* link list operations */
//add to list tail
static void datafifo_list_addtail(t_datafifo *df, t_datafifo_item * f)
{
	pthread_mutex_lock(&df->mutex);

	if(df->listhead == NULL){
		df->listhead = f;
		df->listtail = f;
	}else{
		df->listtail->next = f;
		f->prev = df->listtail ;
		df->listtail = f;
	}

	pthread_cond_broadcast(&df->cond);
	pthread_mutex_unlock(&df->mutex);
}

//remove from list, and also free it
//NOTE: need caller to lock the mutex.
static void datafifo_remove_head_internal_reqlock(struct s_datafifo *df)
{
	t_datafifo_item * orig = df->listhead;

	df->listhead = orig->next;

	if(df->listhead == NULL){
		//listhead empty
		df->listtail = NULL;
	}else{
		df->listhead->prev = NULL;
	}

	//reset other variables when alloc
	orig->status = DATAFIFO_ITEM_STATUS_INIT;

	pthread_cond_broadcast(&df->full_cond);
}

static int datafifo_producer_remove_check(t_datafifo * df)
{
	pthread_mutex_lock(&df->mutex);
	while(1){
		if(! df->listhead){
			break;
		}
		if(df->listhead->status != DATAFIFO_ITEM_STATUS_DOING){
			break;
		}
		if(df->listhead->refcnt != 0){
			break;
		}
#ifdef DATAFIFO_DEBUG
		printf("P-");
#endif
		//maybe consumer has used this, so need to check it here
		datafifo_remove_head_internal_reqlock(df);
	}
	pthread_mutex_unlock(&df->mutex);

	return 0;
}

static int datafifo_addto_consumers(t_datafifo * df, t_datafifo_item *item)
{
	t_datafifo_con * con = df->consumers;

	while(con){
		t_datafifo_item * cur = datafifo_alloc(con->datafifo);

		//add to consumer's link list
		cur->ref = item;
		cur->next = NULL;
		cur->prev = NULL;
		cur->arg.data = &item->arg.data[0];
		cur->arg.len = item->arg.len;
		cur->arg.flags = item->arg.flags;
		cur->arg.timestamp = item->arg.timestamp;
		cur->status = DATAFIFO_ITEM_STATUS_READY;

#ifdef DATAFIFO_DEBUG
		printf("C+:%x:%x\n", con, item);
#endif

		pthread_mutex_lock(&df->mutex);
		item->refcnt ++;
		pthread_mutex_unlock(&df->mutex);

		datafifo_list_addtail(con->datafifo, cur);

		//go through all consumers
		con = con->next;
	}

	//set status to DOING at last, to prevent consumer's remove it.
	item->status = DATAFIFO_ITEM_STATUS_DOING;

	//need to check here, because maybe consumers has used them up
	//before set status to DOING. so remove here.
	datafifo_producer_remove_check(df);

	return 0;
}

int datafifo_producer_data_add(t_datafifo * df, unsigned char *data, int len, int flags, int timestamp)
{
	if(flags & FRAME_FLAG_NEWFRAME){
		t_datafifo_item * cur = datafifo_alloc(df);

		//check if last frame exist and not ready, then set it to ready
		t_datafifo_item * old_item = NULL;

		pthread_mutex_lock(&df->mutex);
		if(df->listtail){
			if(df->listtail->status == DATAFIFO_ITEM_STATUS_FILLING){
				if(flags & FRAME_FLAG_FIRSTFRAME){
					//if new stream come, discard old un-completed frame by set len to 0.
					df->listtail->arg.len = 0;
				}
				df->listtail->status = DATAFIFO_ITEM_STATUS_READY;
				old_item = df->listtail ;
			}
		}
		pthread_mutex_unlock(&df->mutex);

		if(old_item){
			//add old item to consumer's linklist
			//debug_printf("timestamp:%d", old_item->arg.timestamp);
			datafifo_addto_consumers(df, old_item);
			//usleep(1000);
		}

		//set new frame flag
		if(flags & FRAME_FLAG_FIRSTFRAME){
			cur->arg.flags |= FRAME_FLAG_FIRSTFRAME;
		}
		if(flags & FRAME_FLAG_KEYFRAME){
			cur->arg.flags |= FRAME_FLAG_KEYFRAME;
		}
		
		//set timestamp
		cur->arg.timestamp=timestamp;
		//debug_printf("timestamp:%d", timestamp);
		//add new frame to tail
#ifdef DATAFIFO_DEBUG
		printf("P+");
#endif
		datafifo_list_addtail(df, cur);
	}else{
		if(df->listtail->status != DATAFIFO_ITEM_STATUS_FILLING){
			printf("not first, but not filling\n");
			return -4;
		}
	}

	//memcpy don't need to lock, because 'status' can guarantee tail will not changed.
	if(df->listtail->arg.len + len > df->data_maxlen){
		return -5;
	}

	memcpy(df->listtail->arg.data + df->listtail->arg.len, data, len);
	df->listtail->arg.len += len;
	//debug_printf("timestamp:%d", df->listtail->arg.timestamp);
	//if frame end
	if(flags & FRAME_FLAG_FRAMEEND){
		df->listtail->status = DATAFIFO_ITEM_STATUS_READY;
		datafifo_addto_consumers(df, df->listtail);
	}

	return 0;
}

int datafifo_init(t_datafifo *df, int size, int cnt)
{
	int item_i;

	memset(df, 0, sizeof(t_datafifo));
	df->data_maxlen = size;
	df->item_cnt = cnt;

	df->items = (t_datafifo_item * )malloc(sizeof(t_datafifo_item) * cnt);
	if(df->items == NULL){
		printf("alloc datafifo failed\n");
		return -1;
	}
	memset(df->items, 0, sizeof(t_datafifo_item) * cnt);

	for(item_i = 0; item_i < cnt; item_i ++){
		if(size){
			df->items[item_i].arg.data = (unsigned char *)malloc(size + 8 /*FF_INPUT_BUFFER_PADDING_SIZE*/ + 4);
			if(df->items[item_i].arg.data == NULL){
				printf("alloc datafifo item failed\n");
				return -2;
			}
			memset(df->items[item_i].arg.data + size, 0, 8);
		}
		df->items[item_i].status = DATAFIFO_ITEM_STATUS_INIT;
		df->items[item_i].datafifo = df;
	}

	pthread_mutex_init(&df->mutex, NULL);
	pthread_cond_init(&df->cond, NULL);
	pthread_cond_init(&df->full_cond, NULL);
	return 0;
}

//attach: attach the consumer to the producer.
int datafifo_connect(t_datafifo * producer, t_datafifo * consumer)
{
	t_datafifo_con * con = malloc(sizeof(t_datafifo_con));
	if(con == NULL){
		printf("datafifo_con malloc failed\n");
		return -1;
	}
	con->datafifo = consumer;

	pthread_mutex_lock(&producer->mutex);

	con->next = producer->consumers;
	con->prev = NULL;
	if(producer->consumers != NULL){
		producer->consumers->prev = con;
	}
	producer->consumers = con;

	pthread_mutex_unlock(&producer->mutex);

	return 0;
}

int datafifo_disconnect(t_datafifo * producer, t_datafifo * consumer)
{
	pthread_mutex_lock(&producer->mutex);

	t_datafifo_con * tmp = producer->consumers;
	while(tmp){
		if(tmp->datafifo == consumer){
			break;
		}
		tmp = tmp->next;
	}

	if(tmp == NULL){
		printf("datafifo disconnect not found\n");
		return -1;
	}

	//unlink from consumer list
	if(tmp->prev == NULL){
		producer->consumers = tmp->next;
	}else{
		tmp->prev->next = tmp->next;
	}
	if(tmp->next != NULL){
		tmp->next->prev = tmp->prev;
	}

	free(tmp);

	pthread_mutex_unlock(&producer->mutex);

	return 0;
}

int datafifo_consumer_remove_head(t_datafifo *df)
{
#ifdef DATAFIFO_DEBUG
	printf("C-:%x:%x\n", df, df->listhead->ref);
#endif

	if(df->listhead == NULL){
		return -1;
	}

	t_datafifo * producer = NULL;
	if(df->listhead->ref){
		producer = df->listhead->ref->datafifo;

		//decrease producer item's ref count
		pthread_mutex_lock(&producer->mutex);
		df->listhead->ref->refcnt --;
		pthread_mutex_unlock(&producer->mutex);

		if(producer){
			//check if need to remove from producer
			datafifo_producer_remove_check(producer);
		}
	}else{
		printf("consumer remove without ref\n");
	}

	//remove from consumer's link list
	pthread_mutex_lock(&df->mutex);
	datafifo_remove_head_internal_reqlock(df);
	pthread_mutex_unlock(&df->mutex);

	return 0;
}

void datafifo_consumer_clear(t_datafifo *df)
{
	while(datafifo_consumer_remove_head(df) == 0);
}

void datafifo_print(t_datafifo *df)
{
#ifdef DATAFIFO_DEBUG
	int i;
	printf("Datafifo:%08x, items:%d\n", df, df->item_cnt);
	for(i=0; i < df->item_cnt; i++){
		printf("  Item%d: %08x,%d,%08x,%d\n", i, &df->items[i], df->items[i].status, df->items[i].ref, df->items[i].refcnt);
	}
	printf(" ListHead:\n");
	t_datafifo_item * tmp = df->listhead;
	while(tmp){
		printf("  %08x:\n", tmp);
		tmp = tmp->next;
	}
	printf(" Mutex: %d\n", df->mutex.value);
	printf(" Cond: %d\n", df->cond.value);
	printf(" FullCond: %d\n", df->full_cond.value);
#endif
}
