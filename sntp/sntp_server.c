/* *******************��׼���Ǳ�׼��******************** */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/errno.h>
//#include <linux/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>
#include <errno.h>

#include "mysntp.h"



/**************************************************************************
 *
 * ��������: ����ʱ����Ϣ.
 *
 * ����˵��: [IN] sock, socket ���;
 *           [IN] dest, Ŀ�ĵ�ַ;
 *           [IN] xmt,  �ͻ��˵Ĵ���ʱ��, �����ֽ���;
 *           [IN] recv, �������ݵ�ʱ��, �����ֽ���, xmt != NULL ʱ��ʹ��;
 *
 * �� �� ֵ: �ɹ����ط��͵��ֽ���, ʧ�ܷ��ش�����.
 *
 **************************************************************************/
static int sntp_send_packet(int s, struct sockaddr_in *tDestAddr, 
                            tTS *tOrig, tTS *tRecv)
{
    int iRes;
    unsigned char mode = MODE_BROADCAST;
    struct sntp_s tSntpMsg;

    memset(&tSntpMsg, 0, sizeof(tSntpMsg));
	printf("h002tSntpMsg.tRecv.intg.ui %u\n", tRecv->intg.ui);
	printf("h002tSntpMsg.tRecv.frag.uf %u\n", tRecv->frag.uf);
	
    /* ���յ��ͻ�������, �����˴���ʱ��� */
    if (tOrig)
    {
        mode = MODE_SERVER;
      //  tSntpMsg.tOrig = *tOrig;	/* �����ֽ��� */
		HTONL_FP(tOrig, &tSntpMsg.tOrig);	/* �����ֽ��� */
        HTONL_FP(tRecv, &tSntpMsg.tRecv);	/* �����ֽ��� */

		printf("h001tSntpMsg.tOrig.intg.ui %u\n", tOrig->intg.ui);
		printf("h001tSntpMsg.tOrig.frag.uf %u\n", tOrig->frag.uf);
    }

    tSntpMsg.ucLiVnMode= MSG_LI_VN_MODE(LEAP_NOT_SYNC, 
                                    SNTP_VERSION, mode);
    tSntpMsg.ucStratum = SNTP_STRATUM;
    
    SntpGetSysTime(&tSntpMsg.tXmt);
	system("date");
    printf("[SNTP] Transmit Timestamp: %u : %u\n", tSntpMsg.tXmt.intg.ui, 
            tSntpMsg.tXmt.frag.uf);
    HTONL_FP(&tSntpMsg.tXmt, &tSntpMsg.tXmt);	/* �����ֽ��� */

    iRes = sendto(s, (char *)&tSntpMsg, sizeof(tSntpMsg), 0,
                    (struct sockaddr *)tDestAddr, sizeof(struct sockaddr_in));
	if (-1 == iRes)
	{
		perror("[SNTP]--sendto()");
	}

	printf("================================================\n");
    return iRes;
}


int main(int argc, char **argv)
{
	int s = 0;        /* ��������socket��� */
    struct sockaddr_in tLocalAddr, tDestAddr; /* ������ַ */
    int iRes = 0;
    int iAddrLen = sizeof(tLocalAddr);
    struct sntp_s tSntpMsg;
    fd_set tReadSet, tRead;
    tTS *tOrig = NULL, tRecv;
    struct timeval tTimeOut;
	
    s = socket(AF_INET, SOCK_DGRAM, 0);/* ���� SNTP ��socket */

    tLocalAddr.sin_family = AF_INET;
    tLocalAddr.sin_port = htons((u_short)123);
    tLocalAddr.sin_addr.s_addr = INADDR_ANY;
    
    iRes = bind(s, (struct sockaddr *)&tLocalAddr, sizeof(tLocalAddr));
    if (iRes == -1)
    {
		perror("[SNTP] -- bind()");
        close(s);
        return -1;
    }

    FD_ZERO(&tRead);
    FD_SET(s, &tRead);

	printf("\n------------------------------------------------\n");
    printf("[SNTP] server is running ... ...                  \n");
	printf("------------------------------------------------  \n");
    while (1)
    {
    	memset(&tSntpMsg, 0, sizeof(tSntpMsg));
        tDestAddr.sin_family = AF_INET;
        tDestAddr.sin_port = htons(5123);
        tDestAddr.sin_addr.s_addr = inet_addr(SNTP_MCAST_ADDR);

        tReadSet = tRead;
        tOrig = NULL;
		tTimeOut.tv_sec  = 120;
		tTimeOut.tv_usec = 0;
        iRes = select(FD_SETSIZE, &tReadSet, NULL, NULL, &tTimeOut);
		
        if (iRes == -1)
        {
        	perror("[SNTP]--select()");
			continue;
        }
		if (iRes > 0)
        {
            if (!FD_ISSET(s, &tReadSet))
                continue;

            SntpGetSysTime(&tRecv); /* �õ���������ʱ��ʱ�� */
			
            iRes = recvfrom(s, (char *)&tSntpMsg, 48,
                              0, (struct sockaddr *)&tDestAddr, &iAddrLen);
			printf("n000tSntpMsg.tOrig.intg.ui %u\n", tSntpMsg.tOrig.intg.ui);
			printf("n000tSntpMsg.tOrig.frag.ui %u\n", tSntpMsg.tOrig.frag.uf);
            if (iRes == -1)
            {
                perror("[SNTP] recvfrom()");
                continue;
            }


//            if ((iRes < sizeof(tSntpMsg)) || 
//                (MSG_MODE(tSntpMsg.ucLiVnMode) != MODE_CLIENT))
//            {
//            	printf("Recv Size: %d\n", iRes);
//				
//				printf("Mode is: %d\n"�� MSG_MODE(tSntpMsg.ucLiVnMode));
//                printf("[SNTP] wrong packet\n");
//                continue;
//            }

            tOrig = &tSntpMsg.tOrig;	/* �����ֽ��� */
			NTOHL_FP(&tSntpMsg.tOrig, tOrig);	/* �����ֽ��� */
            printf("[SNTP] request from : %s\n", inet_ntoa(tDestAddr.sin_addr));
        }
		if(iRes == 0)
		{
			printf("time out\n");
			perror("select");
		}

        /* else is timeout, distribute the time too. */
        iRes = sntp_send_packet(s, &tDestAddr, tOrig, &tRecv);
    }

    close(s);

    return 0;
}

