/***********************************************************
 * ��Ȩ���� (C), �Ϻ������Ϣ�������޹�˾��
 *
 * �ļ����ƣ� sntp_client.c
 * �ļ���ʶ��
 * ����ժҪ�� SNTP�ͻ��˳��򣬻��SNTP��������ʱ��
 * ����˵���� ��
 * ��    �ߣ� 
 * ������ڣ�
 *
 * �޸ļ�¼1��
 *    �޸����ڣ�
 *    �� �� �ţ�V1.0
 *    �� ��    ��
 *    �޸����ݣ�����
 **********************************************************/

 
/******************��׼�⡢�Ǳ�׼�� **********************/
#include "Socket.h"
#include "Sntp_client.h"


/********************* 全局变量 ***************************/



/******************* 本地全局变量 *************************/
#if SNTP_IPV6_SUPPORT
static struct in6_addr g_ipv6BroadCastAddr = {{{0xff, 0xff, 0xff, 0xff, \
                                                0x00, 0x00, 0x00, 0x00, \
                                                0x00, 0x00, 0x00, 0x00, \
									            0x00, 0x00, 0x00, 0x00}}};
static struct in6_addr g_SntpIn6AnyAddr    = {{{0x00, 0x00, 0x00, 0x00, \
                                                0x00, 0x00, 0x00, 0x00, \
                                                0x00, 0x00, 0x00, 0x00, \
												0x00, 0x00, 0x00, 0x00 }}};
#endif

 

/*****************************************************/
/*inline function ,do not do any check*/
static  char Ipv4OrIpv6(const char * pcAddr)
{
    do
    {
        if(*pcAddr == '.')
            return SNTP_V4;	// ipv4
        if(*pcAddr == ':')
            return SNTP_V6;	// ipv6
    }while(*pcAddr++ != '\0');

    return SNTP_ERR;	// error
}


SNTP_STATUS SntpClient(char *pcServerAddr, u_int dwTimeout, TTimeSpec *ptNow)
{
	SNTP_SockAddr  tLocalAddr;
    SNTP_SockAddr  tServerAddr;
	
	WORD32 dwInterval;
    SOCKET iSocketID;
	BOOL   bIsNeedSendNtpReq = TURE;
	BYTE   bIpTpye = SNTP_ERR;
	char   cRetStatus;
	int    iResult;
	fd_set tReadFds;
	TNTP_Packet    tNtpMsg;
	struct timeval tWaitTime;

	if(NULL == pcServerAddr)
    {
        return SNTP_ERROR_INVALID_PARAMETER;
    }
	
	bIpTpye = Ipv4OrIpv6(pcServerAddr);


	/* ����SOCKET */
	if (SNTP_V4 == bIpTpye)
	{
		iSocketID = socket(AF_INET, SOCK_DGRAM, 0);
		if (INVALID_SOCKET == iSocketID)
		{
			printf("Failed To Create Client Socket For Ipv4!\r\n");
			cRetStatus = SNTP_ERROR_CREATESOCKET;
			goto LEAVE;
		}    
		if (0 >= inet_pton(AF_INET, \
						  pcServerAddr, \
						  (VOID *)&(tServerAddr.sin_addr.s_addr)))	//QUE
		{
			bIsNeedSendNtpReq = FALSE;
		}
	}
	#ifdef SNTP_IPV6_SUPPORT
	else if (SNTP_V6 == bIpTpye)
	{
		iSocketID = socket(PF_INET6, SOCK_DGRAM, 0);
		if (iSocketID == INVALID_SOCKET)
		{
			printf("Failed To Create Client Socket For Ipv6!\r\n");
			cRetStatus = SNTP_ERROR_CREATESOCKET;
			goto LEAVE;
		}
		if (0 >= inet_pton(AF_INET6, \
						  pcServerAddr, \
						  (VOID *)&(tServerAddr6.sin6_addr.s6_addr)))
		{
			bIsNeedSendNtpReq = FALSE;
		}
	}
	#endif
	else
	{
	    bIsNeedSendNtpReq = FALSE;
	}/* end of ����SOCKET */

	
	 /* ���δ�ṩ�Ϸ��ķ�������ַ��������SNTP���󣬲��õȴ����չ㲥���� */
	if(bIsNeedSendNtpReq)
	{
		/* �󶨱��ص�ַ��SOCKET */
		if(SNTP_V4 == bIpTpye)
		{
			memset(&tLocalAddr, 0,sizeof(tLocalAddr));
			tLocalAddr.sin_family      = AF_INET;
			tLocalAddr.sin_addr.s_addr = INADDR_ANY;
			tLocalAddr.sin_port        = htons((unsigned short)123);	//QUE
			
			memset(&tServerAddr, 0, sizeof(tServerAddr));
			tServerAddr.sin_family      = AF_INET;
			tServerAddr.sin_port        = htons((unsigned short)123);
		}
		#ifdef SNTP_IPV6_SUPPORT
		else if(SNTP_V6 == bIpTpye)
		{
			memset(&tLocalAddr6, 0,sizeof(tLocalAddr6));
			tLocalAddr6.sin6_family      = AF_INET6;
			tLocalAddr6.sin6_port        = htons((unsigned short)123);	//QUE
			tLocalAddr6.sin6_flowinfo    = 0;
			tLocalAddr6.sin6_addr        = g_SntpIn6AnyAddr;
				
			memset(&tServerAddr6, 0, sizeof(tServerAddr6));
			tServerAddr6.sin6_family      = AF_INET6;
			tServerAddr6.sin6_port        = htons((unsigned short)123);
		}
		#endif
		else
		{
			printf("SNTP:fatal ERR! OR Ipv6 is not Supported\r\n");
		}
		
		if(SOCKET_ERROR == bind(iSocketID, \
							    (struct sockaddr *)&tLocalAddr, \
							    sizeof(tLocalAddr)))
		{
			printf("SNTP:bind address err!");
			cRetStatus = SNTP_ERROR_BINDSOCKET;
			goto LEAVE;
		}
		
		/* ��ʼ��SNTP�����ֶ� */
		memset(&tNtpMsg, 0, sizeof(tNtpMsg));
		tNtpMsg.Control_Word  = htonl(0x1B000000);	//QUE
		
		
		/* ����SNTP�����ֶ� */
		if(-1 == sendto(iSocketId, (char *)&tNtpMsg, 0, \
			           (char *)&tServerAddr, sizeof(tServerAddr)));
		{
			printf("Failed To send SNTP Massage!\r\n");
			cRetStatus = SNTP_ERROR_SENDTO;
			goto LEAVE;
		}
	}
	else
	{
	   /* �󶨱��ع㲥��ַ��SOCKET */ 
	   #ifdef SNTP_IPV6_SUPPORT
	   tLocalAddr6.sin6_family    = AF_INET6;
	   tLocalAddr6.sin6_addr      = g_ipv6BroadCastAddr;
	   tLocalAddr6.sin6_flowinfo  = 0;
	   tLocalAddr6.sin6_port      = htons((unsigned short)123);
	   #else
	   tLocalAddr.sin_family      = AF_INET;
	   tLocalAddr.sin_addr.s_addr = INADDR_BROADCAST;
	   tLocalAddr.sin_port        = htons((unsigned short)123);
	   #endif
	   
	   if(SOCKET_ERROR == bind(iSocketID, \
	   					       (struct sockaddr *)&tLocalAddr, \
	   					       sizeof(tLocalAddr))
	   {
	       printf("Failed to Bind Socket!\r\n");
		   cRetStatus = SNTP_ERROR_BINDSOCKET;
		   goto LEAVE;
	   }
	}
	
    /* �ȴ������¼� */
	tWaitTime.tv_sec  = dwTimeout/1000;				/*  ת��Ϊ��  */
    tWaitTime.tv_usec = (dwTimeout%1000)*1000;		/*  ת��Ϊ΢�� */
    FD_ZERO(&tReadFds);
    FD_SET(((WORD32)(iSocketID)), &tReadFds);

	iResult = select(FD_SETSIZE, &tReadFds, NULL, NULL, &tWaitTime)
	if (SOCKET_ERROR == iResult)
	{
		printf("Select ERROR!\r\n");
		cRetStatus = SNTP_ERROR_SELECT;
		goto LEAVE;
	}
	if (0 == iResult)
	{
		printf("Select ERROR:Timeout!\r\n");
		cRetStatus = SNTP_ERROR_TIMEOUT;
		goto LEAVE;
	}
	if (!FD_ISSET(((WORD32)(iSocketId)), &tReadFds))
	{
		printf("FD_ISSET ERROR!\r\n");
		cRetStatus = SNTP_ERROR_FDISSET;
		goto LEAVE;
	}
	
	/* �ӷ��������ܻ��͵ı��� */
	WORD32 dwServerAddrLen = 0;
	dwServerAddrLen = sizeof(tServerAddr);
	memset(&tNtpMsg, 0, sizeof(tNtpMsg));
	if (sizeof(tNtpMsg) != recvfrom(iSocketID, \
									(char *)&tNtpMsg, \
									sizeof(tNtpMsg), 0, \
									(struct sockaddr*)&tServerAddr, \
									(INT*)&dwServAddrLen))	//QUE
    {
        printf("SNTP:recvfrom err!�\n");
		cRetStatus = SNTP_ERROR_RECVFROM;
        goto LEAVE;
    }

    closesocket(iSocketID);
	
	 /* ��ֹ��sntp��ʱ��������ַ����Ϊ�ػ���ַ���¶�ʱ���� */
    if(0 == tNtpMsg.m_TransTS.m_dwInteger)
    {
    	cRetStatus = SNTP_ERROR_INVALID_ADDRESS
        goto LEAVE;
    }

    ptNow->dwSec     = ntohl(tNtpMsg.m_TransTS.m_dwInteger);
    ptNow->dwNanoSec = (WORD32)((WORD32)ntohl(tNtpMsg.m_TransTS.m_dwFractionl) \
							   * (SWORD64)1000000000/0xffffffff);

    /*������1900.01.01 00:00:00~1970.01.01 00:00:00����������*/
    dwInterval = ((WORD32)(2000-1900)*365+25-1)*24*3600;
    ptNow->dwSec -= dwInterval;
    /*�����ע�ͺ���Ҫ���벻Ҫɾ��*/
    /*������������⣬��һ����������������Ч
    SNTP��������һ�������ʱ������2036�꣬��������
    ��׼ʱ���ᵽ��2000�꣬���Ա��������������ʱ����2136�ꡣ
    if (ptNow->dwSec < dwInterval)
    {
        ptNow->dwSec += 0xffffffff - dwInterval + 1;
    }
    else
    {
        ptNow->dwSec -= dwInterval;
    }*/
    return SNTP_SUCCESS;
	

LEAVE:

	return cRetStatus;
    
}