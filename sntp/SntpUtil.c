
#include "mysntp.h"
#include <linux/time.h>
#include <stdio.h>

/**************************************************************************
 *
 * 函数功能: 得到系统当前时间.
 *
 * 参数说明: [OUT] current, SNTP 时间.
 *
 * 返 回 值: 成功返回 0, 失败返回 -1.
 *
 **************************************************************************/

int SntpGetSysTime(tTS *ptCurrent)
{
	struct timeval tNowTV;
	tTS tNowTS;
	
	if (-1 == gettimeofday(&tNowTV, NULL))
	{
		printf("error:gettimeofday().\n");
		return -1;
	}
	
	printf("tNowTV.tv_sec: %ld\n", tNowTV.tv_sec);
	printf("tNowTV.tv_usec: %ld\n", tNowTV.tv_usec);

//	ptCurrent = &tNowTS;
	ptCurrent->intg.ui = tNowTV.tv_sec;
	ptCurrent->frag.uf = tNowTV.tv_usec;

    return 0;
}
