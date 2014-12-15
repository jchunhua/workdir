/* 
 *
 *
 *
 *
 *
 */


/********************* ����ȫ�ֱ��� **********************/
#if SNTP_IPV6_SUPPORT
static struct in6_addr g_ipv6BroadCastAddr = {{{0xff, 0xff, 0xff, 0xff, 
                                                0x00, 0x00, 0x00, 0x00, 
                                                0x00, 0x00, 0x00, 0x00, 
									            0x00, 0x00, 0x00, 0x00}}};
static struct in6_addr g_SntpIn6AnyAddr    = {{{0x00, 0x00, 0x00, 0x00, 
                                                0x00, 0x00, 0x00, 0x00, 
                                                0x00, 0x00, 0x00, 0x00, 
												0x00, 0x00, 0x00, 0x00}}};
#endif

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
 * ��������: ����ӷ��������յ�����Ϣ.
 *
 * ����˵��:	[IN] sntp_msg, SNTP ��Ϣ;
 *      		[IN] msg_len, ��Ϣ����������;
 *   			[IN] dest_t,  ����ʱ��;
 *
 * �� �� ֵ: �ɹ����� 0, ʧ�ܷ��� -1.
 *
 **************************************************************************/
static int SntpRecvHandle(struct sntp_s *tSntpMsg, int iMsgLen, 
                            tTS *ptDest, int iFlag)
{
    double dDiff;
    unsigned char mode;
    tTS tT2_1;	/* T2 - T1 */
	tTS tT3_4;	/* T3 - T4 */
	tTS tT4_1;	/* T4 - T1 */
	tTS tT3_2;	/* T3 - T2 */
    tTS tOffset, tOrig, tRecv, tXmt;
	tTS tInterval;	/* ���̴���ʱ�� */
	tTS tNowTS;
	struct timeval tNowTV;
	struct timezone tTimeZone;


    /* ��� SNTP ��Ϣ����Ч�� */
    if (iMsgLen < sizeof(tSntpMsg))
    {
        printf("[SNTP] packet is too small %d\n", iMsgLen);
        return -1;
    }

    if ((MSG_VERSION(tSntpMsg->ucLiVnMode) < SNTP_OLD_VERSION) ||
        (MSG_VERSION(tSntpMsg->ucLiVnMode) > SNTP_VERSION)) 
    {
        printf("[SNTP] version is wrong\n");
        return -1;
    }

    mode = MSG_MODE(tSntpMsg->ucLiVnMode);
	printf("Mode: %d\n", mode);
    if (mode != MODE_SERVER && mode != MODE_BROADCAST)
    {
        printf("[SNTP] mode is not matched %d\n", mode);
        return -1;
    }

	printf("n000tSntpMsg->tOrig.intg.ui %u\n", tSntpMsg->tOrig.intg.ui);
	printf("n000tSntpMsg->tOrig.frag.uf %u\n", tSntpMsg->tOrig.frag.uf);
	
	printf("n000tSntpMsg->tRecv.intg.ui %u\n", tSntpMsg->tRecv.intg.ui);
	printf("n000tSntpMsg->tRecv.frag.uf %u\n", tSntpMsg->tRecv.frag.uf);

	printf("n000tSntpMsg->tXmt.intg.ui %u\n", tSntpMsg->tXmt.intg.ui);
	printf("n000tSntpMsg->tXmt.frag.uf %u\n\n", tSntpMsg->tXmt.frag.uf);



    NTOHL_FP(&tSntpMsg->tOrig, &tOrig);		/* �洢�ֽ��� */
    NTOHL_FP(&tSntpMsg->tRecv, &tRecv);		/* �洢�ֽ��� */
    NTOHL_FP(&tSntpMsg->tXmt, &tXmt);		/* �洢�ֽ��� */

	printf("h001tSntpMsg->tOrig.intg.ui %u\n", tOrig.intg.ui);
	printf("h001tSntpMsg->tOrig.frag.uf %u\n", tOrig.frag.uf);
	
	printf("h002tSntpMsg->tRecv.intg.ui %u\n", tRecv.intg.ui);
	printf("h002tSntpMsg->tRecv.frag.uf %u\n", tRecv.frag.uf);

	printf("h003tSntpMsg->tXmt.intg.ui %u\n", tXmt.intg.ui);
	printf("h003tSntpMsg->tXmt.frag.uf %u\n", tXmt.frag.uf);

	printf("h004ptDest.intg.ui %u\n", ptDest->intg.ui);
	printf("h004ptDest.frag.uf %u\n", ptDest->frag.uf);

	tRecv.intg.ui -= 0x83aa7e80;
	tXmt.intg.ui  -= 0x83aa7e80;

	printf("h001tSntpMsg->tOrig.intg.ui %u\n", tOrig.intg.ui);
	printf("h001tSntpMsg->tOrig.frag.uf %u\n", tOrig.frag.uf);
	
	printf("h002tSntpMsg->tRecv.intg.ui %u\n", tRecv.intg.ui);
	printf("h002tSntpMsg->tRecv.frag.uf %u\n", tRecv.frag.uf);

	printf("h003tSntpMsg->tXmt.intg.ui %u\n", tXmt.intg.ui);
	printf("h003tSntpMsg->tXmt.frag.uf %u\n", tXmt.frag.uf);

	printf("h004ptDest.intg.ui %u\n", ptDest->intg.ui);
	printf("h004ptDest.frag.uf %u\n", ptDest->frag.uf);

	if (0 == iFlag)
	{
		printf("11111111111111111111111111111\n");
		tNowTV.tv_sec  = tXmt.intg.ui;
		tNowTV.tv_usec = (tXmt.frag.uf * 1.0 / pow(2, 32)) * 1000000;

		printf("tNowTV.tv_sec  = %ld\n", tNowTV.tv_sec);
		printf("tNowTV.tv_usec = %ld\n", tNowTV.tv_usec);
	}
	else
	{
		printf("2222222222222222222222222222\n");
	    /* û�б����� Transmit Timestamp */
	    if (tOrig.fp_ui == 0 && tOrig.fp_uf == 0)
	    {
	        tOffset = tXmt;
	        TS_SUB(&tOffset, ptDest);
	    }
	    else
	    {
	        /* ����ʱ��ƫ��(��������ͻ���֮��): ((T2 - T1) + (T3 - T4)) / 2 */
	        tT3_4 = tXmt;
	        TS_SUB(&tT3_4, ptDest); /* T3 - T4 */
	        tT2_1 = tRecv;
	        TS_SUB(&tT2_1, &tOrig);
	        
	        tOffset = tT3_4;
	        TS_RSHIFT(&tOffset);
	        TS_RSHIFT(&tT2_1);
	        TS_ADD(&tOffset, &tT2_1);	/* tOffset��Ϊ�������뱾�ص�ƫ�� */
			
			printf("offset: %u\n", tOffset.frag.uf);
			TS_FPTOD(&tOffset, dDiff);
   			printf("[SNTP] offset with the server is : %d microsecond\n", 
           					(int)(dDiff * 1000000));


			/* ���㵥�̴���ʱ��: ((T4 - T1) - (T3 - T2))/2 */
			tT4_1 = *ptDest;
			TS_SUB(&tT4_1, &tOrig);
			tT3_2 = tXmt;
			TS_SUB(&tT3_2, &tRecv);
			tInterval = tT4_1; 
			TS_SUB(&tInterval, &tT3_2);
			TS_RSHIFT(&tInterval);




			/* ���ñ���ʱ�� */
			tNowTS = tXmt;
			memcpy(&tNowTS, &tXmt, sizeof(tTS));
			TS_ADD(&tNowTS, &tInterval);
			tNowTV.tv_sec  = tNowTS.intg.ui;
			tNowTV.tv_usec = (tNowTS.frag.uf * 1.0 / pow(2, 32)) * 1000000;
		}
	}

	tTimeZone.tz_dsttime = 0;
	tTimeZone.tz_minuteswest = -480;
	if(-1 == settimeofday(&tNowTV, &tTimeZone))
	{
		perror("settimeofday()");
		return -1;
	}
	printf("tTimeZone:%d:%d\n", tTimeZone.tz_dsttime, tTimeZone.tz_minuteswest);
	

	system("date");
//	system("hwclock -w");

	/* �����������ͻ��˵�ʱ��ƫ�� */
	TS_SUB(&tRecv, &tOrig);
	TS_SUB(&tRecv, &tInterval);
	printf("pian cha : %d\n", tRecv.intg.ui);
	printf("pian cha : %d\n", tRecv.frag.uf);


	printf("\n===============================================================\n");
	printf("===============================================================\n\n");

    return 0;
}

/**************************************************************************
  *
  *
  *
  *
  *
  *
  *************************************************************************/
static  unsigned char Ipv4OrIpv6(const char * pcAddr)
{
    do
    {	
        if(*pcAddr == '.')
        	return SNTP_V4;	// ipv4
        if(*pcAddr == ':')
            return SNTP_V6;	// ipv6
    }while(*(pcAddr++) != '\0');
	
    return SNTP_ERR;	// error
}

/**************************************************************************
  *
  *
  *
  *
  *
  *
  *************************************************************************/
int main(int argc, char *argv[])
{
	SntpClient(argv[1]);
	return 0;
}

int SntpClient(char *pcSrvAddr)
{
	int iRes;
	int iRet;
	int iCnt = 0; 	/* �����������ͬ������һ��ֱ��д
					������ʱ�䣬�ڶ������Ƚ���д��ʱ�� */
	int iAddrLen;
	unsigned char   cIpType;
	int   bIsNeedSendNtpReq = 0;
	fd_set tReadFds;
	int s = -1;	/* �ͻ���socket��� */
	struct ip_mreq mcast_addr; /* �ಥ��ַ */
	struct sockaddr_in tLocalAddr;
	struct sockaddr_in tDestAddr;
	struct sntp_s tSntpMsg;

	tTS  tNow;
	tTS  tOrig;

	/* ���ip��ַ���� */
	if (NULL == pcSrvAddr)
	{
		printf("[SNTP] err: server ip is empty!\n");
		return SNTP_ERROR_INVALID_PARAMETER;
	}
	else 
	{
		printf("[SNTP] server ip:%s\n", pcSrvAddr);
	}

	cIpType = Ipv4OrIpv6(pcSrvAddr);

	/* ����SOCKET */
//	if (SNTP_V4 == cIpType)
//	{	
		s = socket(AF_INET, SOCK_DGRAM, 0);
		
		if (-1 == s)
		{
			printf("[SNTP] err: socket().\n");
			return SNTP_ERROR_CREATESOCKET;
		}    
		if (0 >= inet_pton(AF_INET, pcSrvAddr, 
						  (void *)&tDestAddr.sin_addr.s_addr))	
		{
			bIsNeedSendNtpReq = -1;
		}
//	}
//	else
//	{
//	    bIsNeedSendNtpReq = -1;
//	}	/* end of ����SOCKET */



	 /* ���δ�ṩ�Ϸ��ķ�������ַ��������SNTP���󣬲��õȴ����չ㲥���� */
	if(0 == bIsNeedSendNtpReq)
	{	
		/* �󶨱��ص�ַ��SOCKET */
		if(SNTP_V4 == cIpType)
		{
			iAddrLen = sizeof(tLocalAddr);
			
			memset(&tLocalAddr, 0,sizeof(tLocalAddr));
			tLocalAddr.sin_family      = AF_INET;
			tLocalAddr.sin_addr.s_addr = INADDR_ANY;
			tLocalAddr.sin_port        = htons((unsigned short)5123);

			tDestAddr.sin_family       = AF_INET;
			tDestAddr.sin_port         = htons((unsigned short)(123));
			memset(tDestAddr.sin_zero, 0, sizeof(tDestAddr.sin_zero));
			printf("destAddr:%d\n", tDestAddr.sin_addr.s_addr);
		}
		else
		{
			printf("[SNTP] ERR: IP ERR! OR Ipv6 is not Supported.\n");
		}
	}
	else  /* �󶨶ಥ��ַ��SOCKET */
	{
		printf(" bind MultiAddr to socket\n");
	
		/* �ಥ��ַ */
	    mcast_addr.imr_multiaddr.s_addr = inet_addr(SNTP_MCAST_ADDR);
	    mcast_addr.imr_interface.s_addr = INADDR_ANY;

	    /* ����ಥ�� */
	    iRes = setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP,
	                        (char *)&mcast_addr, sizeof(mcast_addr));
	}

	while (iCnt < 2)
	{
		/* ��ʼ��SNTP�����ֶ� */
		memset(&tSntpMsg, 0, sizeof(tSntpMsg));
		SntpGetSysTime(&tSntpMsg.tXmt);
//		memcpy(&tOrig, &tSntpMsg.tOrig, sizeof(&tOrig));
		printf("h tXmt.intg=%u\n", tSntpMsg.tXmt.intg.ui);
		printf("h tXmt.frag=%u\n", tSntpMsg.tXmt.frag.uf);
	    HTONL_FP(&tSntpMsg.tXmt, &tSntpMsg.tXmt);
		printf("n tXmt.intg=%u\n", tSntpMsg.tXmt.intg.ui);
		printf("n tXmt.frag=%u\n", tSntpMsg.tXmt.frag.uf);
	    tSntpMsg.ucLiVnMode = MSG_LI_VN_MODE(LEAP_NOT_SYNC, 
	                            SNTP_VERSION, MODE_CLIENT);


		/* ����SNTP�����ֶ� */
		if (SNTP_V4 == cIpType)
		{
			iRes = sendto(s, (char *)&tSntpMsg, sizeof(tSntpMsg), 
						  0, (const struct sockaddr *)&tDestAddr, iAddrLen);
			if (-1 == iRes)
			{
				perror("[SNTP] ERROR--sendto()");
				return SNTP_ERROR_SENDTO;
			}
			printf("n tOrig.intg=%u\n", tSntpMsg.tXmt.intg.ui);
			printf("n tOrig.frag=%u\n", tSntpMsg.tXmt.frag.uf);
			int  mode = MSG_MODE(tSntpMsg.ucLiVnMode);
			printf("mode:%d\n", mode);
			int version = MSG_VERSION(tSntpMsg.ucLiVnMode);
			printf("version:%d\n", version);
		}
	
	    if (iRes == -1)
	    {
			perror("[SNTP] -- setsockopt(IP_ADD_MEMBERSHIP)");
	        close(s);
	        return SNTP_ERROR_BINDSOCKET;
	    }

		printf("[SNTP] is sychronizing ... ...\n");

		/* �ȴ������¼� */
		 while (1)
	    {
			printf("tSntpMsg.tOrig.intg.ui = %u\n", tSntpMsg.tXmt.intg.ui);
			printf("tSntpMsg.tOrig.frag.uf = %u\n", tSntpMsg.tXmt.frag.uf);
			
	        memset(&tSntpMsg, 0, sizeof(tSntpMsg));
			
	        iRes = recvfrom(s, (char *)&tSntpMsg, sizeof(tSntpMsg),
	                          0, (struct sockaddr *)&tDestAddr, &iAddrLen);
	        if (iRes == -1)
	        {
	            printf("[SNTP] ERROR: recvfrom(). \n");
	            break;
	        }
			
	        SntpGetSysTime(&tNow);	/* �洢�ֽ��� */

	        if (0 == SntpRecvHandle(&tSntpMsg, iRes, &tNow, 1))
				break;
			else
				continue;
		 }/* end of while (1)*/
		 
		iCnt++;
		 
	} /* end of while (iCnt < 2)*/
	
	/* �뿪�ಥ�� */
    setsockopt(s, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                  (char *)&mcast_addr, sizeof(mcast_addr));

    close(s);
    return 0;
}


