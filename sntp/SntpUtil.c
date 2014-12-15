
#include "mysntp.h"
#include <linux/time.h>
#include <stdio.h>

/**************************************************************************
 *
 * ��������: �õ�ϵͳ��ǰʱ��.
 *
 * ����˵��: [OUT] current, SNTP ʱ��.
 *
 * �� �� ֵ: �ɹ����� 0, ʧ�ܷ��� -1.
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
